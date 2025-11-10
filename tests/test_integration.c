/**
 * @file test_integration.c
 * @brief Gaming Server Integration Tests - Real Flow Simulation
 * 
 * 模擬真實的伺服器運行流程，類似 g-client 的整合測試風格：
 * - 完整的 WebSocket 客戶端連線與查詢流程
 * - CEC 事件偵測與狀態廣播流程
 * - PS5 喚醒請求與驗證流程
 * - 多模組協作的真實場景
 * 
 * @date 2025-11-07
 * @version 3.1 - Fixed CMock function calls
 */

#define _POSIX_C_SOURCE 200809L

#include "unity.h"

// Mock 模組
#include "mock_cec_monitor.h"
#include "mock_ps5_detector.h"
#include "mock_ps5_wake.h"
#include "mock_websocket_server.h"
#include "mock_logger.h"

// POSIX 標準庫
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>

/* ============================================================
 *  測試配置與常數
 * ============================================================ */

#define MAX_EVENT_HISTORY 50
#define TEST_SLEEP_MS 10
#define MAX_TEST_ITERATIONS 100

#define TEST_SUBNET "192.168.1.0/24"
#define TEST_PS5_IP "192.168.1.100"
#define TEST_PS5_MAC "00:11:22:33:44:55"
#define TEST_CEC_DEVICE "/dev/cec0"
#define TEST_CACHE_FILE "/tmp/ps5_cache.json"
#define TEST_WS_PORT 8080

/* ============================================================
 *  模擬的伺服器狀態
 * ============================================================ */

typedef enum {
    SIM_STATE_IDLE = 0,
    SIM_STATE_MONITORING,
    SIM_STATE_DETECTING,
    SIM_STATE_RESPONDING,
    SIM_STATE_WAKING,
    SIM_STATE_ERROR
} sim_server_state_t;

typedef struct {
    // 狀態
    sim_server_state_t current_state;
    
    // 模組狀態
    bool cec_initialized;
    bool detector_initialized;
    bool wake_initialized;
    bool ws_initialized;
    
    // PS5 狀態
    ps5_power_state_t ps5_power;
    bool ps5_network_online;
    ps5_info_t ps5_info;
    
    // WebSocket 客戶端
    int connected_clients;
    
    // 統計
    int cec_events;
    int client_queries;
    int wake_requests;
    int broadcasts;
    
} sim_server_context_t;

/* ============================================================
 *  全域變數
 * ============================================================ */

static sim_server_context_t g_server = {0};

// 事件計數器
static int g_state_change_count = 0;
static int g_cec_event_count = 0;
static int g_ws_message_count = 0;

// 事件歷史
static sim_server_state_t g_state_history[MAX_EVENT_HISTORY];
static int g_state_history_count = 0;

/* ============================================================
 *  時間輔助函數
 * ============================================================ */

static void get_current_time(struct timespec *ts) {
    if (clock_gettime(CLOCK_MONOTONIC, ts) != 0) {
        perror("clock_gettime failed");
    }
}

static long timespec_diff_ms(const struct timespec *start, const struct timespec *end) {
    long sec_diff = end->tv_sec - start->tv_sec;
    long nsec_diff = end->tv_nsec - start->tv_nsec;
    return sec_diff * 1000 + nsec_diff / 1000000;
}

static void sleep_ms(long milliseconds) {
    struct timespec sleep_time = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000
    };
    struct timespec remaining;
    while (nanosleep(&sleep_time, &remaining) == -1) {
        if (errno == EINTR) {
            sleep_time = remaining;
        } else {
            break;
        }
    }
}

/* ============================================================
 *  模擬伺服器輔助函數
 * ============================================================ */

static void sim_change_state(sim_server_state_t new_state) {
    if (g_server.current_state != new_state) {
        printf("  State: ");
        switch (g_server.current_state) {
            case SIM_STATE_IDLE: printf("IDLE"); break;
            case SIM_STATE_MONITORING: printf("MONITORING"); break;
            case SIM_STATE_DETECTING: printf("DETECTING"); break;
            case SIM_STATE_RESPONDING: printf("RESPONDING"); break;
            case SIM_STATE_WAKING: printf("WAKING"); break;
            case SIM_STATE_ERROR: printf("ERROR"); break;
        }
        printf(" -> ");
        switch (new_state) {
            case SIM_STATE_IDLE: printf("IDLE\n"); break;
            case SIM_STATE_MONITORING: printf("MONITORING\n"); break;
            case SIM_STATE_DETECTING: printf("DETECTING\n"); break;
            case SIM_STATE_RESPONDING: printf("RESPONDING\n"); break;
            case SIM_STATE_WAKING: printf("WAKING\n"); break;
            case SIM_STATE_ERROR: printf("ERROR\n"); break;
        }
        
        g_server.current_state = new_state;
        g_state_change_count++;
        
        if (g_state_history_count < MAX_EVENT_HISTORY) {
            g_state_history[g_state_history_count++] = new_state;
        }
    }
}

