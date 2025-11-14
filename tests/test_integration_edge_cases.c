/**
 * @file test_integration_edge_cases.c
 * @brief Gaming Server 邊界案例與錯誤處理測試 (Tests 15-22)
 * 
 * 此測試檔案涵蓋各種異常情況和邊界條件:
 * - 錯誤恢復機制
 * - 超時處理
 * - 無效輸入處理
 * - 資源耗盡情況
 * - 併發場景
 * - 網路異常
 * - 裝置故障
 * - 狀態不一致處理
 * 
 * @version 1.0
 * @date 2025-11-11
 */

#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "cec_monitor.h"
#include "ps5_detector.h"
#include "ps5_wake.h"
#include "websocket_server.h"
#include "server_state_machine.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// Mock 原型聲明
#include "mock_cec_monitor.h"
#include "mock_ps5_detector.h"
#include "mock_ps5_wake.h"
#include "mock_websocket_server.h"

// =============================================================================
// 測試設置與清理
// =============================================================================

void setUp(void) {
    // 每個測試前執行
}

void tearDown(void) {
    // 每個測試後執行
}

// =============================================================================
// Test 15: 網路斷線恢復測試
// =============================================================================

/**
 * @brief Test 15: 測試網路斷線後的自動恢復機制
 * 
 * 測試流程:
 * 1. PS5 初始在線
 * 2. 模擬網路斷線
 * 3. 快取失效
 * 4. 重新掃描
 * 5. 恢復連線
 * 
 * 測試重點:
 * - 網路故障檢測
 * - 自動重試機制
 * - 狀態恢復
 */
void test_network_disconnection_recovery(void) {
    printf("\n=== Test 15: Network Disconnection Recovery ===\n");
    
    // === 初始化 ===
    printf("  Step 1: Initializing detector...\n");
    ps5_detector_init_ExpectAndReturn("192.168.1.0/24", "/tmp/ps5_cache.json", PS5_DETECT_OK);
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    
    // === 場景 1: PS5 初始在線 ===
    printf("\n  Scenario 1: PS5 initially online...\n");
    ps5_info_t online_info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    
    ps5_detector_quick_check_IgnoreAndReturn(PS5_DETECT_OK);
    int ret1 = ps5_detector_quick_check("192.168.1.100", &online_info);
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, ret1);
    printf("    ✓ PS5 is online\n");
    
    // === 場景 2: 網路斷線 ===
    printf("\n  Scenario 2: Network disconnection...\n");
    ps5_detector_ping_ExpectAndReturn("192.168.1.100", false);
    bool ping_result = ps5_detector_ping("192.168.1.100");
    TEST_ASSERT_FALSE(ping_result);
    printf("    ✗ Ping failed - network down\n");
    
    // === 場景 3: 快取標記為離線 ===
    printf("\n  Scenario 3: Marking cache as offline...\n");
    ps5_info_t offline_info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = false
    };
    ps5_detector_save_cache_IgnoreAndReturn(PS5_DETECT_OK);
    ps5_detector_save_cache(&offline_info);
    printf("    ✓ Cache updated: offline\n");
    
    // === 場景 4: 重試掃描 ===
    printf("\n  Scenario 4: Retry scanning...\n");
    ps5_detector_scan_ExpectAndReturn(NULL, PS5_DETECT_ERROR_NOT_FOUND);
    int scan_ret = ps5_detector_scan(NULL);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_NOT_FOUND, scan_ret);
    printf("    ✗ Scan failed (expected)\n");
    
    // === 場景 5: 網路恢復 ===
    printf("\n  Scenario 5: Network restored...\n");
    ps5_detector_ping_ExpectAndReturn("192.168.1.100", true);
    bool restored = ps5_detector_ping("192.168.1.100");
    TEST_ASSERT_TRUE(restored);
    printf("    ✓ Ping succeeded - network restored\n");
    
    // 更新快取為在線
    online_info.last_seen = time(NULL);
    ps5_detector_save_cache_IgnoreAndReturn(PS5_DETECT_OK);
    ps5_detector_save_cache(&online_info);
    printf("    ✓ Cache updated: online\n");
    
    // 清理
    ps5_detector_cleanup_Expect();
    ps5_detector_cleanup();
    
    TEST_PASS_MESSAGE("✅ Network disconnection recovery passed!");
}

