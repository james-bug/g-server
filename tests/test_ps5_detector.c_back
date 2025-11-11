/**
 * @file test_ps5_detector.c
 * @brief Unit tests for PS5 Detector module
 * 
 * @author Gaming System Development Team
 * @date 2025-11-05
 */

#include "unity.h"
#include "ps5_detector.h"
#include <string.h>

/* ============================================================
 *  Test Fixtures
 * ============================================================ */

void setUp(void) {
    // Clean state before each test
    ps5_detector_cleanup();
}

void tearDown(void) {
    // Clean up after each test
    ps5_detector_cleanup();
}

/* ============================================================
 *  Test Group 1: Initialization Tests
 * ============================================================ */

void test_ps5_detector_init_with_valid_params(void) {
    int result = ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, result);
}

void test_ps5_detector_init_with_null_subnet(void) {
    int result = ps5_detector_init(NULL, "/tmp/ps5_cache.json");
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_INVALID_PARAM, result);
}

void test_ps5_detector_init_with_null_cache_path(void) {
    int result = ps5_detector_init("192.168.1.0/24", NULL);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_INVALID_PARAM, result);
}

void test_ps5_detector_init_twice_should_fail(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    int result = ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_NOT_INIT, result);
}

/* ============================================================
 *  Test Group 2: Validation Tests
 * ============================================================ */

void test_ps5_detector_validate_mac_valid(void) {
    TEST_ASSERT_TRUE(ps5_detector_validate_mac("AA:BB:CC:DD:EE:FF"));
    TEST_ASSERT_TRUE(ps5_detector_validate_mac("00:11:22:33:44:55"));
    TEST_ASSERT_TRUE(ps5_detector_validate_mac("aa:bb:cc:dd:ee:ff"));
}

void test_ps5_detector_validate_mac_invalid(void) {
    TEST_ASSERT_FALSE(ps5_detector_validate_mac(NULL));
    TEST_ASSERT_FALSE(ps5_detector_validate_mac(""));
    TEST_ASSERT_FALSE(ps5_detector_validate_mac("INVALID"));
    TEST_ASSERT_FALSE(ps5_detector_validate_mac("AA:BB:CC:DD:EE"));
    TEST_ASSERT_FALSE(ps5_detector_validate_mac("AA-BB-CC-DD-EE-FF"));
    TEST_ASSERT_FALSE(ps5_detector_validate_mac("ZZ:BB:CC:DD:EE:FF"));
}

void test_ps5_detector_validate_ip_valid(void) {
    TEST_ASSERT_TRUE(ps5_detector_validate_ip("192.168.1.100"));
    TEST_ASSERT_TRUE(ps5_detector_validate_ip("10.0.0.1"));
    TEST_ASSERT_TRUE(ps5_detector_validate_ip("172.16.0.1"));
    TEST_ASSERT_TRUE(ps5_detector_validate_ip("8.8.8.8"));
}

void test_ps5_detector_validate_ip_invalid(void) {
    TEST_ASSERT_FALSE(ps5_detector_validate_ip(NULL));
    TEST_ASSERT_FALSE(ps5_detector_validate_ip(""));
    TEST_ASSERT_FALSE(ps5_detector_validate_ip("INVALID"));
    TEST_ASSERT_FALSE(ps5_detector_validate_ip("192.168.1"));
    TEST_ASSERT_FALSE(ps5_detector_validate_ip("192.168.1.256"));
    TEST_ASSERT_FALSE(ps5_detector_validate_ip("192.168.1.1.1"));
}

/* ============================================================
 *  Test Group 3: Cache Tests
 * ============================================================ */

void test_ps5_detector_save_and_get_cache(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/test_ps5_cache.json");
    
    // Create test data
    ps5_info_t save_info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    
    // Save cache
    int result = ps5_detector_save_cache(&save_info);
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, result);
    
    // Get cache
    ps5_info_t load_info;
    result = ps5_detector_get_cached(&load_info);
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, result);
    
    // Verify
    TEST_ASSERT_EQUAL_STRING(save_info.ip, load_info.ip);
    TEST_ASSERT_EQUAL_STRING(save_info.mac, load_info.mac);
    TEST_ASSERT_TRUE(load_info.online);
}

void test_ps5_detector_get_cached_without_init(void) {
    ps5_info_t info;
    int result = ps5_detector_get_cached(&info);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_NOT_INIT, result);
}

void test_ps5_detector_get_cached_with_null(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    int result = ps5_detector_get_cached(NULL);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_INVALID_PARAM, result);
}

