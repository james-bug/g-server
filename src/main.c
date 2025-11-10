/**
 * @file main.c
 * @brief gaming-server Main Daemon
 * 
 * 整合所有模組提供完整的伺服器功能:
 * - CEC Monitor: 監控 PS5 電源狀態
 * - PS5 Detector: 偵測 PS5 位置
 * - PS5 Wake: 喚醒 PS5
 * - WebSocket Server: 提供查詢服務
 * - State Machine: 協調各模組
 * 
 * @author Gaming System Development Team
 * @date 2025-11-06
 * @version 1.0.0
 */

// POSIX headers
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <cjson/cJSON.h>

// gaming-server modules
#include "cec_monitor.h"
#include "ps5_detector.h"
#include "ps5_wake.h"
#include "websocket_server.h"
#include "server_state_machine.h"

/* ============================================================
 *  Constants and Macros
 * ============================================================ */

#define PROGRAM_NAME        "gaming-server"
#define PROGRAM_VERSION     "1.0.0"

#define DEFAULT_WS_PORT     8080
#define DEFAULT_CEC_DEVICE  "/dev/cec0"
#define DEFAULT_SUBNET      "192.168.1.0/24"
#define DEFAULT_CACHE_PATH  "/var/run/gaming/ps5_cache.json"

#define MAIN_LOOP_INTERVAL_MS   100

/* ============================================================
 *  Global Variables
 * ============================================================ */

// 運行標誌
static volatile bool g_running = true;

// 伺服器上下文
static server_context_t g_server_ctx;

// 配置
static struct {
    int ws_port;
    char cec_device[64];
    char subnet[32];
    char cache_path[256];
    bool use_mock;
} g_config = {
    .ws_port = DEFAULT_WS_PORT,
    .cec_device = DEFAULT_CEC_DEVICE,
    .subnet = DEFAULT_SUBNET,
    .cache_path = DEFAULT_CACHE_PATH,
    .use_mock = false
};

/* ============================================================
 *  Signal Handling
 * ============================================================ */

/**
 * @brief Signal 處理函數
 */
static void signal_handler(int signum) {
    switch (signum) {
        case SIGTERM:
        case SIGINT:
            fprintf(stdout, "\n[Server] Received signal %d, shutting down...\n", signum);
            g_running = false;
            break;
            
        case SIGUSR1:
            fprintf(stdout, "[Server] Received SIGUSR1, reloading config...\n");
            // TODO: 重新載入配置
            break;
            
        default:
            break;
    }
}

/**
 * @brief 設定 Signal 處理
 */
static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    
    // 忽略 SIGPIPE
    signal(SIGPIPE, SIG_IGN);
}

/* ============================================================
 *  Callback Functions
 * ============================================================ */

/**
 * @brief CEC 事件回調
 */
static void on_cec_event(cec_event_t event, ps5_power_state_t state, void *user_data) {
    server_context_t *ctx = (server_context_t*)user_data;
    
    fprintf(stdout, "[CEC] Event: %d, Power State: %d\n", event, state);
    
    // 更新狀態機
    server_sm_update_cec_state(ctx, state);
}

/**
 * @brief WebSocket 連線回調
 */
static void on_ws_connect(int client_id, const char *client_ip, void *user_data) {
    (void)user_data;
    fprintf(stdout, "[WebSocket] Client %d connected from %s\n", client_id, client_ip);
}

/**
 * @brief WebSocket 斷線回調
 */
static void on_ws_disconnect(int client_id, void *user_data) {
    (void)user_data;
    fprintf(stdout, "[WebSocket] Client %d disconnected\n", client_id);
}

/**
 * @brief WebSocket 訊息處理回調
 */
static char* on_ws_message(int client_id, ws_message_type_t msg_type,
                           const char *payload, void *user_data) {
    server_context_t *ctx = (server_context_t*)user_data;
    char *response = NULL;
    
    fprintf(stdout, "[WebSocket] Client %d, Message Type: %s\n", 
            client_id, ws_message_type_to_string(msg_type));
    
    switch (msg_type) {
        case WS_MSG_QUERY_PS5: {
            // 處理 PS5 狀態查詢
            server_sm_handle_event(ctx, SERVER_EVENT_CLIENT_QUERY);
            
            const char *status = server_sm_get_ps5_status(ctx);
            
            // 建立回應
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "ps5_status");
            cJSON_AddStringToObject(root, "status", status);
            cJSON_AddStringToObject(root, "ip", ctx->ps5_status.info.ip);
            cJSON_AddStringToObject(root, "mac", ctx->ps5_status.info.mac);
            cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
            
            response = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            
            server_sm_handle_event(ctx, SERVER_EVENT_COMPLETED);
            break;
        }
        
        case WS_MSG_WAKE_PS5: {
            // 處理 PS5 喚醒請求
            server_sm_handle_event(ctx, SERVER_EVENT_WAKE_REQUEST);
            
            fprintf(stdout, "[Server] Waking PS5...\n");
            
            // 執行喚醒
            ps5_info_t *ps5 = &ctx->ps5_status.info;
            wake_result_t wake_result = ps5_wake(ps5, 30);
            
            // 建立回應
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "wake_response");
            
            if (wake_result == WAKE_RESULT_SUCCESS) {
                cJSON_AddStringToObject(root, "status", "success");
                cJSON_AddStringToObject(root, "message", "PS5 woke up successfully");
                server_sm_handle_event(ctx, SERVER_EVENT_COMPLETED);
            } else {
                cJSON_AddStringToObject(root, "status", "failed");
                cJSON_AddStringToObject(root, "message", ps5_wake_result_string(wake_result));
                server_sm_handle_event(ctx, SERVER_EVENT_ERROR);
            }
            
            response = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            break;
        }
        
        case WS_MSG_PING: {
            // 回應 Pong
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "pong");
            cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
            
            response = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            break;
        }
        
        default:
            fprintf(stderr, "[WebSocket] Unknown message type: %d\n", msg_type);
            break;
    }
    
    return response;
}