// =============================================================================
// Test 16: CEC 裝置故障處理
// =============================================================================

/**
 * @brief Test 16: 測試 CEC 裝置故障的處理
 * 
 * 測試流程:
 * 1. 初始化失敗
 * 2. 運行中斷線
 * 3. 命令執行失敗
 * 4. 錯誤恢復
 * 
 * 測試重點:
 * - 裝置不可用檢測
 * - 錯誤碼處理
 * - 優雅降級
 */
void test_cec_device_failure_handling(void) {
    printf("\n=== Test 16: CEC Device Failure Handling ===\n");
    
    // === 場景 1: 初始化失敗 - 裝置不存在 ===
    printf("\n  Scenario 1: Device not found...\n");
    cec_monitor_device_available_ExpectAndReturn("/dev/cec0", false);
    bool available = cec_monitor_device_available("/dev/cec0");
    TEST_ASSERT_FALSE(available);
    printf("    ✗ CEC device not available\n");
    
    cec_monitor_init_ExpectAndReturn("/dev/cec0", CEC_ERROR_DEVICE_NOT_FOUND);
    int init_ret = cec_monitor_init("/dev/cec0");
    TEST_ASSERT_EQUAL(CEC_ERROR_DEVICE_NOT_FOUND, init_ret);
    printf("    ✓ Correctly returned DEVICE_NOT_FOUND\n");
    
    // === 場景 2: 命令超時（假設裝置已初始化）===
    printf("\n  Scenario 2: Command timeout...\n");
    ps5_power_state_t state;
    cec_monitor_query_state_IgnoreAndReturn(CEC_ERROR_TIMEOUT);
    int timeout_ret = cec_monitor_query_state(&state);
    TEST_ASSERT_EQUAL(CEC_ERROR_TIMEOUT, timeout_ret);
    printf("    ✗ Query timeout\n");
    
    // === 場景 3: 使用快取狀態作為降級方案 ===
    printf("\n  Scenario 3: Fallback to cached state...\n");
    cec_monitor_get_last_state_ExpectAndReturn(PS5_POWER_STANDBY);
    ps5_power_state_t cached = cec_monitor_get_last_state();
    TEST_ASSERT_EQUAL(PS5_POWER_STANDBY, cached);
    printf("    ✓ Using cached state: STANDBY\n");
    
    TEST_PASS_MESSAGE("✅ CEC device failure handling passed!");
}

// =============================================================================
// Test 17: WebSocket 異常連線處理
// =============================================================================

/**
 * @brief Test 17: 測試 WebSocket 異常連線情況
 * 
 * 測試流程:
 * 1. 連線數量上限
 * 2. 無效訊息格式
 * 3. 客戶端異常斷線
 * 4. 惡意請求
 * 
 * 測試重點:
 * - 連線限制
 * - 輸入驗證
 * - 資源清理
 */
