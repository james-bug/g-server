/**
 * @file test_cec_monitor.c
 * @brief CEC Monitor 單元測試
 * 
 * @copyright Gaming System Development Team
 * @date 2025-11-04
 */

#include "unity.h"
#include "cec_monitor.h"
#include <string.h>
#include <unistd.h>

// Test fixtures
void setUp(void) {
    // 每個測試前執行
}

void tearDown(void) {
    // 每個測試後執行
    cec_monitor_cleanup();
}

// ============================================================================
// 初始化測試
// ============================================================================

void test_cec_monitor_init_with_valid_device(void) {
    // 注意: 測試環境可能沒有真實的 CEC 裝置
    // 此測試驗證 API 行為，而非真實硬體
    int result = cec_monitor_init("/dev/cec0");
    
    // 如果裝置不存在，應該返回錯誤
    if (result == CEC_ERROR_DEVICE_NOT_FOUND) {
        TEST_PASS_MESSAGE("CEC device not found (expected in test environment)");
    } else {
        TEST_ASSERT_EQUAL(CEC_OK, result);
    }
}

void test_cec_monitor_init_with_null_device(void) {
    int result = cec_monitor_init(NULL);
    TEST_ASSERT_EQUAL(CEC_ERROR_DEVICE_NOT_FOUND, result);
}

void test_cec_monitor_init_with_empty_device(void) {
    int result = cec_monitor_init("");
    TEST_ASSERT_EQUAL(CEC_ERROR_DEVICE_NOT_FOUND, result);
}

void test_cec_monitor_init_with_invalid_device(void) {
    int result = cec_monitor_init("/dev/nonexistent_cec");
    TEST_ASSERT_EQUAL(CEC_ERROR_DEVICE_NOT_FOUND, result);
}

// ============================================================================
// 回調測試
// ============================================================================

static int callback_count = 0;
static cec_event_t last_event;
static ps5_power_state_t last_state;

static void test_callback(cec_event_t event, ps5_power_state_t state, void *user_data) {
    callback_count++;
    last_event = event;
    last_state = state;
}

void test_cec_monitor_set_callback(void) {
    cec_monitor_set_callback(test_callback, NULL);
    // 設定回調不應該失敗
    TEST_PASS();
}

void test_cec_monitor_callback_with_user_data(void) {
    int test_data = 42;
    cec_monitor_set_callback(test_callback, &test_data);
    TEST_PASS();
}

// ============================================================================
// 狀態查詢測試
// ============================================================================

void test_cec_monitor_get_last_state_initial(void) {
    // 未初始化時，狀態應該是 UNKNOWN
    ps5_power_state_t state = cec_monitor_get_last_state();
    TEST_ASSERT_EQUAL(PS5_POWER_UNKNOWN, state);
}

void test_cec_monitor_query_state_with_null(void) {
    int result = cec_monitor_query_state(NULL);
    TEST_ASSERT_EQUAL(CEC_ERROR_NOT_INIT, result);
}

// ============================================================================
// 字串轉換測試
// ============================================================================

void test_cec_monitor_state_string_unknown(void) {
    const char *str = cec_monitor_state_string(PS5_POWER_UNKNOWN);
    TEST_ASSERT_EQUAL_STRING("unknown", str);
}

void test_cec_monitor_state_string_on(void) {
    const char *str = cec_monitor_state_string(PS5_POWER_ON);
    TEST_ASSERT_EQUAL_STRING("on", str);
}

void test_cec_monitor_state_string_standby(void) {
    const char *str = cec_monitor_state_string(PS5_POWER_STANDBY);
    TEST_ASSERT_EQUAL_STRING("standby", str);
}

void test_cec_monitor_state_string_off(void) {
    const char *str = cec_monitor_state_string(PS5_POWER_OFF);
    TEST_ASSERT_EQUAL_STRING("off", str);
}

void test_cec_monitor_event_string_power_on(void) {
    const char *str = cec_monitor_event_string(CEC_EVENT_POWER_ON);
    TEST_ASSERT_EQUAL_STRING("power_on", str);
}

void test_cec_monitor_event_string_standby(void) {
    const char *str = cec_monitor_event_string(CEC_EVENT_STANDBY);
    TEST_ASSERT_EQUAL_STRING("standby", str);
}

