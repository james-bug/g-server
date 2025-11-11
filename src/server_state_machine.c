/**
 * @file server_state_machine.c
 * @brief Server State Machine Implementation
 */

// POSIX headers
#define _POSIX_C_SOURCE 200809L

#include "server_state_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 *  Internal Helper Functions
 * ============================================================ */

/**
 * @brief 變更狀態
 */
static void change_state(server_context_t *ctx, server_state_t new_state) {
    if (ctx->state != new_state) {
        ctx->prev_state = ctx->state;
        ctx->state = new_state;
        ctx->state_enter_time = time(NULL);
        
        // TODO: 觸發狀態變化回調 (生產環境實作)
    }
}

/**
 * @brief 檢查狀態是否超時
 */
static bool is_state_timeout(const server_context_t *ctx) {
    time_t now = time(NULL);
    time_t elapsed = now - ctx->state_enter_time;
    return (elapsed >= SERVER_STATE_TIMEOUT_SEC);
}

/**
 * @brief 判斷 PS5 綜合狀態
 */
static const char* determine_ps5_status(ps5_power_state_t cec_state, 
                                        bool network_online) {
    // 優先級: CEC 狀態 > 網路狀態
    
    if (cec_state == PS5_POWER_ON && network_online) {
        return "on";
    } else if (cec_state == PS5_POWER_ON && !network_online) {
        // CEC 顯示開機但網路未連線,可能正在啟動
        return "starting";
    } else if (cec_state == PS5_POWER_STANDBY) {
        return "standby";
    } else if (cec_state == PS5_POWER_OFF) {
        return "off";
    } else if (network_online) {
        // CEC 狀態未知但網路在線
        return "on";
    } else {
        return "unknown";
    }
}

/* ============================================================
 *  Public Function Implementations
 * ============================================================ */

/**
 * @brief 初始化 Server State Machine
 */
int server_sm_init(server_context_t *ctx) {
    if (ctx == NULL) {
        return -1;
    }
    
    // 初始化上下文
    memset(ctx, 0, sizeof(server_context_t));
    
    ctx->state = SERVER_STATE_INIT;
    ctx->prev_state = SERVER_STATE_INIT;
    ctx->state_enter_time = time(NULL);
    ctx->last_detect_time = 0;
    ctx->initialized = true;
    ctx->running = false;
    
    // 初始化 PS5 狀態
    ctx->ps5_status.cec_state = PS5_POWER_UNKNOWN;
    ctx->ps5_status.network_online = false;
    ctx->ps5_status.last_update = time(NULL);
    memset(&ctx->ps5_status.info, 0, sizeof(ps5_info_t));
    
    // 轉換到 IDLE 狀態
    change_state(ctx, SERVER_STATE_IDLE);
    ctx->running = true;
    
    return 0;
}

/**
 * @brief 設定狀態變化回調
 */
void server_sm_set_callback(server_context_t *ctx,
                             server_state_callback_t callback,
                             void *user_data) {
    if (ctx == NULL) {
        return;
    }
    
    // TODO: 儲存回調 (生產環境實作)
    ctx->user_data = user_data;
    (void)callback;  // 避免未使用警告
}

/**
 * @brief 處理事件
 */
