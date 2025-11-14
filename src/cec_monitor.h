/**
 * @file cec_monitor.h
 * @brief CEC Monitor - HDMI-CEC device monitoring for PS5
 * 
 * @author Gaming System Development Team
 * @date 2025-11-05
 * @version 1.0.0
 */

// ⭐ POSIX 標準定義 (必須在最前面!)
#define _POSIX_C_SOURCE 200809L

#ifndef CEC_MONITOR_H
#define CEC_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  Error Codes (參考 gaming-client 模式)
 * ============================================================ */

#define CEC_OK                          0
#define CEC_ERROR_NOT_INIT             -1
#define CEC_ERROR_DEVICE_NOT_FOUND     -2
#define CEC_ERROR_INVALID_PARAM        -3
#define CEC_ERROR_COMMAND_FAILED       -4
#define CEC_ERROR_TIMEOUT              -5
#define CEC_ERROR_UNKNOWN              -99

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief PS5 power state
 */
typedef enum {
    PS5_POWER_UNKNOWN = 0,    /**< Unknown state */
    PS5_POWER_ON,             /**< Power on */
    PS5_POWER_STANDBY,        /**< Standby mode */
    PS5_POWER_OFF,            /**< Power off */
} ps5_power_state_t;

/**
 * @brief CEC event types
 * ⚠️ 這些事件用於回調,與測試文件中的不同
 */
typedef enum {
    CEC_EVENT_NONE = 0,           /**< No event */
    CEC_EVENT_POWER_ON,           /**< PS5 powered on */
    CEC_EVENT_STANDBY,            /**< PS5 entered standby */
    CEC_EVENT_POWER_OFF,          /**< PS5 powered off */
    CEC_EVENT_POWER_CHANGE,       /**< Power state changed (generic) */
    CEC_EVENT_DEVICE_FOUND,       /**< Device found */
    CEC_EVENT_DEVICE_LOST,        /**< Device lost */
    CEC_EVENT_ERROR,              /**< Error occurred */
} cec_event_t;

/**
 * @brief CEC event callback function type
 */
typedef void (*cec_event_callback_t)(cec_event_t event, 
                                      ps5_power_state_t state, 
                                      void *user_data);

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Initialize the CEC monitor
 * @param device_path CEC device path (e.g., "/dev/cec0")
 * @return CEC_OK on success, negative error code on failure
 */
int cec_monitor_init(const char *device_path);

/**
 * @brief Set the CEC event callback
 * @param callback Callback function
 * @param user_data User data to pass to callback
 */
void cec_monitor_set_callback(cec_event_callback_t callback, void *user_data);

/**
 * @brief Run the CEC monitor (blocking)
 * @return CEC_OK on success, negative error code on failure
 */
int cec_monitor_run(void);

/**
 * @brief Process CEC events (non-blocking, single iteration)
 * @param timeout_ms Timeout in milliseconds
 * @return CEC_OK on success, negative error code on failure
 */
int cec_monitor_process(int timeout_ms);

/**
 * @brief Stop the CEC monitor
 */
void cec_monitor_stop(void);

/**
 * @brief Get the current PS5 power state
 * @return Current PS5 power state
 */
ps5_power_state_t cec_monitor_get_power_state(void);

/**
 * @brief Get the last known PS5 power state (alias for compatibility)
 * @return Last known PS5 power state
 */
ps5_power_state_t cec_monitor_get_last_state(void);

/**
 * @brief Query and update PS5 power state
 * @param state Pointer to store the power state (can be NULL)
 * @return CEC_OK on success, negative error code on failure
 */
int cec_monitor_query_state(ps5_power_state_t *state);

/**
 * @brief Get the timestamp of last state update
 * @return Timestamp of last update (Unix time)
 */
time_t cec_monitor_get_last_update(void);

/**
 * @brief Check if CEC device is available
 * @param device_path CEC device path
 * @return true if device is available, false otherwise
 */
bool cec_monitor_device_available(const char *device_path);

/**
 * @brief Clean up CEC monitor resources
 */
void cec_monitor_cleanup(void);

/* ============================================================
 *  String Conversion Functions
 * ============================================================ */

/**
 * @brief Convert PS5 power state to string
 * @param state PS5 power state
 * @return String representation
 */
const char* ps5_power_state_to_string(ps5_power_state_t state);

/**
 * @brief Convert PS5 power state to string (alias for compatibility)
 * @param state PS5 power state
 * @return String representation
 */
const char* cec_monitor_state_string(ps5_power_state_t state);

/**
 * @brief Convert CEC event to string
 * @param event CEC event
 * @return String representation
 */
const char* cec_event_to_string(cec_event_t event);

/**
 * @brief Convert CEC event to string (alias for compatibility)
 * @param event CEC event
 * @return String representation
 */
const char* cec_monitor_event_string(cec_event_t event);

/**
 * @brief Convert error code to string
 * @param error Error code
 * @return Error message string
 */
const char* cec_monitor_error_string(int error);

#ifdef __cplusplus
}
#endif

#endif /* CEC_MONITOR_H */
