/**
 * @file cec_monitor.c
 * @brief CEC Monitor - HDMI-CEC 訊息監控實作
 * 
 * 使用 v4l-utils 的 cec-ctl 工具監控 PS5 電源狀態
 * 
 * @copyright Gaming System Development Team
 * @date 2025-11-04
 */

#include "cec_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// 條件編譯: 根據不同模式選擇 logger
#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming/logger.h>
  #else
    #include "../../gaming-core/src/logger.h"
  #endif
  #define LOG_INFO(...)  logger_info(__VA_ARGS__)
  #define LOG_ERROR(...) logger_error(__VA_ARGS__)
  #define LOG_DEBUG(...) logger_debug(__VA_ARGS__)
#else
  // 測試模式: 不輸出日誌
  #define LOG_INFO(...)  
  #define LOG_ERROR(...) 
  #define LOG_DEBUG(...) 
#endif

// 全域上下文
static cec_monitor_ctx_t g_ctx = {
    .device = "",
    .last_state = PS5_POWER_UNKNOWN,
    .last_update = 0,
    .callback = NULL,
    .user_data = NULL,
    .running = false,
};

// CEC 指令模板
#define CEC_CMD_QUERY_POWER "cec-ctl -d %s --give-device-power-status 2>&1"
#define CEC_CMD_MONITOR     "cec-ctl -M -d %s 2>&1"

/**
 * @brief 解析 CEC 輸出，提取電源狀態
 */
static ps5_power_state_t parse_power_state(const char *output) {
    if (!output) return PS5_POWER_UNKNOWN;
    
    // 查找電源狀態關鍵字
    if (strstr(output, "power status: on") || 
        strstr(output, "pwr-state: on")) {
        return PS5_POWER_ON;
    }
    
    if (strstr(output, "power status: standby") ||
        strstr(output, "pwr-state: standby")) {
        return PS5_POWER_STANDBY;
    }
    
    if (strstr(output, "power status: to-standby") ||
        strstr(output, "pwr-state: to-standby")) {
        return PS5_POWER_STANDBY;
    }
    
    // 檢查 active source (表示開機)
    if (strstr(output, "active source")) {
        return PS5_POWER_ON;
    }
    
    // 檢查 inactive source (表示待機/關機)
    if (strstr(output, "inactive source") ||
        strstr(output, "standby")) {
        return PS5_POWER_STANDBY;
    }
    
    return PS5_POWER_UNKNOWN;
}

/**
 * @brief 執行 CEC 指令並讀取輸出
 */
static int execute_cec_command(const char *cmd, char *output, size_t output_size) {
    FILE *fp;
    
    LOG_DEBUG("Executing CEC command: %s", cmd);
    
    fp = popen(cmd, "r");
    if (!fp) {
        LOG_ERROR("Failed to execute CEC command: %s", strerror(errno));
        return CEC_ERROR_EXEC_FAILED;
    }
    
    // 讀取輸出
    size_t total = 0;
    while (fgets(output + total, output_size - total, fp) != NULL) {
        total = strlen(output);
        if (total >= output_size - 1) break;
    }
    
    int status = pclose(fp);
    if (status == -1) {
        LOG_ERROR("pclose() failed: %s", strerror(errno));
        return CEC_ERROR_EXEC_FAILED;
    }
    
    LOG_DEBUG("CEC command output: %s", output);
    
    return CEC_OK;
}

/**
 * @brief 觸發狀態變化事件
 */