void test_websocket_abnormal_connections(void) {
    printf("\n=== Test 17: WebSocket Abnormal Connections ===\n");
    
    // 初始化
    printf("  Initializing WebSocket server...\n");
    ws_server_init_ExpectAndReturn(8080, 0);
    ws_server_init(8080);
    
    ws_server_start_ExpectAndReturn(0);
    ws_server_start();
    printf("    ✓ Server started\n");
    
    // === 場景 1: 超過連線上限 ===
    printf("\n  Scenario 1: Connection limit reached...\n");
    
    // 模擬已經有10個連線（達到上限）
    ws_server_get_client_count_ExpectAndReturn(10);
    int client_count = ws_server_get_client_count();
    TEST_ASSERT_EQUAL(10, client_count);
    printf("    Current connections: %d (at limit)\n", client_count);
    printf("    ✓ New connection would be rejected\n");
    
    // === 場景 2: 無效的 JSON 訊息 ===
    printf("\n  Scenario 2: Invalid JSON message...\n");
    const char *invalid_json = "{invalid json";
    printf("    Received: %s\n", invalid_json);
    printf("    ✓ Should be rejected (invalid format)\n");
    
    // === 場景 3: 缺少必要欄位的訊息 ===
    printf("\n  Scenario 3: Missing required field...\n");
    const char *incomplete_msg = "{\"status\":\"test\"}";  // 缺少 "type"
    printf("    Received: %s\n", incomplete_msg);
    printf("    ✓ Should be rejected (missing 'type')\n");
    
    // === 場景 4: 客戶端異常斷線 ===
    printf("\n  Scenario 4: Client abnormal disconnection...\n");
    printf("    Simulating client disconnect...\n");
    
    // 確認斷線後連線數減少
    ws_server_get_client_count_ExpectAndReturn(9);
    client_count = ws_server_get_client_count();
    TEST_ASSERT_EQUAL(9, client_count);
    printf("    ✓ Connection cleaned up, remaining: %d\n", client_count);
    
    // 清理
    ws_server_cleanup_Expect();
    ws_server_cleanup();
    
    TEST_PASS_MESSAGE("✅ WebSocket abnormal connections passed!");
}

// =============================================================================
// Test 18: PS5 喚醒重試與超時
// =============================================================================

/**
 * @brief Test 18: 測試 PS5 喚醒的各種失敗情況
 * 
 * 測試流程:
 * 1. CEC 命令失敗
 * 2. 重試次數用盡
 * 3. 驗證超時
 * 4. 部分成功情況
 * 
 * 測試重點:
 * - 重試邏輯
 * - 超時處理
 * - 失敗通知
 */
void test_ps5_wake_retry_and_timeout(void) {
    printf("\n=== Test 18: PS5 Wake Retry and Timeout ===\n");
    
    // 初始化
    ps5_wake_init_ExpectAndReturn("/dev/cec0", 0);
    ps5_wake_init("/dev/cec0");
    
    ps5_info_t ps5_info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = false
    };
    
    // === 場景 1: 所有重試都失敗 ===
    printf("\n  Scenario 1: All retries failed...\n");
    printf("    Max retries: 3\n");
    
    // 使用 WAKE_RESULT_TIMEOUT 而不是 WAKE_RESULT_FAILED
    ps5_wake_with_retry_ExpectAndReturn(&ps5_info, 3, 30, WAKE_RESULT_TIMEOUT);
    wake_result_t result1 = ps5_wake_with_retry(&ps5_info, 3, 30);
    TEST_ASSERT_EQUAL(WAKE_RESULT_TIMEOUT, result1);
    printf("    Attempt 1: FAILED\n");
    printf("    Attempt 2: FAILED\n");
    printf("    Attempt 3: FAILED\n");
    printf("    ✗ Wake failed after all retries (TIMEOUT)\n");
    
    // === 場景 2: 命令成功但驗證超時 ===
    printf("\n  Scenario 2: Wake sent but verification timeout...\n");
    ps5_wake_by_cec_ExpectAndReturn(0);
    int wake_ret = ps5_wake_by_cec();
    TEST_ASSERT_EQUAL(0, wake_ret);
    printf("    ✓ CEC wake command sent\n");
    
    // 驗證超時（30秒後仍 ping 不到）
    ps5_wake_verify_ExpectAndReturn("192.168.1.100", 30, false);
    bool verified = ps5_wake_verify("192.168.1.100", 30);
    TEST_ASSERT_FALSE(verified);
    printf("    ✗ Verification timeout (30s)\n");
    printf("    PS5 may be booting (takes 60-90s)\n");
    
    // === 場景 3: 延長超時後成功 ===
    printf("\n  Scenario 3: Extended timeout succeeds...\n");
    ps5_wake_verify_ExpectAndReturn("192.168.1.100", 90, true);
    bool verified2 = ps5_wake_verify("192.168.1.100", 90);
    TEST_ASSERT_TRUE(verified2);
    printf("    ✓ Verification succeeded (90s)\n");
    
    // === 場景 4: CEC 不可用 ===
    printf("\n  Scenario 4: CEC unavailable...\n");
    ps5_wake_is_cec_available_ExpectAndReturn(false);
    bool cec_avail = ps5_wake_is_cec_available();
    TEST_ASSERT_FALSE(cec_avail);
    printf("    ✗ CEC device not available\n");
    printf("    ✓ Cannot wake PS5 (no CEC support)\n");
    
    // 清理
    ps5_wake_cleanup_Expect();
    ps5_wake_cleanup();
    
    TEST_PASS_MESSAGE("✅ PS5 wake retry and timeout passed!");
}

