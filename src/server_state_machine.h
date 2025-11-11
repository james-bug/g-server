/**
 * @file server_state_machine.h
 * @brief Server State Machine Module - 協調 gaming-server 各模組運作
 * 
 * 此模組協調所有伺服器功能:
 * - CEC 監控狀態
 * - PS5 偵測結果
 * - WebSocket 客戶端請求
 * - PS5 喚醒命令
 * 
 * @author Gaming System Development Team
 * @date 2025-11-06
 * @version 1.0.0
 */

#ifndef SERVER_STATE_MACHINE_H
#define SERVER_STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "ps5_detector.h"
#include "cec_monitor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ServerStateMachine Server State Machine Module
 * @brief Server state management and coordination
 * @{
 */

/* ============================================================
 *  Constants and Macros
 * ============================================================ */

/** 狀態超時時間 (秒) */
#define SERVER_STATE_TIMEOUT_SEC        30

/** PS5 偵測間隔 (秒) */
#define SERVER_DETECT_INTERVAL_SEC      60

/** 狀態更新間隔 (毫秒) */
#define SERVER_UPDATE_INTERVAL_MS       100

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief 伺服器狀態
 */
typedef enum {
    SERVER_STATE_INIT = 0,          /**< 初始化中 */
    SERVER_STATE_IDLE,              /**< 空閒,等待事件 */
    SERVER_STATE_MONITORING,        /**< 監控 PS5 狀態 */
    SERVER_STATE_DETECTING,         /**< 偵測 PS5 位置 */
    SERVER_STATE_QUERYING,          /**< 處理客戶端查詢 */
    SERVER_STATE_WAKING,            /**< 喚醒 PS5 中 */
    SERVER_STATE_BROADCASTING,      /**< 廣播狀態更新 */
    SERVER_STATE_ERROR,             /**< 錯誤狀態 */
} server_state_t;

/**
 * @brief 伺服器事件
 */
typedef enum {
    SERVER_EVENT_NONE = 0,          /**< 無事件 */
    SERVER_EVENT_CEC_CHANGE,        /**< CEC 狀態變化 */
    SERVER_EVENT_CLIENT_QUERY,      /**< 客戶端查詢 */
    SERVER_EVENT_WAKE_REQUEST,      /**< 喚醒請求 */
    SERVER_EVENT_DETECT_TIMEOUT,    /**< 偵測超時 */
    SERVER_EVENT_COMPLETED,         /**< 操作完成 */
    SERVER_EVENT_ERROR,             /**< 錯誤發生 */
} server_event_t;

/**
 * @brief PS5 綜合狀態
 */
typedef struct {
    ps5_power_state_t cec_state;    /**< CEC 電源狀態 */
    bool network_online;            /**< 網路是否在線 */
    ps5_info_t info;                /**< PS5 資訊 */
    time_t last_update;             /**< 最後更新時間 */
} ps5_status_t;

/**
 * @brief 伺服器上下文
 */
typedef struct {
    // 狀態
    server_state_t state;           /**< 當前狀態 */
    server_state_t prev_state;      /**< 前一個狀態 */
    
    // PS5 狀態
    ps5_status_t ps5_status;        /**< PS5 狀態 */
    
    // 計時器
    time_t state_enter_time;        /**< 進入當前狀態的時間 */
    time_t last_detect_time;        /**< 最後偵測時間 */
    
    // 標誌
    bool initialized;               /**< 是否已初始化 */
    bool running;                   /**< 是否運行中 */
    
    // 回調資料
    void *user_data;                /**< 使用者資料 */
    
} server_context_t;

/**
 * @brief 狀態變化回調函數類型
 * 
 * @param old_state 舊狀態
 * @param new_state 新狀態
 * @param user_data 使用者資料
 */
typedef void (*server_state_callback_t)(server_state_t old_state,
                                         server_state_t new_state,
                                         void *user_data);

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief 初始化 Server State Machine
 * 
 * @param ctx 伺服器上下文
 * @return 0 成功, <0 失敗
 */
int server_sm_init(server_context_t *ctx);

/**
 * @brief 設定狀態變化回調
 * 
 * @param ctx 伺服器上下文
 * @param callback 回調函數
 * @param user_data 使用者資料
 */
void server_sm_set_callback(server_context_t *ctx,
                             server_state_callback_t callback,
                             void *user_data);

/**
 * @brief 處理事件
 * 
 * @param ctx 伺服器上下文
 * @param event 事件
 * @return 0 成功, <0 失敗
 */
int server_sm_handle_event(server_context_t *ctx, server_event_t event);

/**
 * @brief 更新狀態機 (應定期呼叫)
 * 
 * @param ctx 伺服器上下文
 * @return 0 成功, <0 失敗
 */
int server_sm_update(server_context_t *ctx);

/**
 * @brief 更新 CEC 狀態
 * 
 * @param ctx 伺服器上下文
 * @param cec_state CEC 電源狀態
 * @return 0 成功, <0 失敗
 */
int server_sm_update_cec_state(server_context_t *ctx, 
                                ps5_power_state_t cec_state);

/**
 * @brief 更新 PS5 網路狀態
 * 
 * @param ctx 伺服器上下文
 * @param online 是否在線
 * @return 0 成功, <0 失敗
 */
int server_sm_update_network_state(server_context_t *ctx, bool online);

/**
 * @brief 更新 PS5 資訊
 * 
 * @param ctx 伺服器上下文
 * @param info PS5 資訊
 * @return 0 成功, <0 失敗
 */
int server_sm_update_ps5_info(server_context_t *ctx, const ps5_info_t *info);

/**
 * @brief 取得 PS5 綜合狀態字串
 * 
 * 根據 CEC 和網路狀態綜合判斷
 * 
 * @param ctx 伺服器上下文
 * @return 狀態字串: "on", "standby", "off", "unknown"
 */
const char* server_sm_get_ps5_status(const server_context_t *ctx);

/**
 * @brief 取得當前狀態
 * 
 * @param ctx 伺服器上下文
 * @return 當前狀態
 */
server_state_t server_sm_get_state(const server_context_t *ctx);

/**
 * @brief 檢查是否處於錯誤狀態
 * 
 * @param ctx 伺服器上下文
 * @return true 錯誤狀態, false 正常
 */
bool server_sm_is_error(const server_context_t *ctx);

/**
 * @brief 停止狀態機
 * 
 * @param ctx 伺服器上下文
 */
void server_sm_stop(server_context_t *ctx);

/**
 * @brief 清理資源
 * 
 * @param ctx 伺服器上下文
 */
void server_sm_cleanup(server_context_t *ctx);

/**
 * @brief 狀態轉換為字串
 * 
 * @param state 狀態
 * @return 狀態字串
 */
const char* server_state_to_string(server_state_t state);

/**
 * @brief 事件轉換為字串
 * 
 * @param event 事件
 * @return 事件字串
 */
const char* server_event_to_string(server_event_t event);

/**
 * @brief 錯誤碼轉換為字串
 * 
 * @param error 錯誤碼
 * @return 錯誤訊息字串
 */
const char* server_sm_error_string(int error);

/** @} */ // end of ServerStateMachine group

#ifdef __cplusplus
}
#endif

#endif // SERVER_STATE_MACHINE_H
