/**
 * @file cec_monitor.h
 * @brief CEC Monitor - HDMI-CEC 訊息監控
 * 
 * 監控 HDMI-CEC 訊息,偵測 PS5 電源狀態變化
 * 使用 v4l-utils 的 cec-ctl 工具
 * 
 * @copyright Gaming System Development Team
 * @date 2025-11-04
 */

#ifndef CEC_MONITOR_H
#define CEC_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * @brief PS5 電源狀態
 */
typedef enum {
    PS5_POWER_UNKNOWN = 0,    /**< 未知狀態 */
    PS5_POWER_ON,             /**< 開機 */
    PS5_POWER_STANDBY,        /**< 待機 */
    PS5_POWER_OFF,            /**< 關機 */
} ps5_power_state_t;

/**
 * @brief CEC 事件類型
 */
typedef enum {
    CEC_EVENT_POWER_ON = 0,   /**< PS5 開機 */
    CEC_EVENT_STANDBY,        /**< PS5 進入待機 */
    CEC_EVENT_POWER_OFF,      /**< PS5 關機 */
} cec_event_t;

/**
 * @brief CEC 錯誤碼
 */
typedef enum {
    CEC_OK = 0,                     /**< 成功 */
    CEC_ERROR_NOT_INIT = -1,        /**< 未初始化 */
    CEC_ERROR_DEVICE_NOT_FOUND = -2,/**< 裝置不存在 */
    CEC_ERROR_PARSE_FAILED = -3,    /**< 解析失敗 */
    CEC_ERROR_TIMEOUT = -4,         /**< 超時 */
    CEC_ERROR_EXEC_FAILED = -5,     /**< 執行失敗 */
} cec_error_t;

/**
 * @brief CEC 事件回調函數類型
 * 
 * @param event CEC 事件
 * @param state 新的電源狀態
 * @param user_data 使用者資料指標
 */
typedef void (*cec_callback_t)(cec_event_t event, 
                               ps5_power_state_t state, 
                               void *user_data);

/**
 * @brief CEC 監控上下文
 */
typedef struct {
    char device[32];              /**< CEC 裝置路徑 */
    ps5_power_state_t last_state; /**< 最後的電源狀態 */
    time_t last_update;           /**< 最後更新時間 */
    cec_callback_t callback;      /**< 事件回調函數 */
    void *user_data;              /**< 使用者資料 */
    bool running;                 /**< 是否運行中 */
} cec_monitor_ctx_t;

/**
 * @brief 初始化 CEC 監控器
 * 
 * 初始化 CEC 裝置並準備監控
 * 
 * @param device CEC 裝置路徑,如 "/dev/cec0"
 * @return 0 成功, <0 失敗(CEC_ERROR_*)
 * 
 * @note 需要 v4l-utils 套件已安裝
 */
int cec_monitor_init(const char *device);

/**
 * @brief 設定 CEC 事件回調
 * 
 * @param callback 回調函數指標
 * @param user_data 使用者資料指標,會傳遞給回調函數
 * 
 * @note 回調函數在監控執行緒中被呼叫,請注意執行緒安全
 */
void cec_monitor_set_callback(cec_callback_t callback, void *user_data);

/**
 * @brief 開始監控 (阻塞式)
 * 
 * 持續監控 CEC 訊息,直到被停止
 * 
 * @return 0 正常停止, <0 錯誤
 * 
 * @note 此函數會阻塞,建議在獨立執行緒中執行
 */
int cec_monitor_run(void);

/**
 * @brief 處理 CEC 事件 (非阻塞,單次)
 * 
 * 執行一次 CEC 查詢並處理事件
 * 適合整合到主事件循環中
 * 
 * @param timeout_ms 超時時間 (毫秒), -1 為永久等待
 * @return 0 成功, <0 失敗
 */
int cec_monitor_process(int timeout_ms);

/**
 * @brief 主動查詢當前 PS5 電源狀態
 * 
 * 執行一次性的 CEC 查詢,不觸發回調
 * 
 * @param state 輸出電源狀態
 * @return 0 成功, <0 失敗
 */
int cec_monitor_query_state(ps5_power_state_t *state);

/**
 * @brief 取得最後的電源狀態
 * 
 * @return PS5 電源狀態
 * 
 * @note 此函數返回快取的狀態,不執行實際查詢
 */
ps5_power_state_t cec_monitor_get_last_state(void);

/**
 * @brief 取得最後更新時間
 * 
 * @return Unix 時間戳
 */
time_t cec_monitor_get_last_update(void);

/**
 * @brief 停止監控
 * 
 * 停止 cec_monitor_run() 的執行
 */
void cec_monitor_stop(void);

/**
 * @brief 清理資源
 * 
 * 釋放所有配置的資源
 */
void cec_monitor_cleanup(void);

/**
 * @brief 電源狀態轉換為字串
 * 
 * @param state 電源狀態
 * @return 狀態字串 ("unknown", "on", "standby", "off")
 */
const char* cec_monitor_state_string(ps5_power_state_t state);

/**
 * @brief CEC 事件轉換為字串
 * 
 * @param event CEC 事件
 * @return 事件字串
 */
const char* cec_monitor_event_string(cec_event_t event);

/**
 * @brief 錯誤碼轉換為字串
 * 
 * @param error 錯誤碼
 * @return 錯誤訊息字串
 */
const char* cec_monitor_error_string(cec_error_t error);

/**
 * @brief 檢查 CEC 裝置是否可用
 * 
 * @param device CEC 裝置路徑
 * @return true 可用, false 不可用
 */
bool cec_monitor_device_available(const char *device);

#endif // CEC_MONITOR_H