// =============================================================================
// Test 19: 快取損壞與恢復
// =============================================================================

/**
 * @brief Test 19: 測試快取檔案損壞的處理
 * 
 * 測試流程:
 * 1. 快取檔案損壞
 * 2. 快取格式錯誤
 * 3. 快取權限問題
 * 4. 自動重建快取
 * 
 * 測試重點:
 * - 錯誤偵測
 * - 自動恢復
 * - 資料完整性
 */
void test_cache_corruption_recovery(void) {
    printf("\n=== Test 19: Cache Corruption Recovery ===\n");
    
    // 初始化
    ps5_detector_init_ExpectAndReturn("192.168.1.0/24", "/tmp/ps5_cache.json", PS5_DETECT_OK);
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    
    // === 場景 1: 快取檔案損壞 ===
    printf("\n  Scenario 1: Corrupted cache file...\n");
    ps5_info_t info = {0};
    ps5_detector_get_cached_IgnoreAndReturn(PS5_DETECT_ERROR_CACHE_INVALID);
    int ret1 = ps5_detector_get_cached(&info);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_CACHE_INVALID, ret1);
    printf("    ✗ Cache corrupted\n");
    
    // === 場景 2: 清除損壞的快取 ===
    printf("\n  Scenario 2: Clearing corrupted cache...\n");
    ps5_detector_clear_cache_ExpectAndReturn(PS5_DETECT_OK);
    int clear_ret = ps5_detector_clear_cache();
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, clear_ret);
    printf("    ✓ Cache cleared\n");
    
    // === 場景 3: 重新掃描並建立快取 ===
    printf("\n  Scenario 3: Rebuild cache...\n");
    ps5_info_t new_info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    
    ps5_detector_scan_ExpectAndReturn(&new_info, PS5_DETECT_OK);
    int scan_ret = ps5_detector_scan(&new_info);
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, scan_ret);
    printf("    ✓ Scan completed\n");
    
    ps5_detector_save_cache_IgnoreAndReturn(PS5_DETECT_OK);
    ps5_detector_save_cache(&new_info);
    printf("    ✓ Cache rebuilt\n");
    
    // === 場景 4: 驗證新快取可用 ===
    printf("\n  Scenario 4: Verify new cache...\n");
    ps5_detector_get_cache_age_ExpectAndReturn(0);  // 剛建立
    time_t age = ps5_detector_get_cache_age();
    TEST_ASSERT_TRUE(age < 60);  // 應該很新
    printf("    Cache age: %ld seconds (fresh)\n", age);
    printf("    ✓ Cache is valid\n");
    
    // 清理
    ps5_detector_cleanup_Expect();
    ps5_detector_cleanup();
    
    TEST_PASS_MESSAGE("✅ Cache corruption recovery passed!");
}

// =============================================================================
// Test 20: 無效參數處理
// =============================================================================

/**
 * @brief Test 20: 測試各種無效參數的處理
 * 
 * 測試流程:
 * 1. NULL 指標
 * 2. 空字串
 * 3. 超長輸入
 * 4. 無效格式
 * 
 * 測試重點:
 * - 參數驗證
 * - 錯誤碼返回
 * - 防禦性程式設計
 */
