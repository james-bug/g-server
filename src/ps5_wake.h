/**
 * @file ps5_wake.h
 * @brief PS5 Wake Module - 透過 HDMI-CEC 喚醒 PS5
 * 
 * 注意: PS5 僅支援 CEC 喚醒,不支援 WOL
 */

#ifndef PS5_WAKE_H
#define PS5_WAKE_H

#include <stdbool.h>
#include "ps5_detector.h"

/**
 * @brief 喚醒結果
 */
typedef enum {
    WAKE_RESULT_SUCCESS = 0,      /**< 喚醒成功 */
    WAKE_RESULT_TIMEOUT,          /**< 超時 */
    WAKE_RESULT_CEC_ERROR,        /**< CEC 錯誤 */
    WAKE_RESULT_VERIFY_FAILED,    /**< 驗證失敗 */
    WAKE_RESULT_NOT_INITIALIZED,  /**< 未初始化 */
} wake_result_t;

/**
 * @brief 初始化 PS5 喚醒模組
 * @param cec_device CEC 裝置路徑,如 "/dev/cec0"
 * @return 0 成功, <0 失敗
 */
int ps5_wake_init(const char *cec_device);

/**
 * @brief 透過 CEC 喚醒 PS5
 * @return 0 成功, <0 失敗
 */
int ps5_wake_by_cec(void);

/**
 * @brief 喚醒 PS5 並驗證 (主要 API)
 * @param info PS5 資訊 (用於驗證)
 * @param timeout_sec 驗證超時時間 (秒)
 * @return wake_result_t 喚醒結果
 */
wake_result_t ps5_wake(const ps5_info_t *info, int timeout_sec);

/**
 * @brief 驗證 PS5 是否成功喚醒
 * @param ip PS5 IP 位址
 * @param timeout_sec 超時時間 (秒)
 * @return true 成功喚醒, false 失敗
 */
bool ps5_wake_verify(const char *ip, int timeout_sec);

/**
 * @brief 重試喚醒 (如果第一次失敗)
 * @param info PS5 資訊
 * @param max_retries 最大重試次數
 * @param timeout_sec 每次驗證超時時間 (秒)
 * @return wake_result_t 喚醒結果
 */
wake_result_t ps5_wake_with_retry(const ps5_info_t *info, 
                                   int max_retries, 
                                   int timeout_sec);

/**
 * @brief 取得 CEC 裝置狀態
 * @return true CEC 裝置可用, false 不可用
 */
bool ps5_wake_is_cec_available(void);

/**
 * @brief 清理資源
 */
void ps5_wake_cleanup(void);

/**
 * @brief 喚醒結果轉換為字串
 * @param result 喚醒結果
 * @return 結果描述字串
 */
const char* ps5_wake_result_string(wake_result_t result);

/**
 * @brief 錯誤碼轉換為字串
 * @param error 錯誤碼
 * @return 錯誤訊息字串
 */
const char* ps5_wake_error_string(int error);

#endif // PS5_WAKE_H