/* ============================================================
 *  Initialization and Cleanup
 * ============================================================ */

/**
 * @brief 初始化所有模組
 */
static int initialize_modules(void) {
    fprintf(stdout, "[Server] Initializing modules...\n");
    
    // 1. 初始化 CEC Monitor
    fprintf(stdout, "[Server] Initializing CEC Monitor (%s)...\n", g_config.cec_device);
    if (cec_monitor_init(g_config.cec_device) != 0) {
        fprintf(stderr, "[Server] Failed to initialize CEC Monitor\n");
        // 非關鍵錯誤,繼續
    } else {
        cec_monitor_set_callback(on_cec_event, &g_server_ctx);
    }
    
    // 2. 初始化 PS5 Detector
    fprintf(stdout, "[Server] Initializing PS5 Detector (%s)...\n", g_config.subnet);
    if (ps5_detector_init(g_config.subnet, g_config.cache_path) != 0) {
        fprintf(stderr, "[Server] Failed to initialize PS5 Detector\n");
        return -1;
    }
    
    // 3. 初始化 PS5 Wake
    fprintf(stdout, "[Server] Initializing PS5 Wake...\n");
    if (ps5_wake_init(g_config.cec_device) != 0) {
        fprintf(stderr, "[Server] Failed to initialize PS5 Wake\n");
        // 非關鍵錯誤,繼續
    }
    
    // 4. 初始化 WebSocket Server
    fprintf(stdout, "[Server] Initializing WebSocket Server (port %d)...\n", g_config.ws_port);
    if (ws_server_init(g_config.ws_port) != 0) {
        fprintf(stderr, "[Server] Failed to initialize WebSocket Server\n");
        return -1;
    }
    
    ws_server_set_message_handler(on_ws_message, &g_server_ctx);
    ws_server_set_connect_callback(on_ws_connect, &g_server_ctx);
    ws_server_set_disconnect_callback(on_ws_disconnect, &g_server_ctx);
    
    // 5. 初始化 State Machine
    fprintf(stdout, "[Server] Initializing State Machine...\n");
    if (server_sm_init(&g_server_ctx) != 0) {
        fprintf(stderr, "[Server] Failed to initialize State Machine\n");
        return -1;
    }
    
    fprintf(stdout, "[Server] All modules initialized successfully\n");
    return 0;
}

/**
 * @brief 啟動所有服務
 */
static int start_services(void) {
    fprintf(stdout, "[Server] Starting services...\n");
    
    // 啟動 WebSocket Server
    if (ws_server_start() != 0) {
        fprintf(stderr, "[Server] Failed to start WebSocket Server\n");
        return -1;
    }
    
    fprintf(stdout, "[Server] All services started successfully\n");
    return 0;
}

/**
 * @brief 清理所有模組
 */
static void cleanup_modules(void) {
    fprintf(stdout, "[Server] Cleaning up modules...\n");
    
    ws_server_stop();
    ws_server_cleanup();
    
    cec_monitor_stop();
    cec_monitor_cleanup();
    
    ps5_detector_cleanup();
    ps5_wake_cleanup();
    
    server_sm_stop(&g_server_ctx);
    server_sm_cleanup(&g_server_ctx);
    
    fprintf(stdout, "[Server] Cleanup completed\n");
}

/* ============================================================
 *  Main Loop
 * ============================================================ */

/**
 * @brief 處理狀態機狀態
 */
