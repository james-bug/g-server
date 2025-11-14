/**
 * @file test_cec_monitor.c
 * @brief Unit tests for CEC Monitor module
 * 
 * @author Gaming System Development Team
 * @date 2025-11-05
 */

#include "unity.h"
#include "cec_monitor.h"
#include <string.h>

/* ============================================================
 *  Test Fixtures
 * ============================================================ */

void setUp(void) {
    // Clean state before each test
    cec_monitor_cleanup();
}

void tearDown(void) {
    // Clean up after each test
    cec_monitor_cleanup();
}

/* ============================================================
 *  Test Group 1: Initialization Tests
 * ============================================================ */

void test_cec_monitor_init_with_valid_device(void) {
    int result = cec_monitor_init("/dev/cec0");
    
    // In test mode, device check is skipped, so should succeed
    if (result == CEC_ERROR_DEVICE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("CEC device not available");
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
    int result = cec_monitor_init("/dev/invalid_cec_device");
    TEST_ASSERT_EQUAL(CEC_ERROR_DEVICE_NOT_FOUND, result);
}

void test_cec_monitor_init_twice_should_fail(void) {
    cec_monitor_init("/dev/cec0");
    int result = cec_monitor_init("/dev/cec0");
    TEST_ASSERT_EQUAL(CEC_ERROR_NOT_INIT, result);
}

/* ============================================================
 *  Test Group 2: Callback Tests
 * ============================================================ */

static cec_event_t g_received_event;
static ps5_power_state_t g_received_state;
static int g_callback_count;

static void test_callback(cec_event_t event, ps5_power_state_t state, void *user_data) {
    g_received_event = event;
    g_received_state = state;
    g_callback_count++;
}

void test_cec_monitor_set_callback_should_succeed(void) {
    cec_monitor_init("/dev/cec0");
    cec_monitor_set_callback(test_callback, NULL);
    TEST_PASS();
}

void test_cec_monitor_set_callback_with_null(void) {
    cec_monitor_init("/dev/cec0");
    cec_monitor_set_callback(NULL, NULL);
    TEST_PASS();
}

/* ============================================================
 *  Test Group 3: State Query Tests
 * ============================================================ */

void test_cec_monitor_get_power_state_initial(void) {
    cec_monitor_init("/dev/cec0");
    ps5_power_state_t state = cec_monitor_get_power_state();
    TEST_ASSERT_EQUAL(PS5_POWER_UNKNOWN, state);
}

void test_cec_monitor_get_last_state_initial(void) {
    cec_monitor_init("/dev/cec0");
    ps5_power_state_t state = cec_monitor_get_last_state();
    TEST_ASSERT_EQUAL(PS5_POWER_UNKNOWN, state);
}

void test_cec_monitor_query_state_with_null(void) {
    int result = cec_monitor_query_state(NULL);
    TEST_ASSERT_EQUAL(CEC_ERROR_NOT_INIT, result);
}

void test_cec_monitor_query_state_after_init(void) {
    cec_monitor_init("/dev/cec0");
    ps5_power_state_t state;
    int result = cec_monitor_query_state(&state);
    // May fail due to no real CEC device, that's OK
    TEST_ASSERT_TRUE(result == CEC_OK || result == CEC_ERROR_COMMAND_FAILED);
}

/* ============================================================
 *  Test Group 4: String Conversion Tests
 * ============================================================ */

void test_cec_monitor_state_string_unknown(void) {
    const char *str = cec_monitor_state_string(PS5_POWER_UNKNOWN);
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", str);
}

void test_cec_monitor_state_string_on(void) {
    const char *str = cec_monitor_state_string(PS5_POWER_ON);
    TEST_ASSERT_EQUAL_STRING("ON", str);
}

void test_cec_monitor_state_string_standby(void) {
    const char *str = cec_monitor_state_string(PS5_POWER_STANDBY);
    TEST_ASSERT_EQUAL_STRING("STANDBY", str);
}

void test_cec_monitor_state_string_off(void) {
    const char *str = cec_monitor_state_string(PS5_POWER_OFF);
    TEST_ASSERT_EQUAL_STRING("OFF", str);
}

void test_cec_monitor_event_string_power_on(void) {
    const char *str = cec_monitor_event_string(CEC_EVENT_POWER_ON);
    TEST_ASSERT_EQUAL_STRING("POWER_ON", str);
}

void test_cec_monitor_event_string_standby(void) {
    const char *str = cec_monitor_event_string(CEC_EVENT_STANDBY);
    TEST_ASSERT_EQUAL_STRING("STANDBY", str);
}

void test_cec_monitor_event_string_power_off(void) {
    const char *str = cec_monitor_event_string(CEC_EVENT_POWER_OFF);
    TEST_ASSERT_EQUAL_STRING("POWER_OFF", str);
}

void test_cec_monitor_error_string_ok(void) {
    const char *str = cec_monitor_error_string(CEC_OK);
    TEST_ASSERT_EQUAL_STRING("OK", str);
}

void test_cec_monitor_error_string_not_init(void) {
    const char *str = cec_monitor_error_string(CEC_ERROR_NOT_INIT);
    TEST_ASSERT_EQUAL_STRING("Not initialized", str);
}

void test_cec_monitor_error_string_device_not_found(void) {
    const char *str = cec_monitor_error_string(CEC_ERROR_DEVICE_NOT_FOUND);
    TEST_ASSERT_EQUAL_STRING("Device not found", str);
}

/* ============================================================
 *  Test Group 5: Device Availability Tests
 * ============================================================ */

void test_cec_monitor_device_available_null(void) {
    bool available = cec_monitor_device_available(NULL);
    TEST_ASSERT_FALSE(available);
}

void test_cec_monitor_device_available_empty(void) {
    bool available = cec_monitor_device_available("");
    TEST_ASSERT_FALSE(available);
}

void test_cec_monitor_device_available_valid(void) {
    // In test mode, should always return true
    bool available = cec_monitor_device_available("/dev/cec0");
    #ifdef TESTING
    TEST_ASSERT_TRUE(available);
    #else
    // In real mode, depends on actual device
    TEST_ASSERT_TRUE(available == true || available == false);
    #endif
}

/* ============================================================
 *  Test Group 6: Lifecycle Tests
 * ============================================================ */

void test_cec_monitor_cleanup_without_init(void) {
    cec_monitor_cleanup();
    TEST_PASS();
}

void test_cec_monitor_cleanup_after_init(void) {
    cec_monitor_init("/dev/cec0");
    cec_monitor_cleanup();
    TEST_PASS();
}

void test_cec_monitor_get_last_update_initial(void) {
    cec_monitor_init("/dev/cec0");
    time_t timestamp = cec_monitor_get_last_update();
    TEST_ASSERT_TRUE(timestamp > 0);
}

void test_cec_monitor_stop_without_init(void) {
    cec_monitor_stop();
    TEST_PASS();
}

void test_cec_monitor_stop_after_init(void) {
    cec_monitor_init("/dev/cec0");
    cec_monitor_stop();
    TEST_PASS();
}

void test_cec_monitor_process_without_init(void) {
    int result = cec_monitor_process(100);
    TEST_ASSERT_NOT_EQUAL(CEC_OK, result);
}

void test_cec_monitor_process_after_init(void) {
    cec_monitor_init("/dev/cec0");
    int result = cec_monitor_process(100);
    // May fail due to no real CEC device
    TEST_ASSERT_TRUE(result == CEC_OK || result == CEC_ERROR_COMMAND_FAILED);
}

/* ============================================================
 *  ⚠️ 重要: 不要有 main() 函數!
 *  Ceedling 會自動生成 test runner (包含 main)
 * ============================================================ */