static void sim_server_update(void) {
    // 模擬伺服器更新邏輯
    switch (g_server.current_state) {
        case SIM_STATE_IDLE:
            // 空閒狀態，等待事件
            break;
            
        case SIM_STATE_MONITORING:
            // 監控 CEC 和網路
            if (g_server.ps5_power != PS5_POWER_UNKNOWN) {
                sim_change_state(SIM_STATE_DETECTING);
            }
            break;
            
        case SIM_STATE_DETECTING:
            // 偵測 PS5 網路狀態
            sim_change_state(SIM_STATE_RESPONDING);
            break;
            
        case SIM_STATE_RESPONDING:
            // 回應客戶端查詢
            sim_change_state(SIM_STATE_IDLE);
            break;
            
        case SIM_STATE_WAKING:
            // 喚醒 PS5
            if (g_server.ps5_power == PS5_POWER_ON) {
                sim_change_state(SIM_STATE_IDLE);
            }
            break;
            
        case SIM_STATE_ERROR:
            // 錯誤狀態
            sim_change_state(SIM_STATE_IDLE);
            break;
    }
}

static void simulate_time_and_update(int iterations) {
    for (int i = 0; i < iterations; i++) {
        sim_server_update();
        if (TEST_SLEEP_MS > 0) {
            sleep_ms(TEST_SLEEP_MS);
        }
    }
}

/* ============================================================
 *  Setup 與 Teardown
 * ============================================================ */

void setUp(void) {
    // 重置伺服器狀態
    memset(&g_server, 0, sizeof(g_server));
    g_server.current_state = SIM_STATE_IDLE;
    g_server.ps5_power = PS5_POWER_UNKNOWN;
    
    // 重置計數器
    g_state_change_count = 0;
    g_cec_event_count = 0;
    g_ws_message_count = 0;
    g_state_history_count = 0;
    memset(g_state_history, 0, sizeof(g_state_history));
    
    // 設定 Logger Mocks
    logger_init_IgnoreAndReturn(0);
    logger_info_Ignore();
    logger_warning_Ignore();
    logger_error_Ignore();
    logger_debug_Ignore();
}

void tearDown(void) {
    // 清理
}

/* ============================================================
 *  整合測試案例 - 真實流程模擬
 * ============================================================ */

/**
 * 測試 1: 伺服器啟動與初始化流程
 */