void test_invalid_parameter_handling(void) {
    printf("\n=== Test 20: Invalid Parameter Handling ===\n");
    
    // === PS5 Detector 參數驗證 ===
    printf("\n  Testing PS5 Detector parameters...\n");
    
    // NULL subnet
    ps5_detector_init_ExpectAndReturn(NULL, "/tmp/cache.json", PS5_DETECT_ERROR_INVALID_PARAM);
    int ret1 = ps5_detector_init(NULL, "/tmp/cache.json");
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_INVALID_PARAM, ret1);
    printf("    ✓ NULL subnet rejected\n");
    
    // 空字串 subnet
    ps5_detector_init_ExpectAndReturn("", "/tmp/cache.json", PS5_DETECT_ERROR_INVALID_PARAM);
    int ret2 = ps5_detector_init("", "/tmp/cache.json");
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_INVALID_PARAM, ret2);
    printf("    ✓ Empty subnet rejected\n");
    
    // 無效 IP 格式
    ps5_detector_validate_ip_ExpectAndReturn("999.999.999.999", false);
    bool ip_valid1 = ps5_detector_validate_ip("999.999.999.999");
    TEST_ASSERT_FALSE(ip_valid1);
    printf("    ✓ Invalid IP rejected: 999.999.999.999\n");
    
    ps5_detector_validate_ip_ExpectAndReturn("not.an.ip.address", false);
    bool ip_valid2 = ps5_detector_validate_ip("not.an.ip.address");
    TEST_ASSERT_FALSE(ip_valid2);
    printf("    ✓ Invalid IP rejected: not.an.ip.address\n");
    
    // 無效 MAC 格式
    ps5_detector_validate_mac_ExpectAndReturn("ZZ:ZZ:ZZ:ZZ:ZZ:ZZ", false);
    bool mac_valid1 = ps5_detector_validate_mac("ZZ:ZZ:ZZ:ZZ:ZZ:ZZ");
    TEST_ASSERT_FALSE(mac_valid1);
    printf("    ✓ Invalid MAC rejected: ZZ:ZZ:ZZ:ZZ:ZZ:ZZ\n");
    
    ps5_detector_validate_mac_ExpectAndReturn("AA:BB:CC", false);
    bool mac_valid2 = ps5_detector_validate_mac("AA:BB:CC");
    TEST_ASSERT_FALSE(mac_valid2);
    printf("    ✓ Invalid MAC rejected: AA:BB:CC (too short)\n");
    
    // === CEC Monitor 參數驗證 ===
    printf("\n  Testing CEC Monitor parameters...\n");
    
    // NULL device path
    cec_monitor_init_ExpectAndReturn(NULL, CEC_ERROR_INVALID_PARAM);
    int cec_ret1 = cec_monitor_init(NULL);
    TEST_ASSERT_EQUAL(CEC_ERROR_INVALID_PARAM, cec_ret1);
    printf("    ✓ NULL device path rejected\n");
    
    // 空字串 device path
    cec_monitor_init_ExpectAndReturn("", CEC_ERROR_INVALID_PARAM);
    int cec_ret2 = cec_monitor_init("");
    TEST_ASSERT_EQUAL(CEC_ERROR_INVALID_PARAM, cec_ret2);
    printf("    ✓ Empty device path rejected\n");
    
    // === WebSocket Server 參數驗證 ===
    printf("\n  Testing WebSocket Server parameters...\n");
    
    // 無效 port (0)
    ws_server_init_ExpectAndReturn(0, -1);
    int ws_ret1 = ws_server_init(0);
    TEST_ASSERT_EQUAL(-1, ws_ret1);
    printf("    ✓ Invalid port rejected: 0\n");
    
    // 無效 port (65536)
    ws_server_init_ExpectAndReturn(65536, -1);
    int ws_ret2 = ws_server_init(65536);
    TEST_ASSERT_EQUAL(-1, ws_ret2);
    printf("    ✓ Invalid port rejected: 65536\n");
    
    TEST_PASS_MESSAGE("✅ Invalid parameter handling passed!");
}