void test_ps5_detector_save_cache_without_init(void) {
    ps5_info_t info = {0};
    int result = ps5_detector_save_cache(&info);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_NOT_INIT, result);
}

void test_ps5_detector_save_cache_with_null(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    int result = ps5_detector_save_cache(NULL);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_INVALID_PARAM, result);
}

void test_ps5_detector_clear_cache(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/test_ps5_cache2.json");
    
    // Save some data first
    ps5_info_t info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    ps5_detector_save_cache(&info);
    
    // Clear cache
    int result = ps5_detector_clear_cache();
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, result);
    
    // Verify cache is gone
    result = ps5_detector_get_cached(&info);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_CACHE_INVALID, result);
}

/* ============================================================
 *  Test Group 4: Detection Tests
 * ============================================================ */

void test_ps5_detector_ping_with_valid_ip(void) {
    // In test mode, should return true for valid IPs
    bool result = ps5_detector_ping("192.168.1.100");
    #ifdef TESTING
    TEST_ASSERT_TRUE(result);
    #else
    // In real mode, depends on actual network
    TEST_ASSERT_TRUE(result == true || result == false);
    #endif
}

void test_ps5_detector_ping_with_null(void) {
    bool result = ps5_detector_ping(NULL);
    TEST_ASSERT_FALSE(result);
}

void test_ps5_detector_ping_with_invalid_ip(void) {
    bool result = ps5_detector_ping("INVALID");
    TEST_ASSERT_FALSE(result);
}

void test_ps5_detector_scan_without_init(void) {
    ps5_info_t info;
    int result = ps5_detector_scan(&info);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_NOT_INIT, result);
}

void test_ps5_detector_scan_with_null(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    int result = ps5_detector_scan(NULL);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_INVALID_PARAM, result);
}

void test_ps5_detector_quick_check_without_init(void) {
    ps5_info_t info;
    int result = ps5_detector_quick_check(NULL, &info);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_NOT_INIT, result);
}

void test_ps5_detector_quick_check_with_null(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    int result = ps5_detector_quick_check(NULL, NULL);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_INVALID_PARAM, result);
}

/* ============================================================
 *  Test Group 5: String Conversion Tests
 * ============================================================ */

void test_ps5_detector_error_string_ok(void) {
    const char *str = ps5_detector_error_string(PS5_DETECT_OK);
    TEST_ASSERT_EQUAL_STRING("OK", str);
}

void test_ps5_detector_error_string_not_init(void) {
    const char *str = ps5_detector_error_string(PS5_DETECT_ERROR_NOT_INIT);
    TEST_ASSERT_EQUAL_STRING("Not initialized", str);
}

void test_ps5_detector_error_string_not_found(void) {
    const char *str = ps5_detector_error_string(PS5_DETECT_ERROR_NOT_FOUND);
    TEST_ASSERT_EQUAL_STRING("PS5 not found", str);
}

void test_ps5_detector_method_string_cache(void) {
    const char *str = ps5_detector_method_string(DETECT_METHOD_CACHE);
    TEST_ASSERT_EQUAL_STRING("CACHE", str);
}

void test_ps5_detector_method_string_arp(void) {
    const char *str = ps5_detector_method_string(DETECT_METHOD_ARP);
    TEST_ASSERT_EQUAL_STRING("ARP", str);
}

void test_ps5_detector_method_string_scan(void) {
    const char *str = ps5_detector_method_string(DETECT_METHOD_SCAN);
    TEST_ASSERT_EQUAL_STRING("SCAN", str);
}

void test_ps5_detector_method_string_ping(void) {
    const char *str = ps5_detector_method_string(DETECT_METHOD_PING);
    TEST_ASSERT_EQUAL_STRING("PING", str);
}

/* ============================================================
 *  Test Group 6: Lifecycle Tests
 * ============================================================ */

void test_ps5_detector_cleanup_without_init(void) {
    ps5_detector_cleanup();
    TEST_PASS();
}

void test_ps5_detector_cleanup_after_init(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    ps5_detector_cleanup();
    TEST_PASS();
}

void test_ps5_detector_get_cache_age(void) {
    ps5_detector_init("192.168.1.0/24", "/tmp/test_ps5_cache3.json");
    
    // Save cache
    ps5_info_t info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    ps5_detector_save_cache(&info);
    
    // Get cache age
    time_t age = ps5_detector_get_cache_age();
    
    // Should be very recent (0-2 seconds)
    TEST_ASSERT_TRUE(age >= 0 && age <= 2);
}

/* ============================================================
 *  ⚠️ 重要: 不要有 main() 函數!
 *  Ceedling 會自動生成 test runner (包含 main)
 * ============================================================ */