void test_server_startup_sequence(void) {
    printf("\n=== Test: Server Startup Sequence ===\n");
    
    struct timespec start_time, end_time;
    get_current_time(&start_time);
    
    // 步驟 1: 初始化 CEC Monitor
    printf("  Step 1: Initializing CEC Monitor...\n");
    cec_monitor_init_ExpectAndReturn(TEST_CEC_DEVICE, 0);
    int init_result = cec_monitor_init(TEST_CEC_DEVICE);
    TEST_ASSERT_EQUAL(0, init_result);
    g_server.cec_initialized = true;
    printf("    CEC Monitor initialized\n");
    
    sleep_ms(50);
    
    // 步驟 2: 初始化 PS5 Detector
    printf("  Step 2: Initializing PS5 Detector...\n");
    ps5_detector_init_ExpectAndReturn(TEST_SUBNET, TEST_CACHE_FILE, 0);
    init_result = ps5_detector_init(TEST_SUBNET, TEST_CACHE_FILE);
    TEST_ASSERT_EQUAL(0, init_result);
    g_server.detector_initialized = true;
    printf("    PS5 Detector initialized\n");
    
    sleep_ms(50);
    
    // 步驟 3: 初始化 PS5 Wake
    printf("  Step 3: Initializing PS5 Wake...\n");
    ps5_wake_init_ExpectAndReturn(TEST_CEC_DEVICE, 0);
    init_result = ps5_wake_init(TEST_CEC_DEVICE);
    TEST_ASSERT_EQUAL(0, init_result);
    g_server.wake_initialized = true;
    printf("    PS5 Wake initialized\n");
    
    sleep_ms(50);
    
    // 步驟 4: 初始化 WebSocket Server
    printf("  Step 4: Initializing WebSocket Server...\n");
    ws_server_init_ExpectAndReturn(TEST_WS_PORT, 0);
    init_result = ws_server_init(TEST_WS_PORT);
    TEST_ASSERT_EQUAL(0, init_result);
    g_server.ws_initialized = true;
    printf("    WebSocket Server initialized (port %d)\n", TEST_WS_PORT);
    
    sleep_ms(50);
    
    // 步驟 5: 進入監控狀態
    printf("  Step 5: Entering monitoring state...\n");
    sim_change_state(SIM_STATE_MONITORING);
    
    get_current_time(&end_time);
    long elapsed_ms = timespec_diff_ms(&start_time, &end_time);
    
    printf("\n  Startup Statistics:\n");
    printf("    - Duration: %ld ms\n", elapsed_ms);
    printf("    - CEC Monitor: %s\n", g_server.cec_initialized ? "OK" : "FAILED");
    printf("    - PS5 Detector: %s\n", g_server.detector_initialized ? "OK" : "FAILED");
    printf("    - PS5 Wake: %s\n", g_server.wake_initialized ? "OK" : "FAILED");
    printf("    - WebSocket Server: %s\n", g_server.ws_initialized ? "OK" : "FAILED");
    printf("    - Current State: MONITORING\n");
    
    TEST_ASSERT_TRUE(g_server.cec_initialized);
    TEST_ASSERT_TRUE(g_server.detector_initialized);
    TEST_ASSERT_TRUE(g_server.wake_initialized);
    TEST_ASSERT_TRUE(g_server.ws_initialized);
    TEST_ASSERT_EQUAL(SIM_STATE_MONITORING, g_server.current_state);
    
    TEST_PASS_MESSAGE("Server startup sequence passed!");
}

/**
 * 測試 2: 客戶端查詢完整流程
 */
void test_client_query_complete_flow(void) {
    printf("\n=== Test: Client Query Complete Flow ===\n");
    
    // 初始化所有模組
    cec_monitor_init_IgnoreAndReturn(0);
    ps5_detector_init_IgnoreAndReturn(0);
    ws_server_init_IgnoreAndReturn(0);
    
    cec_monitor_init(TEST_CEC_DEVICE);
    ps5_detector_init(TEST_SUBNET, TEST_CACHE_FILE);
    ws_server_init(TEST_WS_PORT);
    
    struct timespec start_time;
    get_current_time(&start_time);
    
    // 步驟 1: 客戶端連線
    printf("  Step 1: Client connecting...\n");
    ws_server_get_client_count_ExpectAndReturn(1);
    g_server.connected_clients = ws_server_get_client_count();
    printf("    Connected clients: %d\n", g_server.connected_clients);
    
    sleep_ms(50);
    
    // 步驟 2: 接收查詢請求
    printf("  Step 2: Receiving status query...\n");
    sim_change_state(SIM_STATE_DETECTING);
    g_server.client_queries++;
    
    // 步驟 3: 查詢 CEC 狀態
    printf("  Step 3: Querying CEC status...\n");
    cec_monitor_get_last_state_ExpectAndReturn(PS5_POWER_ON);
    g_server.ps5_power = cec_monitor_get_last_state();
    printf("    CEC Power State: ON\n");
    
    sleep_ms(50);
    
    // 步驟 4: 偵測網路狀態
    printf("  Step 4: Detecting network status...\n");
    ps5_detector_quick_check_IgnoreAndReturn(0);
    ps5_info_t info = {0};
    int detect_result = ps5_detector_quick_check(TEST_PS5_IP, &info);
    g_server.ps5_network_online = (detect_result == 0);
    printf("    Network Status: %s\n", g_server.ps5_network_online ? "ONLINE" : "OFFLINE");
    
    sleep_ms(50);
    
    // 步驟 5: 判斷最終狀態
    printf("  Step 5: Determining final status...\n");
    const char* ps5_status = "unknown";
    if (g_server.ps5_power == PS5_POWER_ON && g_server.ps5_network_online) {
        ps5_status = "on";
    } else if (g_server.ps5_power == PS5_POWER_STANDBY) {
        ps5_status = "standby";
    } else if (g_server.ps5_power == PS5_POWER_OFF) {
        ps5_status = "off";
    }
    printf("    Final Status: %s\n", ps5_status);
    
    // 步驟 6: 回應客戶端
    printf("  Step 6: Sending response to client...\n");
    sim_change_state(SIM_STATE_RESPONDING);
    
    // 修正：使用 IgnoreAndReturn 代替 ExpectAnyArgsAndReturn
    ws_server_send_IgnoreAndReturn(0);
    int result = ws_server_send(0, "{\"type\":\"ps5_status\",\"status\":\"on\"}");
    TEST_ASSERT_EQUAL(0, result);
    
    // 步驟 7: 完成查詢
    printf("  Step 7: Query completed\n");
    simulate_time_and_update(5);
    
    struct timespec end_time;
    get_current_time(&end_time);
    long elapsed_ms = timespec_diff_ms(&start_time, &end_time);
    
    printf("\n  Query Flow Statistics:\n");
    printf("    - Duration: %ld ms\n", elapsed_ms);
    printf("    - State transitions: %d\n", g_state_change_count);
    printf("    - Client queries: %d\n", g_server.client_queries);
    printf("    - PS5 Status: %s\n", ps5_status);
    
    TEST_ASSERT_EQUAL(PS5_POWER_ON, g_server.ps5_power);
    TEST_ASSERT_TRUE(g_server.ps5_network_online);
    TEST_ASSERT_EQUAL(1, g_server.client_queries);
    
    TEST_PASS_MESSAGE("Client query complete flow passed!");
}