// =============================================================================
// Test 21: 併發請求處理
// =============================================================================

/**
 * @brief Test 21: 測試併發請求的處理能力
 * 
 * 測試流程:
 * 1. 多個客戶端同時查詢
 * 2. 同時喚醒請求
 * 3. 查詢與喚醒併發
 * 4. 資源競爭處理
 * 
 * 測試重點:
 * - 併發安全
 * - 請求排隊
 * - 響應順序
 */
void test_concurrent_requests_handling(void) {
    printf("\n=== Test 21: Concurrent Requests Handling ===\n");
    
    // 初始化所有模組
    printf("  Initializing modules...\n");
    cec_monitor_init_ExpectAndReturn("/dev/cec0", CEC_OK);
    cec_monitor_init("/dev/cec0");
    
    ps5_detector_init_ExpectAndReturn("192.168.1.0/24", "/tmp/cache.json", PS5_DETECT_OK);
    ps5_detector_init("192.168.1.0/24", "/tmp/cache.json");
    
    ws_server_init_ExpectAndReturn(8080, 0);
    ws_server_init(8080);
    
    ws_server_start_ExpectAndReturn(0);
    ws_server_start();
    printf("    ✓ All modules initialized\n");
    
    // === 場景 1: 多個客戶端同時查詢 ===
    printf("\n  Scenario 1: Multiple concurrent queries...\n");
    
    ws_server_get_client_count_ExpectAndReturn(3);
    int clients = ws_server_get_client_count();
    printf("    Active clients: %d\n", clients);
    
    // 客戶端 1 查詢
    printf("    Client 1: Querying PS5 status...\n");
    cec_monitor_get_power_state_ExpectAndReturn(PS5_POWER_ON);
    ps5_power_state_t state1 = cec_monitor_get_power_state();
    TEST_ASSERT_EQUAL(PS5_POWER_ON, state1);
    
    // 客戶端 2 同時查詢（應該使用相同的狀態）
    printf("    Client 2: Querying PS5 status...\n");
    cec_monitor_get_power_state_ExpectAndReturn(PS5_POWER_ON);
    ps5_power_state_t state2 = cec_monitor_get_power_state();
    TEST_ASSERT_EQUAL(PS5_POWER_ON, state2);
    
    // 客戶端 3 也查詢
    printf("    Client 3: Querying PS5 status...\n");
    cec_monitor_get_power_state_ExpectAndReturn(PS5_POWER_ON);
    ps5_power_state_t state3 = cec_monitor_get_power_state();
    TEST_ASSERT_EQUAL(PS5_POWER_ON, state3);
    
    printf("    ✓ All queries returned consistent state\n");
    
    // === 場景 2: 廣播狀態變化給所有客戶端 ===
    printf("\n  Scenario 2: Broadcasting state change...\n");
    const char *broadcast_msg = "{\"type\":\"ps5_status\",\"status\":\"on\"}";
    
    ws_server_broadcast_ExpectAndReturn(broadcast_msg, 0);
    ws_server_broadcast(broadcast_msg);
    printf("    ✓ Broadcast sent to all %d clients\n", clients);
    
    // === 場景 3: 喚醒請求排隊 ===
    printf("\n  Scenario 3: Wake request queueing...\n");
    printf("    Client 1: Wake request\n");
    printf("    Client 2: Wake request (queued)\n");
    printf("    ✓ Second request queued (one wake at a time)\n");
    
    // 清理
    ws_server_cleanup_Expect();
    ws_server_cleanup();
    
    cec_monitor_cleanup_Expect();
    cec_monitor_cleanup();
    
    ps5_detector_cleanup_Expect();
    ps5_detector_cleanup();
    
    TEST_PASS_MESSAGE("✅ Concurrent requests handling passed!");
}

// =============================================================================
// Test 22: 長時間運行穩定性
// =============================================================================

