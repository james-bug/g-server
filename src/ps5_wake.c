/**
 * @file ps5_wake.c
 * @brief PS5 Wake Implementation - 透過 HDMI-CEC 喚醒 PS5
 */

// POSIX headers for time and sleep functions
#define _POSIX_C_SOURCE 200112L

#include "ps5_wake.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

// 全域變數
static char g_cec_device[64] = {0};
static bool g_initialized = false;

// 內部函數宣告
static int execute_cec_command(const char *command);
static bool ping_ps5(const char *ip);

/**
 * 執行 CEC 命令
 */
static int execute_cec_command(const char *command) {
    if (command == NULL) {
        return -1;
    }
    
#ifdef TESTING
    // 測試模式: 模擬成功
    return 0;
#else
    // 實際執行 cec-ctl 命令
    char full_cmd[256];
    snprintf(full_cmd, sizeof(full_cmd), "cec-ctl -d %s %s", g_cec_device, command);
    
    int result = system(full_cmd);
    if (WIFEXITED(result)) {
        return WEXITSTATUS(result);
    }
    
    return -1;
#endif
}

/**
 * Ping 檢查 PS5 是否在線
 */
static bool ping_ps5(const char *ip) {
    if (ip == NULL) {
        return false;
    }
    
#ifdef TESTING
    // 測試模式: 模擬成功
    return true;
#else
    // 實際執行 ping 命令
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1", ip);
    
    int result = system(cmd);
    return (result == 0);
#endif
}

/**
 * 初始化 PS5 喚醒模組
 */
int ps5_wake_init(const char *cec_device) {
    if (cec_device == NULL) {
        return -1;
    }
    
    // 儲存 CEC 裝置路徑
    snprintf(g_cec_device, sizeof(g_cec_device), "%s", cec_device);
    
    // 檢查 CEC 裝置是否存在
#ifndef TESTING
    if (access(g_cec_device, R_OK | W_OK) != 0) {
        return -2;  // CEC 裝置不存在或無權限
    }
#endif
    
    g_initialized = true;
    return 0;
}

/**
 * 透過 CEC 喚醒 PS5
 */
int ps5_wake_by_cec(void) {
    if (!g_initialized) {
        return -1;
    }
    
    // 發送 CEC "Image View On" 命令
    // 這會喚醒 HDMI 連接的裝置 (包括 PS5)
    int result = execute_cec_command("--image-view-on");
    if (result != 0) {
        return -2;  // CEC 命令執行失敗
    }
    
    // 短暫延遲,讓 PS5 有時間回應 (500ms)
    struct timespec sleep_time = {
        .tv_sec = 0,
        .tv_nsec = 500000000  // 500ms = 500,000,000 ns
    };
    nanosleep(&sleep_time, NULL);
    
    return 0;
}

/**
 * 驗證 PS5 是否成功喚醒
 */
bool ps5_wake_verify(const char *ip, int timeout_sec) {
    if (ip == NULL || timeout_sec <= 0) {
        return false;
    }
    
    time_t start_time = time(NULL);
    time_t current_time;
    
    // 持續 ping 檢查,直到超時
    while (1) {
        if (ping_ps5(ip)) {
            return true;  // PS5 已在線
        }
        
        // 檢查是否超時
        current_time = time(NULL);
        if ((current_time - start_time) >= timeout_sec) {
            return false;  // 超時
        }
        
        // 等待 1 秒後重試
        struct timespec sleep_time = {
            .tv_sec = 1,
            .tv_nsec = 0
        };
        nanosleep(&sleep_time, NULL);
    }
    
    return false;
}

/**
 * 喚醒 PS5 並驗證
 */
wake_result_t ps5_wake(const ps5_info_t *info, int timeout_sec) {
    if (!g_initialized) {
        return WAKE_RESULT_NOT_INITIALIZED;
    }
    
    if (info == NULL || timeout_sec <= 0) {
        return WAKE_RESULT_NOT_INITIALIZED;
    }
    
    // 1. 發送 CEC 喚醒命令
    int result = ps5_wake_by_cec();
    if (result != 0) {
        return WAKE_RESULT_CEC_ERROR;
    }
    
    // 2. 驗證 PS5 是否成功喚醒
    bool verified = ps5_wake_verify(info->ip, timeout_sec);
    if (!verified) {
        return WAKE_RESULT_VERIFY_FAILED;
    }
    
    return WAKE_RESULT_SUCCESS;
}

/**
 * 重試喚醒
 */
wake_result_t ps5_wake_with_retry(const ps5_info_t *info, 
                                   int max_retries, 
                                   int timeout_sec) {
    if (!g_initialized || info == NULL) {
        return WAKE_RESULT_NOT_INITIALIZED;
    }
    
    if (max_retries <= 0) {
        max_retries = 1;  // 至少嘗試一次
    }
    
    wake_result_t result;
    
    for (int i = 0; i < max_retries; i++) {
        // 嘗試喚醒
        result = ps5_wake(info, timeout_sec);
        
        if (result == WAKE_RESULT_SUCCESS) {
            return WAKE_RESULT_SUCCESS;
        }
        
        // 如果不是最後一次嘗試,等待後重試
        if (i < max_retries - 1) {
            // 等待 2 秒
            struct timespec sleep_time = {
                .tv_sec = 2,
                .tv_nsec = 0
            };
            nanosleep(&sleep_time, NULL);
        }
    }
    
    return result;  // 返回最後一次的結果
}

/**
 * 取得 CEC 裝置狀態
 */
bool ps5_wake_is_cec_available(void) {
    if (!g_initialized) {
        return false;
    }
    
#ifdef TESTING
    return true;
#else
    // 檢查 CEC 裝置是否可訪問
    return (access(g_cec_device, R_OK | W_OK) == 0);
#endif
}

/**
 * 清理資源
 */
void ps5_wake_cleanup(void) {
    memset(g_cec_device, 0, sizeof(g_cec_device));
    g_initialized = false;
}

/**
 * 喚醒結果轉換為字串
 */
const char* ps5_wake_result_string(wake_result_t result) {
    switch (result) {
        case WAKE_RESULT_SUCCESS:
            return "Wake successful";
        case WAKE_RESULT_TIMEOUT:
            return "Timeout waiting for PS5 to wake";
        case WAKE_RESULT_CEC_ERROR:
            return "CEC command failed";
        case WAKE_RESULT_VERIFY_FAILED:
            return "PS5 did not respond after wake";
        case WAKE_RESULT_NOT_INITIALIZED:
            return "Wake module not initialized";
        default:
            return "Unknown result";
    }
}

/**
 * 錯誤碼轉換為字串
 */
const char* ps5_wake_error_string(int error) {
    switch (error) {
        case 0:
            return "Success";
        case -1:
            return "Not initialized or invalid parameters";
        case -2:
            return "CEC device not accessible";
        case -3:
            return "CEC command execution failed";
        default:
            return "Unknown error";
    }
}