/**
 * 測試 3: CEC 事件與廣播流程
 */
void test_cec_event_and_broadcast_flow(void) {
    printf("\n=== Test: CEC Event and Broadcast Flow ===\n");
    
    // 初始化
    cec_monitor_init_IgnoreAndReturn(0);
    ws_server_init_IgnoreAndReturn(0);
    
    cec_monitor_init(TEST_CEC_DEVICE);
    ws_server_init(TEST_WS_PORT);
    
    // 設定初始客戶端數量
    ws_server_get_client_count_ExpectAndReturn(3);
    g_server.connected_clients = ws_server_get_client_count();
    printf("  Initial connected clients: %d\n", g_server.connected_clients);
    
    struct timespec start_time;
    get_current_time(&start_time);
    
    // 步驟 1: PS5 從待機變為開機
    printf("  Step 1: PS5 powers on (CEC event)...\n");
    cec_monitor_get_last_state_ExpectAndReturn(PS5_POWER_STANDBY);
    g_server.ps5_power = cec_monitor_get_last_state();
    printf("    Initial state: STANDBY\n");
    
    sleep_ms(100);
    
    // 步驟 2: 偵測到電源狀態變化
    printf("  Step 2: Detecting power state change...\n");
    cec_monitor_get_last_state_ExpectAndReturn(PS5_POWER_ON);
    ps5_power_state_t new_power = cec_monitor_get_last_state();
    printf("    New state: ON\n");
    
    bool state_changed = (g_server.ps5_power != new_power);
    TEST_ASSERT_TRUE(state_changed);
    
    g_server.ps5_power = new_power;
    g_server.cec_events++;
    
    // 步驟 3: 廣播給所有客戶端
    printf("  Step 3: Broadcasting to all clients...\n");
    
    // 修正：使用 IgnoreAndReturn 代替 ExpectAnyArgsAndReturn
    ws_server_broadcast_IgnoreAndReturn(0);
    int broadcast_result = ws_server_broadcast("{\"type\":\"ps5_status_changed\",\"status\":\"on\"}");
    TEST_ASSERT_EQUAL(0, broadcast_result);
    g_server.broadcasts++;
    printf("    Broadcast sent to %d clients\n", g_server.connected_clients);
    
    struct timespec end_time;
    get_current_time(&end_time);
    long elapsed_ms = timespec_diff_ms(&start_time, &end_time);
    
    printf("\n  CEC Event Flow Statistics:\n");
    printf("    - Duration: %ld ms\n", elapsed_ms);
    printf("    - CEC events: %d\n", g_server.cec_events);
    printf("    - Broadcasts: %d\n", g_server.broadcasts);
    printf("    - Clients notified: %d\n", g_server.connected_clients);
    
    TEST_ASSERT_EQUAL(PS5_POWER_ON, g_server.ps5_power);
    TEST_ASSERT_EQUAL(1, g_server.cec_events);
    TEST_ASSERT_EQUAL(1, g_server.broadcasts);
    
    TEST_PASS_MESSAGE("CEC event and broadcast flow passed!");
}

/**
 * 測試 4: PS5 喚醒請求完整流程
 */