static void trigger_event(ps5_power_state_t new_state) {
    ps5_power_state_t old_state = g_ctx.last_state;
    
    // 狀態未改變，不觸發事件
    if (new_state == old_state) {
        return;
    }
    
    // 更新狀態
    g_ctx.last_state = new_state;
    g_ctx.last_update = time(NULL);
    
    // 觸發回調
    if (g_ctx.callback) {
        cec_event_t event;
        
        switch (new_state) {
            case PS5_POWER_ON:
                event = CEC_EVENT_POWER_ON;
                break;
            case PS5_POWER_STANDBY:
                event = CEC_EVENT_STANDBY;
                break;
            case PS5_POWER_OFF:
                event = CEC_EVENT_POWER_OFF;
                break;
            default:
                return; // 未知狀態不觸發事件
        }
        
        LOG_INFO("CEC state changed: %s -> %s", 
                 cec_monitor_state_string(old_state),
                 cec_monitor_state_string(new_state));
        
        g_ctx.callback(event, new_state, g_ctx.user_data);
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

int cec_monitor_init(const char *device) {
    if (!device || strlen(device) == 0) {
        LOG_ERROR("Invalid CEC device path");
        return CEC_ERROR_DEVICE_NOT_FOUND;
    }
    
    // 檢查裝置是否存在
    if (access(device, R_OK) != 0) {
        LOG_ERROR("CEC device not accessible: %s", device);
        return CEC_ERROR_DEVICE_NOT_FOUND;
    }
    
    // 初始化上下文
    strncpy(g_ctx.device, device, sizeof(g_ctx.device) - 1);
    g_ctx.last_state = PS5_POWER_UNKNOWN;
    g_ctx.last_update = 0;
    g_ctx.callback = NULL;
    g_ctx.user_data = NULL;
    g_ctx.running = false;
    
    LOG_INFO("CEC Monitor initialized with device: %s", device);
    
    // 執行一次查詢獲取初始狀態
    ps5_power_state_t initial_state;
    if (cec_monitor_query_state(&initial_state) == CEC_OK) {
        g_ctx.last_state = initial_state;
        g_ctx.last_update = time(NULL);
        LOG_INFO("Initial PS5 state: %s", cec_monitor_state_string(initial_state));
    }
    
    return CEC_OK;
}

void cec_monitor_set_callback(cec_callback_t callback, void *user_data) {
    g_ctx.callback = callback;
    g_ctx.user_data = user_data;
    LOG_DEBUG("CEC callback registered");
}

int cec_monitor_run(void) {
    LOG_INFO("CEC Monitor started (blocking mode)");
    
    g_ctx.running = true;
    
    while (g_ctx.running) {
        int ret = cec_monitor_process(1000); // 1 秒超時
        if (ret != CEC_OK && ret != CEC_ERROR_TIMEOUT) {
            LOG_ERROR("CEC monitor process failed: %d", ret);
            usleep(1000000); // 1 秒後重試
        }
    }
    
    LOG_INFO("CEC Monitor stopped");
    return CEC_OK;
}

int cec_monitor_process(int timeout_ms) {
    char output[4096] = {0};
    char cmd[256];
    
    // 構建查詢指令
    snprintf(cmd, sizeof(cmd), CEC_CMD_QUERY_POWER, g_ctx.device);
    
    // 執行指令
    int ret = execute_cec_command(cmd, output, sizeof(output));
    if (ret != CEC_OK) {
        return ret;
    }
    
    // 解析狀態
    ps5_power_state_t new_state = parse_power_state(output);
    if (new_state == PS5_POWER_UNKNOWN) {
        LOG_DEBUG("Could not parse power state from CEC output");
        return CEC_ERROR_PARSE_FAILED;
    }
    
    // 觸發事件 (如果狀態改變)
    trigger_event(new_state);
    
    return CEC_OK;
}

int cec_monitor_query_state(ps5_power_state_t *state) {
    if (!state) {
        return CEC_ERROR_NOT_INIT;
    }
    
    char output[4096] = {0};
    char cmd[256];
    
    // 構建查詢指令
    snprintf(cmd, sizeof(cmd), CEC_CMD_QUERY_POWER, g_ctx.device);
    
    // 執行指令
    int ret = execute_cec_command(cmd, output, sizeof(output));
    if (ret != CEC_OK) {
        *state = PS5_POWER_UNKNOWN;
        return ret;
    }
    
    // 解析狀態
    *state = parse_power_state(output);
    
    return CEC_OK;
}

ps5_power_state_t cec_monitor_get_last_state(void) {
    return g_ctx.last_state;
}

time_t cec_monitor_get_last_update(void) {
    return g_ctx.last_update;
}

void cec_monitor_stop(void) {
    g_ctx.running = false;
    LOG_INFO("CEC Monitor stop requested");
}

void cec_monitor_cleanup(void) {
    g_ctx.running = false;
    g_ctx.callback = NULL;
    g_ctx.user_data = NULL;
    LOG_INFO("CEC Monitor cleaned up");
}

const char* cec_monitor_state_string(ps5_power_state_t state) {
    switch (state) {
        case PS5_POWER_UNKNOWN:  return "unknown";
        case PS5_POWER_ON:       return "on";
        case PS5_POWER_STANDBY:  return "standby";
        case PS5_POWER_OFF:      return "off";
        default:                 return "invalid";
    }
}

const char* cec_monitor_event_string(cec_event_t event) {
    switch (event) {
        case CEC_EVENT_POWER_ON:   return "power_on";
        case CEC_EVENT_STANDBY:    return "standby";
        case CEC_EVENT_POWER_OFF:  return "power_off";
        default:                   return "unknown";
    }
}

const char* cec_monitor_error_string(cec_error_t error) {
    switch (error) {
        case CEC_OK:                     return "Success";
        case CEC_ERROR_NOT_INIT:         return "Not initialized";
        case CEC_ERROR_DEVICE_NOT_FOUND: return "Device not found";
        case CEC_ERROR_PARSE_FAILED:     return "Parse failed";
        case CEC_ERROR_TIMEOUT:          return "Timeout";
        case CEC_ERROR_EXEC_FAILED:      return "Execution failed";
        default:                         return "Unknown error";
    }
}

bool cec_monitor_device_available(const char *device) {
    if (!device) return false;
    return (access(device, R_OK) == 0);
}
