/**
 * @file test_ps5_wake.c
 * @brief PS5 Wake 單元測試 - 僅測試 CEC 喚醒
 */

#include "unity.h"
#include "ps5_wake.h"
#include <string.h>

// 測試用常數
#define TEST_CEC_DEVICE "/dev/cec0"
#define TEST_PS5_IP "192.168.1.100"
#define TEST_PS5_MAC "AA:BB:CC:DD:EE:FF"

void setUp(void) {
    // 每個測試前初始化
    ps5_wake_init(TEST_CEC_DEVICE);
}

void tearDown(void) {
    // 每個測試後清理
    ps5_wake_cleanup();
}

// ============================================
// 初始化測試
// ============================================

void test_ps5_wake_init_should_succeed(void) {
    ps5_wake_cleanup();
    
    int result = ps5_wake_init(TEST_CEC_DEVICE);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_TRUE(ps5_wake_is_cec_available());
}

void test_ps5_wake_init_with_null_should_fail(void) {
    ps5_wake_cleanup();
    
    int result = ps5_wake_init(NULL);
    
    TEST_ASSERT_EQUAL(-1, result);
}

void test_ps5_wake_init_multiple_times(void) {
    ps5_wake_cleanup();
    
    int result1 = ps5_wake_init(TEST_CEC_DEVICE);
    int result2 = ps5_wake_init(TEST_CEC_DEVICE);
    
    TEST_ASSERT_EQUAL(0, result1);
    TEST_ASSERT_EQUAL(0, result2);
}

// ============================================
// CEC 喚醒測試
// ============================================

void test_ps5_wake_by_cec_should_succeed(void) {
    int result = ps5_wake_by_cec();
    
    TEST_ASSERT_EQUAL(0, result);
}

