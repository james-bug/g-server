/**
 * @file ps5_wake.h
 * @brief PS5 Wake - 透過 CEC 喚醒 PS5
 * 
 * 根據架構設計,PS5 喚醒**只透過 HDMI-CEC**,不使用 WOL
 * 
 * @copyright Gaming System Development Team
 * @date 2025-11-04
 */

#ifndef PS5_WAKE_H
#define PS5_WAKE_H

#include <stdbool.h>

/**
 * @brief PS5 喚醒錯誤碼
 */
typedef enum {
    WAKE_OK = 0,                  /**< 成功 */
    WAKE_ERROR_NOT_INIT = -1,     /**< 未初始化 */
    WAKE_ERROR_CEC_FAILED = -2,   /**< CEC 指令失敗 */
    WAKE_ERROR_TIMEOUT = -3,      /**< 超時 */
    WAKE_ERROR_VERIFY_FAILED = -4,/**< 驗證失敗 */
    WAKE_ERROR_ALREADY_ON = -5,   /**< 已經開機 */
} ps5_wake_error_t;

/**
 * @brief PS5 喚醒結果
 */
typedef struct {
    bool success;                 /**< 是否成功 */
    ps5_wake_error_t error_code;  /**< 錯誤碼 */
    char message[128];            /**< 錯誤訊息 */
} ps5_wake_result_t;

/**
 * @brief 初始化 PS5 喚醒模組
 * 
 * @param cec_device CEC 裝置路徑,例如 "/dev/cec0"
 * @return 0 成功, <0 失敗
 */
int ps5_wake_init(const char *cec_device);

/**
 * @brief 執行完整的 PS5 喚醒流程
 * 
 * 流程:
 * 1. 發送 CEC 喚醒指令
 * 2. 等待 3 秒
 * 3. 驗證 PS5 電源狀態
 * 
 * @param result 輸出喚醒結果
 * @return 0 成功, <0 失敗
 * 
 * @note 此函數會阻塞 3 秒,建議在獨立執行緒或 fork 子程序中執行
 */
int ps5_wake_execute(ps5_wake_result_t *result);

/**
 * @brief 發送 CEC 喚醒指令
 * 
 * 執行: cec-ctl -d /dev/cec0 --playback -S
 * 或使用: cec-utility wake (如果可用)
 * 
 * @return 0 成功, <0 失敗
 */
int ps5_wake_send_command(void);

/**
 * @brief 驗證 PS5 是否成功喚醒
 * 
 * 透過查詢 CEC 電源狀態驗證
 * 
 * @param timeout_sec 超時時間 (秒), 預設 30 秒
 * @return true PS5 已開機, false 未開機
 */
bool ps5_wake_verify(int timeout_sec);

/**
 * @brief 非阻塞版本: Fork 子程序執行喚醒
 * 
 * 設計理由:
 * - 喚醒流程包含 3 秒等待
 * - 避免阻塞主程序的 WebSocket Server
 * - 獨立程序執行,透過 Exit Code 回報結果
 * 
 * @param result 輸出喚醒結果
 * @return 子程序 PID (>0), 0 表示在子程序中, <0 失敗
 * 
 * @note 父程序應使用 waitpid() 收集結果
 */
pid_t ps5_wake_execute_async(ps5_wake_result_t *result);

/**
 * @brief 取得最後一次喚醒結果
 * 
 * @return 喚醒結果結構
 */
ps5_wake_result_t ps5_wake_get_last_result(void);

/**
 * @brief 清理資源
 */
void ps5_wake_cleanup(void);

/**
 * @brief 錯誤碼轉換為字串
 * 
 * @param error 錯誤碼
 * @return 錯誤訊息字串
 */
const char* ps5_wake_error_string(ps5_wake_error_t error);

#endif // PS5_WAKE_H
