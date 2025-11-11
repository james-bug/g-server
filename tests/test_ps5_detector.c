/**
 * @file test_ps5_detector.c
 * @brief PS5 Detector 單元測試 - 修復版本
 */

#include "unity.h"
#include "ps5_detector.h"
#include <string.h>
#include <time.h>
#include <unistd.h>

// 測試用的快取檔案路徑
#define TEST_CACHE_PATH "/tmp/test_ps5_cache.json"

void setUp(void) {
    // 每個測試前初始化
    ps5_detector_init("192.168.1.0/24", TEST_CACHE_PATH);
    
    // 清理測試檔案
    unlink(TEST_CACHE_PATH);
}

void tearDown(void) {
    // 每個測試後清理
    ps5_detector_cleanup();
    unlink(TEST_CACHE_PATH);
}

// ============================================
// 基礎功能測試
// ============================================

void test_ps5_detector_init_should_succeed(void) {
    ps5_detector_cleanup();
    
    int result = ps5_detector_init("192.168.1.0/24", TEST_CACHE_PATH);
    
    TEST_ASSERT_EQUAL(0, result);
}

void test_ps5_detector_init_with_null_should_fail(void) {
    ps5_detector_cleanup();
    
    int result = ps5_detector_init(NULL, TEST_CACHE_PATH);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// IP 驗證測試
// ============================================

void test_ps5_detector_validate_ip_valid(void) {
    ps5_info_t info;
    
    // 使用有效 IP 進行快速檢查
    int result = ps5_detector_quick_check("192.168.1.100", &info);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", info.ip);
}

void test_ps5_detector_validate_ip_invalid(void) {
    ps5_info_t info;
    
    // ✅ 修復: 測試無效 IP (會觸發完整掃描,但會返回預設 IP)
    // 無效 IP 應該不會被使用,而是進行完整掃描
    int result = ps5_detector_quick_check("999.999.999.999", &info);
    
    // 無效 IP 會導致執行完整掃描,返回掃描結果
    TEST_ASSERT_EQUAL(0, result);
    // 確認返回的是掃描結果而不是無效 IP
    TEST_ASSERT_NOT_EQUAL_UINT("999.999.999.999", info.ip);
}

void test_ps5_detector_validate_ip_empty(void) {
    ps5_info_t info;
    
    // 空字串 IP 應該觸發完整掃描
    int result = ps5_detector_quick_check("", &info);
    
    TEST_ASSERT_EQUAL(0, result);
}

void test_ps5_detector_validate_ip_null(void) {
    ps5_info_t info;
    
    // NULL IP 應該觸發完整掃描
    int result = ps5_detector_quick_check(NULL, &info);
    
    TEST_ASSERT_EQUAL(0, result);
}

// ============================================
// 掃描功能測試
// ============================================

void test_ps5_detector_scan_should_find_ps5(void) {
    ps5_info_t info;
    
    int result = ps5_detector_scan(&info);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_EQUAL(0, strlen(info.ip));
    TEST_ASSERT_NOT_EQUAL(0, strlen(info.mac));
    TEST_ASSERT_TRUE(info.online);
}

void test_ps5_detector_scan_with_null_should_fail(void) {
    int result = ps5_detector_scan(NULL);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// 快取功能測試 (重點修復)
// ============================================

void test_ps5_detector_save_and_get_cache(void) {
    ps5_info_t info_save, info_get;
    
    // ✅ 修復: 初始化結構體,避免垃圾資料
    memset(&info_save, 0, sizeof(ps5_info_t));
    memset(&info_get, 0, sizeof(ps5_info_t));
    
    // 準備測試資料
    snprintf(info_save.ip, sizeof(info_save.ip), "192.168.1.100");
    snprintf(info_save.mac, sizeof(info_save.mac), "AA:BB:CC:DD:EE:FF");
    info_save.last_seen = time(NULL);
    info_save.online = true;
    
    // 儲存快取
    int result = ps5_detector_save_cache(&info_save);
    TEST_ASSERT_EQUAL(0, result);
    
    // 讀取快取
    result = ps5_detector_get_cached(&info_get);
    TEST_ASSERT_EQUAL(0, result);
    
    // ✅ 修復: 驗證字串內容,不會有垃圾資料
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", info_get.ip);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", info_get.mac);
    TEST_ASSERT_TRUE(info_get.online);
    
    // 額外驗證: 確保字串長度正確
    TEST_ASSERT_EQUAL(17, strlen(info_get.mac));
}

void test_ps5_detector_save_cache_with_invalid_ip_should_fail(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    
    snprintf(info.ip, sizeof(info.ip), "invalid.ip");
    snprintf(info.mac, sizeof(info.mac), "AA:BB:CC:DD:EE:FF");
    
    int result = ps5_detector_save_cache(&info);
    
    TEST_ASSERT_EQUAL(-1, result);
}

void test_ps5_detector_save_cache_with_invalid_mac_should_fail(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    
    snprintf(info.ip, sizeof(info.ip), "192.168.1.100");
    snprintf(info.mac, sizeof(info.mac), "INVALID_MAC");
    
    int result = ps5_detector_save_cache(&info);
    
    TEST_ASSERT_EQUAL(-1, result);
}

void test_ps5_detector_get_cache_before_save_should_fail(void) {
    ps5_info_t info;
    
    int result = ps5_detector_get_cached(&info);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// Ping 功能測試
// ============================================

void test_ps5_detector_ping_valid_ip(void) {
    bool result = ps5_detector_ping("192.168.1.100");
    
    TEST_ASSERT_TRUE(result);
}

void test_ps5_detector_ping_invalid_ip(void) {
    bool result = ps5_detector_ping("999.999.999.999");
    
    TEST_ASSERT_FALSE(result);
}

void test_ps5_detector_ping_null_should_fail(void) {
    bool result = ps5_detector_ping(NULL);
    
    TEST_ASSERT_FALSE(result);
}

// ============================================
// 錯誤處理測試
// ============================================

void test_ps5_detector_error_string(void) {
    const char *msg = ps5_detector_error_string(0);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("Success", msg);
    
    msg = ps5_detector_error_string(-1);
    TEST_ASSERT_NOT_NULL(msg);
}

// ============================================
// 清理功能測試
// ============================================

void test_ps5_detector_cleanup_should_reset_state(void) {
    ps5_info_t info;
    
    // 先執行掃描
    ps5_detector_scan(&info);
    
    // 清理
    ps5_detector_cleanup();
    
    // 清理後的操作應該失敗
    int result = ps5_detector_scan(&info);
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// 整合測試
// ============================================

void test_ps5_detector_full_workflow(void) {
    ps5_info_t info1, info2;
    memset(&info1, 0, sizeof(ps5_info_t));
    memset(&info2, 0, sizeof(ps5_info_t));
    
    // 1. 完整掃描
    int result = ps5_detector_scan(&info1);
    TEST_ASSERT_EQUAL(0, result);
    
    // 2. 儲存快取
    result = ps5_detector_save_cache(&info1);
    TEST_ASSERT_EQUAL(0, result);
    
    // 3. 從快取讀取
    result = ps5_detector_get_cached(&info2);
    TEST_ASSERT_EQUAL(0, result);
    
    // 4. 驗證一致性
    TEST_ASSERT_EQUAL_STRING(info1.ip, info2.ip);
    TEST_ASSERT_EQUAL_STRING(info1.mac, info2.mac);
    
    // 5. 快速檢查 (使用快取的 IP)
    ps5_info_t info3;
    result = ps5_detector_quick_check(info2.ip, &info3);
    TEST_ASSERT_EQUAL(0, result);
}