static void process_state_machine(void) {
    server_state_t state = server_sm_get_state(&g_server_ctx);
    
    switch (state) {
        case SERVER_STATE_DETECTING: {
            // 執行 PS5 偵測
            ps5_info_t info;
            if (ps5_detector_scan(&info) == 0) {
                server_sm_update_ps5_info(&g_server_ctx, &info);
                server_sm_handle_event(&g_server_ctx, SERVER_EVENT_COMPLETED);
                
                fprintf(stdout, "[Server] PS5 detected: %s (%s)\n", info.ip, info.mac);
            } else {
                fprintf(stderr, "[Server] PS5 detection failed\n");
                server_sm_handle_event(&g_server_ctx, SERVER_EVENT_ERROR);
            }
            break;
        }
        
        case SERVER_STATE_BROADCASTING: {
            // 廣播狀態更新給所有客戶端
            const char *status = server_sm_get_ps5_status(&g_server_ctx);
            
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "ps5_status_update");
            cJSON_AddStringToObject(root, "status", status);
            cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
            
            char *json = cJSON_PrintUnformatted(root);
            ws_server_broadcast(json);
            cJSON_free(json);
            cJSON_Delete(root);
            
            server_sm_handle_event(&g_server_ctx, SERVER_EVENT_COMPLETED);
            break;
        }
        
        case SERVER_STATE_ERROR: {
            // 錯誤狀態 - 嘗試恢復
            fprintf(stderr, "[Server] In error state, attempting recovery...\n");
            server_sm_handle_event(&g_server_ctx, SERVER_EVENT_NONE);
            break;
        }
        
        default:
            break;
    }
}

/**
 * @brief 主事件循環
 */
static void main_loop(void) {
    fprintf(stdout, "[Server] Entering main loop...\n");
    
    struct timespec sleep_time = {
        .tv_sec = 0,
        .tv_nsec = MAIN_LOOP_INTERVAL_MS * 1000000  // ms to ns
    };
    
    while (g_running) {
        // 更新狀態機
        server_sm_update(&g_server_ctx);
        
        // 處理 CEC 事件
        cec_monitor_process(50);
        
        // 處理 WebSocket 事件
        ws_server_service(50);
        
        // 處理狀態機狀態
        process_state_machine();
        
        // 短暫休息
        nanosleep(&sleep_time, NULL);
    }
    
    fprintf(stdout, "[Server] Main loop exited\n");
}

/* ============================================================
 *  Configuration and CLI
 * ============================================================ */

/**
 * @brief 顯示使用說明
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  -p, --port PORT       WebSocket port (default: %d)\n", DEFAULT_WS_PORT);
    printf("  -c, --cec DEVICE      CEC device (default: %s)\n", DEFAULT_CEC_DEVICE);
    printf("  -s, --subnet SUBNET   Network subnet (default: %s)\n", DEFAULT_SUBNET);
    printf("  -m, --mock            Use mock mode for testing\n");
    printf("  -h, --help            Show this help message\n");
    printf("  -v, --version         Show version information\n");
    printf("\n");
}

/**
 * @brief 顯示版本資訊
 */
static void print_version(void) {
    printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("Gaming System Server Daemon\n");
}

/**
 * @brief 解析命令列參數
 */
static int parse_arguments(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"port",    required_argument, 0, 'p'},
        {"cec",     required_argument, 0, 'c'},
        {"subnet",  required_argument, 0, 's'},
        {"mock",    no_argument,       0, 'm'},
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "p:c:s:mhv", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'p':
                g_config.ws_port = atoi(optarg);
                break;
                
            case 'c':
                snprintf(g_config.cec_device, sizeof(g_config.cec_device), "%s", optarg);
                break;
                
            case 's':
                snprintf(g_config.subnet, sizeof(g_config.subnet), "%s", optarg);
                break;
                
            case 'm':
                g_config.use_mock = true;
                break;
                
            case 'h':
                print_usage(argv[0]);
                return 1;
                
            case 'v':
                print_version();
                return 1;
                
            default:
                print_usage(argv[0]);
                return -1;
        }
    }
    
    return 0;
}

/* ============================================================
 *  Main Entry Point
 * ============================================================ */

/**
 * @brief 主函數
 */
int main(int argc, char *argv[]) {
    int ret = EXIT_SUCCESS;
    
    // 顯示啟動訊息
    printf("===========================================\n");
    printf("  %s v%s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("  Gaming System Server Daemon\n");
    printf("===========================================\n\n");
    
    // 解析命令列參數
    int parse_result = parse_arguments(argc, argv);
    if (parse_result != 0) {
        return (parse_result > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    // 設定 Signal 處理
    setup_signal_handlers();
    
    // 初始化模組
    if (initialize_modules() != 0) {
        fprintf(stderr, "[Server] Initialization failed\n");
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    // 啟動服務
    if (start_services() != 0) {
        fprintf(stderr, "[Server] Failed to start services\n");
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    fprintf(stdout, "[Server] %s is running (PID: %d)\n", PROGRAM_NAME, getpid());
    fprintf(stdout, "[Server] WebSocket: ws://0.0.0.0:%d\n", g_config.ws_port);
    fprintf(stdout, "[Server] Press Ctrl+C to stop\n\n");
    
    // 進入主循環
    main_loop();
    
cleanup:
    // 清理資源
    cleanup_modules();
    
    fprintf(stdout, "[Server] %s stopped\n", PROGRAM_NAME);
    return ret;
}