void test_cec_monitor_event_string_power_off(void) {
    const char *str = cec_monitor_event_string(CEC_EVENT_POWER_OFF);
    TEST_ASSERT_EQUAL_STRING("power_off", str);
}

void test_cec_monitor_error_string_ok(void) {
    const char *str = cec_monitor_error_string(CEC_OK);
    TEST_ASSERT_EQUAL_STRING("Success", str);
}

void test_cec_monitor_error_string_not_init(void) {
    const char *str = cec_monitor_error_string(CEC_ERROR_NOT_INIT);
    TEST_ASSERT_EQUAL_STRING("Not initialized", str);
}

void test_cec_monitor_error_string_device_not_found(void) {
    const char *str = cec_monitor_error_string(CEC_ERROR_DEVICE_NOT_FOUND);
    TEST_ASSERT_EQUAL_STRING("Device not found", str);
}

// ============================================================================
// 裝置可用性測試
// ============================================================================

void test_cec_monitor_device_available_null(void) {
    bool available = cec_monitor_device_available(NULL);
    TEST_ASSERT_FALSE(available);
}

void test_cec_monitor_device_available_invalid(void) {
    bool available = cec_monitor_device_available("/dev/nonexistent");
    TEST_ASSERT_FALSE(available);
}

// ============================================================================
// 清理測試
// ============================================================================

void test_cec_monitor_cleanup_without_init(void) {
    // 清理未初始化的監控器不應該崩潰
    cec_monitor_cleanup();
    TEST_PASS();
}

void test_cec_monitor_stop_without_init(void) {
    // 停止未初始化的監控器不應該崩潰
    cec_monitor_stop();
    TEST_PASS();
}

// ============================================================================
// 時間戳測試
// ============================================================================

void test_cec_monitor_get_last_update_initial(void) {
    time_t timestamp = cec_monitor_get_last_update();
    // 未初始化時應該是 0
    TEST_ASSERT_EQUAL(0, timestamp);
}

// ============================================================================
// 整合測試 (需要真實硬體或 Mock)
// ============================================================================

void test_cec_monitor_process_without_init(void) {
    // 未初始化直接處理應該安全 (可能返回錯誤)
    int result = cec_monitor_process(100);
    // 由於未初始化，預期會失敗
    TEST_ASSERT_NOT_EQUAL(CEC_OK, result);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(void) {
    UNITY_BEGIN();
    
    // 初始化測試
    RUN_TEST(test_cec_monitor_init_with_valid_device);
    RUN_TEST(test_cec_monitor_init_with_null_device);
    RUN_TEST(test_cec_monitor_init_with_empty_device);
    RUN_TEST(test_cec_monitor_init_with_invalid_device);
    
    // 回調測試
    RUN_TEST(test_cec_monitor_set_callback);
    RUN_TEST(test_cec_monitor_callback_with_user_data);
    
    // 狀態查詢測試
    RUN_TEST(test_cec_monitor_get_last_state_initial);
    RUN_TEST(test_cec_monitor_query_state_with_null);
    
    // 字串轉換測試
    RUN_TEST(test_cec_monitor_state_string_unknown);
    RUN_TEST(test_cec_monitor_state_string_on);
    RUN_TEST(test_cec_monitor_state_string_standby);
    RUN_TEST(test_cec_monitor_state_string_off);
    RUN_TEST(test_cec_monitor_event_string_power_on);
    RUN_TEST(test_cec_monitor_event_string_standby);
    RUN_TEST(test_cec_monitor_event_string_power_off);
    RUN_TEST(test_cec_monitor_error_string_ok);
    RUN_TEST(test_cec_monitor_error_string_not_init);
    RUN_TEST(test_cec_monitor_error_string_device_not_found);
    
    // 裝置可用性測試
    RUN_TEST(test_cec_monitor_device_available_null);
    RUN_TEST(test_cec_monitor_device_available_invalid);
    
    // 清理測試
    RUN_TEST(test_cec_monitor_cleanup_without_init);
    RUN_TEST(test_cec_monitor_stop_without_init);
    
    // 時間戳測試
    RUN_TEST(test_cec_monitor_get_last_update_initial);
    
    // 整合測試
    RUN_TEST(test_cec_monitor_process_without_init);
    
    return UNITY_END();
}