int server_sm_handle_event(server_context_t *ctx, server_event_t event) {
    if (ctx == NULL || !ctx->initialized) {
        return -1;
    }
    
    switch (ctx->state) {
        case SERVER_STATE_IDLE:
            if (event == SERVER_EVENT_CEC_CHANGE) {
                change_state(ctx, SERVER_STATE_MONITORING);
            } else if (event == SERVER_EVENT_CLIENT_QUERY) {
                change_state(ctx, SERVER_STATE_QUERYING);
            } else if (event == SERVER_EVENT_WAKE_REQUEST) {
                change_state(ctx, SERVER_STATE_WAKING);
            }
            break;
            
        case SERVER_STATE_MONITORING:
            if (event == SERVER_EVENT_COMPLETED) {
                change_state(ctx, SERVER_STATE_BROADCASTING);
            } else if (event == SERVER_EVENT_ERROR) {
                change_state(ctx, SERVER_STATE_ERROR);
            }
            break;
            
        case SERVER_STATE_DETECTING:
            if (event == SERVER_EVENT_COMPLETED) {
                change_state(ctx, SERVER_STATE_IDLE);
            } else if (event == SERVER_EVENT_DETECT_TIMEOUT) {
                change_state(ctx, SERVER_STATE_ERROR);
            }
            break;
            
        case SERVER_STATE_QUERYING:
            if (event == SERVER_EVENT_COMPLETED) {
                change_state(ctx, SERVER_STATE_IDLE);
            }
            break;
            
        case SERVER_STATE_WAKING:
            if (event == SERVER_EVENT_COMPLETED) {
                change_state(ctx, SERVER_STATE_MONITORING);
            } else if (event == SERVER_EVENT_ERROR) {
                change_state(ctx, SERVER_STATE_ERROR);
            }
            break;
            
        case SERVER_STATE_BROADCASTING:
            if (event == SERVER_EVENT_COMPLETED) {
                change_state(ctx, SERVER_STATE_IDLE);
            }
            break;
            
        case SERVER_STATE_ERROR:
            if (event == SERVER_EVENT_NONE) {
                // 重置到 IDLE
                change_state(ctx, SERVER_STATE_IDLE);
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

/**
 * @brief 更新狀態機
 */
int server_sm_update(server_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized || !ctx->running) {
        return -1;
    }
    
    time_t now = time(NULL);
    
    // 檢查狀態超時
    if (is_state_timeout(ctx)) {
        if (ctx->state != SERVER_STATE_IDLE && 
            ctx->state != SERVER_STATE_ERROR) {
            // 超時,轉到錯誤狀態
            change_state(ctx, SERVER_STATE_ERROR);
        }
    }
    
    // 定期偵測 PS5 (每 60 秒)
    if (ctx->state == SERVER_STATE_IDLE) {
        if ((now - ctx->last_detect_time) >= SERVER_DETECT_INTERVAL_SEC) {
            ctx->last_detect_time = now;
            change_state(ctx, SERVER_STATE_DETECTING);
            // TODO: 觸發 PS5 偵測 (生產環境實作)
        }
    }
    
    return 0;
}

/**
 * @brief 更新 CEC 狀態
 */
int server_sm_update_cec_state(server_context_t *ctx, 
                                ps5_power_state_t cec_state) {
    if (ctx == NULL || !ctx->initialized) {
        return -1;
    }
    
    if (ctx->ps5_status.cec_state != cec_state) {
        ctx->ps5_status.cec_state = cec_state;
        ctx->ps5_status.last_update = time(NULL);
        
        // 觸發狀態變化事件
        if (ctx->state == SERVER_STATE_IDLE) {
            server_sm_handle_event(ctx, SERVER_EVENT_CEC_CHANGE);
        }
    }
    
    return 0;
}

/**
 * @brief 更新 PS5 網路狀態
 */
int server_sm_update_network_state(server_context_t *ctx, bool online) {
    if (ctx == NULL || !ctx->initialized) {
        return -1;
    }
    
    if (ctx->ps5_status.network_online != online) {
        ctx->ps5_status.network_online = online;
        ctx->ps5_status.last_update = time(NULL);
    }
    
    return 0;
}

/**
 * @brief 更新 PS5 資訊
 */
int server_sm_update_ps5_info(server_context_t *ctx, const ps5_info_t *info) {
    if (ctx == NULL || !ctx->initialized || info == NULL) {
        return -1;
    }
    
    memcpy(&ctx->ps5_status.info, info, sizeof(ps5_info_t));
    ctx->ps5_status.network_online = info->online;
    ctx->ps5_status.last_update = time(NULL);
    
    return 0;
}

/**
 * @brief 取得 PS5 綜合狀態字串
 */
const char* server_sm_get_ps5_status(const server_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return "unknown";
    }
    
    return determine_ps5_status(ctx->ps5_status.cec_state,
                               ctx->ps5_status.network_online);
}

/**
 * @brief 取得當前狀態
 */
server_state_t server_sm_get_state(const server_context_t *ctx) {
    if (ctx == NULL) {
        return SERVER_STATE_ERROR;
    }
    return ctx->state;
}

/**
 * @brief 檢查是否處於錯誤狀態
 */
bool server_sm_is_error(const server_context_t *ctx) {
    if (ctx == NULL) {
        return true;
    }
    return (ctx->state == SERVER_STATE_ERROR);
}

/**
 * @brief 停止狀態機
 */
void server_sm_stop(server_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->running = false;
    change_state(ctx, SERVER_STATE_IDLE);
}

/**
 * @brief 清理資源
 */
void server_sm_cleanup(server_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->running = false;
    memset(ctx, 0, sizeof(server_context_t));
}

/**
 * @brief 狀態轉換為字串
 */
const char* server_state_to_string(server_state_t state) {
    switch (state) {
        case SERVER_STATE_INIT:         return "INIT";
        case SERVER_STATE_IDLE:         return "IDLE";
        case SERVER_STATE_MONITORING:   return "MONITORING";
        case SERVER_STATE_DETECTING:    return "DETECTING";
        case SERVER_STATE_QUERYING:     return "QUERYING";
        case SERVER_STATE_WAKING:       return "WAKING";
        case SERVER_STATE_BROADCASTING: return "BROADCASTING";
        case SERVER_STATE_ERROR:        return "ERROR";
        default:                        return "UNKNOWN";
    }
}

/**
 * @brief 事件轉換為字串
 */
const char* server_event_to_string(server_event_t event) {
    switch (event) {
        case SERVER_EVENT_NONE:          return "NONE";
        case SERVER_EVENT_CEC_CHANGE:    return "CEC_CHANGE";
        case SERVER_EVENT_CLIENT_QUERY:  return "CLIENT_QUERY";
        case SERVER_EVENT_WAKE_REQUEST:  return "WAKE_REQUEST";
        case SERVER_EVENT_DETECT_TIMEOUT: return "DETECT_TIMEOUT";
        case SERVER_EVENT_COMPLETED:     return "COMPLETED";
        case SERVER_EVENT_ERROR:         return "ERROR";
        default:                         return "UNKNOWN";
    }
}

/**
 * @brief 錯誤碼轉換為字串
 */
const char* server_sm_error_string(int error) {
    switch (error) {
        case 0:  return "Success";
        case -1: return "Not initialized or invalid parameters";
        case -2: return "State transition error";
        case -3: return "Timeout";
        default: return "Unknown error";
    }
}