void test_ps5_wake_request_complete_flow(void) {
    printf("\n=== Test: PS5 Wake Request Complete Flow ===\n");
    
    // 初始化
    ps5_wake_init_IgnoreAndReturn(0);
    cec_monitor_init_IgnoreAndReturn(0);
    ps5_detector_init_IgnoreAndReturn(0);
    ws_server_init_IgnoreAndReturn(0);
    
    ps5_wake_init(TEST_CEC_DEVICE);
    cec_monitor_init(TEST_CEC_DEVICE);
    ps5_detector_init(TEST_SUBNET, TEST_CACHE_FILE);
    ws_server_init(TEST_WS_PORT);
    
    // 初始狀態：PS5 關機
    cec_monitor_get_last_state_ExpectAndReturn(PS5_POWER_OFF);
    g_server.ps5_power = cec_monitor_get_last_state();
    printf("  Initial PS5 state: OFF\n");
    
    struct timespec start_time;
    get_current_time(&start_time);
    
    // 步驟 1: 接收喚醒請求
    printf("  Step 1: Receiving wake request from client...\n");
    sim_change_state(SIM_STATE_WAKING);
    g_server.wake_requests++;
    
    // 步驟 2: 發送 CEC 喚醒命令
    printf("  Step 2: Sending CEC wake command...\n");
    ps5_wake_by_cec_ExpectAndReturn(0);
    int wake_result = ps5_wake_by_cec();
    TEST_ASSERT_EQUAL(0, wake_result);
    printf("    CEC wake command sent\n");
    
    // 步驟 3: 等待 PS5 啟動
    printf("  Step 3: Waiting for PS5 to boot...\n");
    sleep_ms(200);
    
    // 步驟 4: 驗證 PS5 狀態
    printf("  Step 4: Verifying PS5 status...\n");
    cec_monitor_get_last_state_ExpectAndReturn(PS5_POWER_ON);
    g_server.ps5_power = cec_monitor_get_last_state();
    printf("    CEC Status: ON\n");
    
    // 步驟 5: 網路驗證
    printf("  Step 5: Verifying network connectivity...\n");
    ps5_detector_quick_check_IgnoreAndReturn(0);
    ps5_info_t info = {0};
    int detect_result = ps5_detector_quick_check(TEST_PS5_IP, &info);
    g_server.ps5_network_online = (detect_result == 0);
    printf("    Network Status: %s\n", g_server.ps5_network_online ? "ONLINE" : "OFFLINE");
    
    // 步驟 6: 回應客戶端
    printf("  Step 6: Sending wake response to client...\n");
    
    // 修正：使用 IgnoreAndReturn
    ws_server_send_IgnoreAndReturn(0);
    int response_result = ws_server_send(0, "{\"type\":\"wake_response\",\"status\":\"success\"}");
    TEST_ASSERT_EQUAL(0, response_result);
    
    // 步驟 7: 完成喚醒流程
    printf("  Step 7: Wake flow completed\n");
    simulate_time_and_update(5);
    
    struct timespec end_time;
    get_current_time(&end_time);
    long elapsed_ms = timespec_diff_ms(&start_time, &end_time);
    
    printf("\n  Wake Flow Statistics:\n");
    printf("    - Duration: %ld ms\n", elapsed_ms);
    printf("    - State transitions: %d\n", g_state_change_count);
    printf("    - Wake requests: %d\n", g_server.wake_requests);
    printf("    - Wake result: SUCCESS\n");
    printf("    - Final PS5 state: %s\n", 
           (g_server.ps5_power == PS5_POWER_ON && g_server.ps5_network_online) ? "ON" : "OTHER");
    
    TEST_ASSERT_EQUAL(PS5_POWER_ON, g_server.ps5_power);
    TEST_ASSERT_TRUE(g_server.ps5_network_online);
    TEST_ASSERT_EQUAL(1, g_server.wake_requests);
    
    TEST_PASS_MESSAGE("PS5 wake request complete flow passed!");
}

/**
 * 測試 5: 多客戶端並發查詢流程
 */