void test_ps5_wake_by_cec_without_init_should_fail(void) {
    ps5_wake_cleanup();
    
    int result = ps5_wake_by_cec();
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// 驗證測試
// ============================================

void test_ps5_wake_verify_should_succeed(void) {
    // 在測試模式下,ping 會成功
    bool result = ps5_wake_verify(TEST_PS5_IP, 5);
    
    TEST_ASSERT_TRUE(result);
}

void test_ps5_wake_verify_with_null_ip_should_fail(void) {
    bool result = ps5_wake_verify(NULL, 5);
    
    TEST_ASSERT_FALSE(result);
}

void test_ps5_wake_verify_with_zero_timeout_should_fail(void) {
    bool result = ps5_wake_verify(TEST_PS5_IP, 0);
    
    TEST_ASSERT_FALSE(result);
}

void test_ps5_wake_verify_with_negative_timeout_should_fail(void) {
    bool result = ps5_wake_verify(TEST_PS5_IP, -1);
    
    TEST_ASSERT_FALSE(result);
}

// ============================================
// 完整喚醒流程測試
// ============================================

void test_ps5_wake_should_succeed(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    
    snprintf(info.ip, sizeof(info.ip), TEST_PS5_IP);
    snprintf(info.mac, sizeof(info.mac), TEST_PS5_MAC);
    info.online = false;
    
    wake_result_t result = ps5_wake(&info, 5);
    
    TEST_ASSERT_EQUAL(WAKE_RESULT_SUCCESS, result);
}

void test_ps5_wake_with_null_info_should_fail(void) {
    wake_result_t result = ps5_wake(NULL, 5);
    
    TEST_ASSERT_EQUAL(WAKE_RESULT_NOT_INITIALIZED, result);
}

void test_ps5_wake_with_zero_timeout_should_fail(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    snprintf(info.ip, sizeof(info.ip), TEST_PS5_IP);
    
    wake_result_t result = ps5_wake(&info, 0);
    
    TEST_ASSERT_EQUAL(WAKE_RESULT_NOT_INITIALIZED, result);
}

void test_ps5_wake_without_init_should_fail(void) {
    ps5_wake_cleanup();
    
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    snprintf(info.ip, sizeof(info.ip), TEST_PS5_IP);
    
    wake_result_t result = ps5_wake(&info, 5);
    
    TEST_ASSERT_EQUAL(WAKE_RESULT_NOT_INITIALIZED, result);
}

// ============================================
// 重試機制測試
// ============================================

void test_ps5_wake_with_retry_should_succeed(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    snprintf(info.ip, sizeof(info.ip), TEST_PS5_IP);
    
    wake_result_t result = ps5_wake_with_retry(&info, 3, 5);
    
    TEST_ASSERT_EQUAL(WAKE_RESULT_SUCCESS, result);
}

void test_ps5_wake_with_retry_zero_retries_should_try_once(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    snprintf(info.ip, sizeof(info.ip), TEST_PS5_IP);
    
    // 即使 max_retries=0,也應該至少嘗試一次
    wake_result_t result = ps5_wake_with_retry(&info, 0, 5);
    
    TEST_ASSERT_EQUAL(WAKE_RESULT_SUCCESS, result);
}

void test_ps5_wake_with_retry_without_init_should_fail(void) {
    ps5_wake_cleanup();
    
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    snprintf(info.ip, sizeof(info.ip), TEST_PS5_IP);
    
    wake_result_t result = ps5_wake_with_retry(&info, 3, 5);
    
    TEST_ASSERT_EQUAL(WAKE_RESULT_NOT_INITIALIZED, result);
}

// ============================================
// CEC 可用性測試
// ============================================

void test_ps5_wake_is_cec_available_after_init(void) {
    bool available = ps5_wake_is_cec_available();
    
    TEST_ASSERT_TRUE(available);
}

void test_ps5_wake_is_cec_available_before_init(void) {
    ps5_wake_cleanup();
    
    bool available = ps5_wake_is_cec_available();
    
    TEST_ASSERT_FALSE(available);
}

// ============================================
// 字串轉換測試
// ============================================

void test_ps5_wake_result_string(void) {
    const char *msg;
    
    msg = ps5_wake_result_string(WAKE_RESULT_SUCCESS);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_TRUE(strlen(msg) > 0);
    
    msg = ps5_wake_result_string(WAKE_RESULT_CEC_ERROR);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_TRUE(strlen(msg) > 0);
    
    msg = ps5_wake_result_string(WAKE_RESULT_VERIFY_FAILED);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_TRUE(strlen(msg) > 0);
}

void test_ps5_wake_error_string(void) {
    const char *msg;
    
    msg = ps5_wake_error_string(0);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("Success", msg);
    
    msg = ps5_wake_error_string(-1);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_TRUE(strlen(msg) > 0);
    
    msg = ps5_wake_error_string(-2);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_TRUE(strlen(msg) > 0);
}

// ============================================
// 清理功能測試
// ============================================

void test_ps5_wake_cleanup_should_reset_state(void) {
    // 初始化後 CEC 可用
    TEST_ASSERT_TRUE(ps5_wake_is_cec_available());
    
    // 清理
    ps5_wake_cleanup();
    
    // 清理後 CEC 不可用
    TEST_ASSERT_FALSE(ps5_wake_is_cec_available());
    
    // 清理後的操作應該失敗
    int result = ps5_wake_by_cec();
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// 整合測試
// ============================================

void test_ps5_wake_full_workflow(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    
    // 1. 設定 PS5 資訊
    snprintf(info.ip, sizeof(info.ip), TEST_PS5_IP);
    snprintf(info.mac, sizeof(info.mac), TEST_PS5_MAC);
    info.online = false;
    
    // 2. 檢查 CEC 可用
    TEST_ASSERT_TRUE(ps5_wake_is_cec_available());
    
    // 3. 喚醒 PS5 (帶重試)
    wake_result_t result = ps5_wake_with_retry(&info, 3, 5);
    TEST_ASSERT_EQUAL(WAKE_RESULT_SUCCESS, result);
    
    // 4. 驗證 PS5 在線
    bool online = ps5_wake_verify(info.ip, 5);
    TEST_ASSERT_TRUE(online);
}

void test_ps5_wake_multiple_wake_attempts(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    snprintf(info.ip, sizeof(info.ip), TEST_PS5_IP);
    
    // 多次喚醒應該都成功
    wake_result_t result1 = ps5_wake(&info, 5);
    wake_result_t result2 = ps5_wake(&info, 5);
    wake_result_t result3 = ps5_wake(&info, 5);
    
    TEST_ASSERT_EQUAL(WAKE_RESULT_SUCCESS, result1);
    TEST_ASSERT_EQUAL(WAKE_RESULT_SUCCESS, result2);
    TEST_ASSERT_EQUAL(WAKE_RESULT_SUCCESS, result3);
}