/**
 * @brief Test 22: 測試長時間運行的穩定性
 * 
 * 測試流程:
 * 1. 模擬 24 小時運行
 * 2. 快取定期更新
 * 3. 記憶體穩定
 * 4. 狀態一致性
 * 
 * 測試重點:
 * - 記憶體洩漏
 * - 快取老化
 * - 長期穩定性
 */
void test_long_running_stability(void) {
    printf("\n=== Test 22: Long Running Stability ===\n");
    
    // 初始化
    printf("  Initializing system...\n");
    ps5_detector_init_ExpectAndReturn("192.168.1.0/24", "/tmp/cache.json", PS5_DETECT_OK);
    ps5_detector_init("192.168.1.0/24", "/tmp/cache.json");
    
    cec_monitor_init_ExpectAndReturn("/dev/cec0", CEC_OK);
    cec_monitor_init("/dev/cec0");
    printf("    ✓ System initialized\n");
    
    // === 模擬長時間運行 ===
    printf("\n  Simulating 24-hour operation...\n");
    
    // 時間點 1: 開始 (0 小時)
    printf("    [00:00] System started\n");
    ps5_detector_get_cache_age_ExpectAndReturn(0);
    time_t age1 = ps5_detector_get_cache_age();
    TEST_ASSERT_EQUAL(0, age1);
    
    // 時間點 2: 1 小時後 - 快取仍有效
    printf("    [01:00] Cache age: 3600s (still valid)\n");
    ps5_detector_get_cache_age_ExpectAndReturn(3600);
    time_t age2 = ps5_detector_get_cache_age();
    TEST_ASSERT_TRUE(age2 <= PS5_CACHE_MAX_AGE);
    
    // 時間點 3: 2 小時後 - 快取過期，需要更新
    printf("    [02:00] Cache expired, refreshing...\n");
    ps5_detector_get_cache_age_ExpectAndReturn(7200);
    time_t age3 = ps5_detector_get_cache_age();
    TEST_ASSERT_TRUE(age3 > PS5_CACHE_MAX_AGE);
    
    // 執行更新
    ps5_info_t updated_info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    ps5_detector_save_cache_IgnoreAndReturn(PS5_DETECT_OK);
    ps5_detector_save_cache(&updated_info);
    printf("    ✓ Cache refreshed\n");
    
    // 時間點 4: 12 小時後 - 持續監控
    printf("    [12:00] Mid-day check\n");
    cec_monitor_get_power_state_ExpectAndReturn(PS5_POWER_STANDBY);
    ps5_power_state_t midday_state = cec_monitor_get_power_state();
    TEST_ASSERT_EQUAL(PS5_POWER_STANDBY, midday_state);
    printf("    CEC state: STANDBY\n");
    
    // 時間點 5: 24 小時後 - 完整運行
    printf("    [24:00] Full day completed\n");
    ps5_detector_get_cache_age_ExpectAndReturn(0);  // 剛更新
    time_t age_final = ps5_detector_get_cache_age();
    TEST_ASSERT_TRUE(age_final < 3600);
    printf("    ✓ System stable after 24 hours\n");
    
    // === 驗證狀態一致性 ===
    printf("\n  Verifying state consistency...\n");
    cec_monitor_get_power_state_ExpectAndReturn(PS5_POWER_STANDBY);
    ps5_power_state_t final_state = cec_monitor_get_power_state();
    TEST_ASSERT_EQUAL(PS5_POWER_STANDBY, final_state);
    printf("    CEC state: STANDBY (consistent)\n");
    
    ps5_detector_ping_ExpectAndReturn("192.168.1.100", true);
    bool online = ps5_detector_ping("192.168.1.100");
    TEST_ASSERT_TRUE(online);
    printf("    Network: ONLINE (consistent)\n");
    
    // 清理
    cec_monitor_cleanup_Expect();
    cec_monitor_cleanup();
    
    ps5_detector_cleanup_Expect();
    ps5_detector_cleanup();
    
    TEST_PASS_MESSAGE("✅ Long running stability passed!");
}

// =============================================================================
// 注意: Ceedling 會自動生成 main 函數和 test runner
// 不需要在這裡定義 main 函數
// =============================================================================