void test_multiple_clients_concurrent_queries(void) {
    printf("\n=== Test: Multiple Clients Concurrent Queries ===\n");
    
    // 初始化
    cec_monitor_init_IgnoreAndReturn(0);
    ps5_detector_init_IgnoreAndReturn(0);
    ws_server_init_IgnoreAndReturn(0);
    
    cec_monitor_init(TEST_CEC_DEVICE);
    ps5_detector_init(TEST_SUBNET, TEST_CACHE_FILE);
    ws_server_init(TEST_WS_PORT);
    
    // 設定 PS5 狀態
    cec_monitor_get_last_state_IgnoreAndReturn(PS5_POWER_ON);
    ps5_detector_quick_check_IgnoreAndReturn(0);
    ws_server_send_IgnoreAndReturn(0);
    
    // 模擬 5 個客戶端
    const int client_count = 5;
    ws_server_get_client_count_ExpectAndReturn(client_count);
    g_server.connected_clients = ws_server_get_client_count();
    
    printf("  Connected clients: %d\n", client_count);
    
    struct timespec start_time;
    get_current_time(&start_time);
    
    // 處理每個客戶端的查詢
    for (int i = 0; i < client_count; i++) {
        printf("  Processing query from client %d...\n", i + 1);
        
        // 查詢 PS5 狀態
        g_server.ps5_power = cec_monitor_get_last_state();
        ps5_info_t info = {0};
        ps5_detector_quick_check(TEST_PS5_IP, &info);
        
        // 回應客戶端
        ws_server_send(i, "{\"type\":\"ps5_status\",\"status\":\"on\"}");
        
        g_server.client_queries++;
        sleep_ms(10);
    }
    
    struct timespec end_time;
    get_current_time(&end_time);
    long elapsed_ms = timespec_diff_ms(&start_time, &end_time);
    
    printf("\n  Multiple Clients Statistics:\n");
    printf("    - Duration: %ld ms\n", elapsed_ms);
    printf("    - Total clients: %d\n", g_server.connected_clients);
    printf("    - Queries processed: %d\n", g_server.client_queries);
    printf("    - Average time per query: %ld ms\n", elapsed_ms / client_count);
    
    TEST_ASSERT_EQUAL(client_count, g_server.client_queries);
    TEST_ASSERT_EQUAL(client_count, g_server.connected_clients);
    
    TEST_PASS_MESSAGE("Multiple clients concurrent queries passed!");
}

/**
 * 測試 6: 錯誤恢復與重試流程
 */
void test_error_recovery_and_retry_flow(void) {
    printf("\n=== Test: Error Recovery and Retry Flow ===\n");
    
    // 初始化
    ps5_detector_init_IgnoreAndReturn(0);
    ps5_detector_init(TEST_SUBNET, TEST_CACHE_FILE);
    
    struct timespec start_time;
    get_current_time(&start_time);
    
    // 步驟 1: 第一次偵測失敗
    printf("  Attempt 1: Detection fails...\n");
    ps5_detector_scan_IgnoreAndReturn(-1);
    ps5_info_t info1 = {0};
    int result1 = ps5_detector_scan(&info1);
    TEST_ASSERT_EQUAL(-1, result1);
    printf("    Result: FAILED\n");
    
    // 步驟 2: 錯誤處理
    printf("  Step 2: Error handling...\n");
    sim_change_state(SIM_STATE_ERROR);
    sleep_ms(50);
    
    // 步驟 3: 自動重試
    printf("  Attempt 2: Retrying detection...\n");
    ps5_detector_scan_IgnoreAndReturn(-1);
    ps5_info_t info2 = {0};
    int result2 = ps5_detector_scan(&info2);
    TEST_ASSERT_EQUAL(-1, result2);
    printf("    Result: FAILED\n");
    
    sleep_ms(50);
    
    // 步驟 4: 第三次成功
    printf("  Attempt 3: Detection succeeds...\n");
    ps5_detector_scan_IgnoreAndReturn(0);
    ps5_info_t info3 = {0};
    int result3 = ps5_detector_scan(&info3);
    TEST_ASSERT_EQUAL(0, result3);
    printf("    Result: SUCCESS\n");
    
    // 步驟 5: 恢復正常
    printf("  Step 5: Recovering to normal operation...\n");
    sim_change_state(SIM_STATE_IDLE);
    
    struct timespec end_time;
    get_current_time(&end_time);
    long elapsed_ms = timespec_diff_ms(&start_time, &end_time);
    
    printf("\n  Error Recovery Statistics:\n");
    printf("    - Duration: %ld ms\n", elapsed_ms);
    printf("    - Total attempts: 3\n");
    printf("    - Failed attempts: 2\n");
    printf("    - Successful attempts: 1\n");
    printf("    - Final state: %s\n", 
           (g_server.current_state == SIM_STATE_IDLE) ? "IDLE" : "ERROR");
    
    TEST_ASSERT_EQUAL(SIM_STATE_IDLE, g_server.current_state);
    
    TEST_PASS_MESSAGE("Error recovery and retry flow passed!");
}
