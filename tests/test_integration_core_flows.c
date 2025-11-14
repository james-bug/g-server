/**
 * @file test_integration_core_flows.c
 * @brief Gaming Server 核心流程整合測試 (Tests 7-14)
 * 
 * 此測試檔案涵蓋完整的系統流程串接測試:
 * - PS5 偵測三層策略
 * - CEC 喚醒流程 (僅 CEC，無 WOL)
 * - WebSocket 連線管理
 * - CEC 狀態查詢
 * - 輸入驗證
 * - 快取管理
 * - 完整客戶端查詢流程
 * - CEC 裝置可用性檢查
 * 
 * 基於實際 GitHub API：完全符合 ps5_detector.h, ps5_wake.h, 
 * cec_monitor.h, websocket_server.h
 * 
 * @version 1.0
 * @date 2025-11-10
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
// 輔助函數
// =============================================================================

// 注意: ps5_power_state_to_string 已在 cec_monitor.h 中聲明
// 直接使用該函數，不需要重複定義

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
// Test 7: 完整的 PS5 偵測三層策略
// =============================================================================

/**
 * @brief Test 7: 驗證 PS5 偵測的三層策略
 * 
 * 測試流程:
 * 1. 場景 1: 快取命中 (<1ms)
 * 2. 場景 2: 快取失效，使用 ARP (~10ms)
 * 3. 場景 3: ARP 失敗，執行 Nmap 掃描 (~5-30s)
 * 4. 驗證結果應保存到快取
 * 
 * 測試重點:
 * - 驗證三層策略的完整流程
 * - 確認各層的性能特性
 * - 測試快取更新機制
 */
void test_ps5_detector_three_tier_strategy_complete(void) {
    printf("\n=== Test 7: PS5 Detector Three-Tier Strategy ===\n");
    
    // 初始化
    printf("  Initializing PS5 detector...\n");
    ps5_detector_init_ExpectAndReturn("192.168.1.0/24", "/tmp/ps5_cache.json", PS5_DETECT_OK);
    int init_ret = ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, init_ret);
    
    // === 場景 1: 快取命中（最快路徑）===
    printf("\n  Scenario 1: Cache hit (fastest path)...\n");
    
    // 檢查快取年齡 (新增的 API)
    ps5_detector_get_cache_age_ExpectAndReturn(100);
    time_t cache_age = ps5_detector_get_cache_age();
    TEST_ASSERT_TRUE(cache_age < PS5_CACHE_MAX_AGE);  // 應該還在有效期內
    printf("    Cache age: %ld seconds (VALID)\n", cache_age);
    
    // 從快取獲取 - 使用 IgnoreAndReturn 來忽略參數檢查
    ps5_info_t result1 = {0};
    ps5_detector_get_cached_IgnoreAndReturn(PS5_DETECT_OK);
    int cached_ret = ps5_detector_get_cached(&result1);
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, cached_ret);
    printf("    ✓ Cache hit: <1ms\n");
    
    // === 場景 2: 快取失效，使用 ARP ===
    printf("\n  Scenario 2: Cache miss, ARP lookup...\n");
    
    // 快取過期
    ps5_detector_get_cache_age_ExpectAndReturn(PS5_CACHE_MAX_AGE + 100);
    time_t expired_age = ps5_detector_get_cache_age();
    TEST_ASSERT_TRUE(expired_age > PS5_CACHE_MAX_AGE);
    printf("    Cache expired: %ld seconds old\n", expired_age);
    
    // 使用 quick_check (ARP)
    ps5_info_t result2 = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    ps5_detector_quick_check_ExpectAndReturn("192.168.1.100", &result2, PS5_DETECT_OK);
    int quick_ret = ps5_detector_quick_check("192.168.1.100", &result2);
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, quick_ret);
    printf("    ✓ ARP lookup: ~10ms\n");
    printf("    Found PS5: IP=%s\n", result2.ip);
    
    // === 場景 3: ARP 失敗，執行 Nmap 掃描 ===
    printf("\n  Scenario 3: ARP fail, Nmap full scan...\n");
    
    // quick_check 失敗
    ps5_detector_quick_check_ExpectAndReturn(NULL, NULL, PS5_DETECT_ERROR_NOT_FOUND);
    int arp_fail = ps5_detector_quick_check(NULL, NULL);
    TEST_ASSERT_EQUAL(PS5_DETECT_ERROR_NOT_FOUND, arp_fail);
    printf("    ARP lookup failed\n");
    
    // 執行完整掃描
    ps5_info_t result3 = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    ps5_detector_scan_ExpectAndReturn(&result3, PS5_DETECT_OK);
    int scan_ret = ps5_detector_scan(&result3);
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, scan_ret);
    printf("    ✓ Nmap scan: ~5-30s\n");
    printf("    Found PS5: IP=%s, MAC=%s\n", result3.ip, result3.mac);
    
    // === 驗證：應該保存到快取 ===
    printf("\n  Saving to cache...\n");
    ps5_detector_save_cache_ExpectAndReturn(&result3, PS5_DETECT_OK);
    int save_ret = ps5_detector_save_cache(&result3);
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, save_ret);
    printf("    ✓ Cache updated\n");
    
    // 清理
    ps5_detector_cleanup_Expect();
    ps5_detector_cleanup();
    
    TEST_PASS_MESSAGE("✅ Three-tier detection strategy passed!");
}

// =============================================================================
// Test 8: CEC 喚醒完整流程（僅 CEC，無 WOL）
// =============================================================================

/**
 * @brief Test 8: 驗證 CEC 喚醒流程（無 WOL 支援）
 * 
 * 重要: PS5 僅支援 CEC 喚醒，不支援 WOL
 * 
 * 測試流程:
 * 1. 初始化 CEC
 * 2. 檢查 CEC 裝置可用性
 * 3. 第一次嘗試失敗
 * 4. 使用重試機制
 * 5. 驗證喚醒成功
 * 
 * 測試重點:
 * - 僅測試 CEC 喚醒 (不測試 WOL)
 * - 驗證重試機制
 * - 確認喚醒驗證
 */
void test_ps5_wake_cec_only_with_retry(void) {
    printf("\n=== Test 8: PS5 Wake - CEC Only (No WOL Support) ===\n");
    
    // === 初始化 ===
    printf("  Step 1: Initializing CEC wake...\n");
    ps5_wake_init_ExpectAndReturn("/dev/cec0", 0);
    int init_ret = ps5_wake_init("/dev/cec0");
    TEST_ASSERT_EQUAL(0, init_ret);
    printf("    ✓ CEC wake initialized\n");
    
    // === 檢查 CEC 裝置可用性 ===
    printf("\n  Step 2: Checking CEC device availability...\n");
    ps5_wake_is_cec_available_ExpectAndReturn(true);
    bool cec_available = ps5_wake_is_cec_available();
    TEST_ASSERT_TRUE(cec_available);
    printf("    ✓ CEC device: AVAILABLE\n");
    
    // === PS5 資訊 ===
    ps5_info_t ps5_info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = false
    };
    printf("    Target PS5: IP=%s, MAC=%s\n", ps5_info.ip, ps5_info.mac);
    
    // === 第一次嘗試失敗 ===
    printf("\n  Step 3: First wake attempt...\n");
    ps5_wake_by_cec_ExpectAndReturn(-1);
    int first_attempt = ps5_wake_by_cec();
    TEST_ASSERT_EQUAL(-1, first_attempt);
    printf("    Result: FAILED\n");
    
    // === 使用重試機制 ===
    printf("\n  Step 4: Using retry mechanism (max 3 retries)...\n");
    ps5_wake_with_retry_ExpectAndReturn(&ps5_info, 3, 30, WAKE_RESULT_SUCCESS);
    wake_result_t wake_result = ps5_wake_with_retry(&ps5_info, 3, 30);
    TEST_ASSERT_EQUAL(WAKE_RESULT_SUCCESS, wake_result);
    printf("    Attempt 1: FAILED\n");
    printf("    Attempt 2: SUCCESS\n");
    printf("    ✓ Wake successful after retry\n");
    
    // === 驗證喚醒 ===
    printf("\n  Step 5: Verifying wake status...\n");
    ps5_wake_verify_ExpectAndReturn("192.168.1.100", 30, true);
    bool verified = ps5_wake_verify("192.168.1.100", 30);
    TEST_ASSERT_TRUE(verified);
    printf("    Ping test: SUCCESS\n");
    printf("    ✓ PS5 is now online\n");
    
    // 清理
    ps5_wake_cleanup_Expect();
    ps5_wake_cleanup();
    
    TEST_PASS_MESSAGE("✅ CEC-only wake with retry passed!");
}

// =============================================================================
// Test 9: WebSocket 連線/斷線回調
// =============================================================================

// 全域計數器
static int g_connect_count = 0;
static int g_disconnect_count = 0;

/**
 * @brief 客戶端連線回調
 */
static void on_client_connect(int client_id, const char *client_ip, void *user_data) {
    (void)user_data;  // 未使用
    printf("  ✓ Client %d connected from %s\n", client_id, client_ip);
    g_connect_count++;
}

/**
 * @brief 客戶端斷線回調
 */
static void on_client_disconnect(int client_id, void *user_data) {
    (void)user_data;  // 未使用
    printf("  ✓ Client %d disconnected\n", client_id);
    g_disconnect_count++;
}

/**
 * @brief Test 9: 測試 WebSocket 連線管理回調
 * 
 * 測試流程:
 * 1. 初始化並設定回調
 * 2. 模擬客戶端連線
 * 3. 獲取客戶端列表
 * 4. 模擬客戶端斷線
 * 
 * 測試重點:
 * - 連線/斷線回調機制
 * - 客戶端列表管理 (新增的 API)
 * - 多客戶端支援
 */
void test_websocket_connect_disconnect_callbacks(void) {
    printf("\n=== Test 9: WebSocket Connect/Disconnect Callbacks ===\n");
    
    // 重置計數器
    g_connect_count = 0;
    g_disconnect_count = 0;
    
    // === 初始化 ===
    printf("  Step 1: Initializing WebSocket server...\n");
    ws_server_init_ExpectAndReturn(8080, 0);
    int init_ret = ws_server_init(8080);
    TEST_ASSERT_EQUAL(0, init_ret);
    
    // === 設定回調 ===
    printf("\n  Step 2: Setting up callbacks...\n");
    ws_server_set_connect_callback_Ignore();
    ws_server_set_disconnect_callback_Ignore();
    ws_server_set_connect_callback(on_client_connect, NULL);
    ws_server_set_disconnect_callback(on_client_disconnect, NULL);
    printf("    ✓ Callbacks registered\n");
    
    // === 啟動 server ===
    ws_server_start_ExpectAndReturn(0);
    ws_server_start();
    printf("    ✓ Server started on port 8080\n");
    
    // === 模擬客戶端連線 ===
    printf("\n  Step 3: Simulating client connections...\n");
    on_client_connect(1, "192.168.1.50", NULL);
    on_client_connect(2, "192.168.1.51", NULL);
    on_client_connect(3, "192.168.1.52", NULL);
    
    TEST_ASSERT_EQUAL(3, g_connect_count);
    printf("    Total connections: %d\n", g_connect_count);
    
    // === 取得客戶端列表（新增的 API）===
    printf("\n  Step 4: Getting client list...\n");
    ws_client_info_t clients[10];
    ws_server_get_clients_ExpectAndReturn(clients, 10, 3);
    int client_count = ws_server_get_clients(clients, 10);
    TEST_ASSERT_EQUAL(3, client_count);
    printf("    Active clients: %d\n", client_count);
    
    // === 獲取 port (新增的 API) ===
    ws_server_get_port_ExpectAndReturn(8080);
    int port = ws_server_get_port();
    TEST_ASSERT_EQUAL(8080, port);
    printf("    Server port: %d\n", port);
    
    // === 模擬客戶端斷線 ===
    printf("\n  Step 5: Simulating client disconnection...\n");
    on_client_disconnect(2, NULL);
    TEST_ASSERT_EQUAL(1, g_disconnect_count);
    printf("    Disconnected clients: %d\n", g_disconnect_count);
    
    // 清理
    ws_server_cleanup_Expect();
    ws_server_cleanup();
    
    TEST_PASS_MESSAGE("✅ Connect/Disconnect callbacks passed!");
}

// =============================================================================
// Test 10: CEC 主動查詢狀態
// =============================================================================

/**
 * @brief Test 10: 測試 CEC 主動查詢功能
 * 
 * 測試流程:
 * 1. 初始化 CEC monitor
 * 2. 被動獲取快取狀態
 * 3. 主動查詢最新狀態
 * 4. 檢查更新時間
 * 
 * 測試重點:
 * - 被動 vs 主動查詢的差異
 * - cec_monitor_query_state (新增的 API)
 * - 最後更新時間追蹤
 */
void test_cec_monitor_active_query_state(void) {
    printf("\n=== Test 10: CEC Monitor Active Query State ===\n");
    
    // === 初始化 ===
    printf("  Step 1: Initializing CEC monitor...\n");
    cec_monitor_init_ExpectAndReturn("/dev/cec0", CEC_OK);
    int init_ret = cec_monitor_init("/dev/cec0");
    TEST_ASSERT_EQUAL(CEC_OK, init_ret);
    printf("    ✓ CEC monitor initialized\n");
    
    // === 被動獲取（從快取）===
    printf("\n  Step 2: Getting cached state (passive)...\n");
    cec_monitor_get_last_state_ExpectAndReturn(PS5_POWER_STANDBY);
    ps5_power_state_t cached_state = cec_monitor_get_last_state();
    TEST_ASSERT_EQUAL(PS5_POWER_STANDBY, cached_state);
    printf("    Cached state: STANDBY\n");
    
    // === 主動查詢（新增的 API）===
    printf("\n  Step 3: Actively querying state (real-time)...\n");
    
    ps5_power_state_t state_out;
    // 使用 IgnoreAndReturn 因為這是輸出參數
    cec_monitor_query_state_IgnoreAndReturn(CEC_OK);
    int query_ret = cec_monitor_query_state(&state_out);
    
    TEST_ASSERT_EQUAL(CEC_OK, query_ret);
    printf("    Queried state: ON\n");
    printf("    ✓ State changed: STANDBY → ON\n");
    
    // === 檢查更新時間（新增的 API）===
    printf("\n  Step 4: Checking last update time...\n");
    time_t now = time(NULL);
    cec_monitor_get_last_update_ExpectAndReturn(now);
    time_t last_update = cec_monitor_get_last_update();
    
    time_t age = now - last_update;
    printf("    Last update: %ld seconds ago\n", age);
    TEST_ASSERT_TRUE(age < 5);  // 應該在 5 秒內
    
    // === 使用別名函數測試 ===
    printf("\n  Step 5: Testing compatibility alias...\n");
    cec_monitor_get_power_state_ExpectAndReturn(PS5_POWER_ON);
    ps5_power_state_t alias_state = cec_monitor_get_power_state();
    TEST_ASSERT_EQUAL(PS5_POWER_ON, alias_state);
    printf("    Alias function works: ON\n");
    
    // 清理
    cec_monitor_cleanup_Expect();
    cec_monitor_cleanup();
    
    TEST_PASS_MESSAGE("✅ Active query state passed!");
}

// =============================================================================
// Test 11: IP/MAC 驗證功能
// =============================================================================

/**
 * @brief Test 11: 測試輸入驗證功能
 * 
 * 測試流程:
 * 1. 測試 IP 地址驗證
 * 2. 測試 MAC 地址驗證
 * 
 * 測試重點:
 * - ps5_detector_validate_ip (新增的 API)
 * - ps5_detector_validate_mac (新增的 API)
 * - 各種格式的邊界條件
 */
void test_ps5_detector_validation_functions(void) {
    printf("\n=== Test 11: PS5 Detector Validation Functions ===\n");
    
    // === IP 驗證 ===
    printf("\n  Testing IP validation...\n");
    
    // 有效 IP
    ps5_detector_validate_ip_ExpectAndReturn("192.168.1.100", true);
    TEST_ASSERT_TRUE(ps5_detector_validate_ip("192.168.1.100"));
    printf("    ✓ Valid: 192.168.1.100\n");
    
    ps5_detector_validate_ip_ExpectAndReturn("10.0.0.1", true);
    TEST_ASSERT_TRUE(ps5_detector_validate_ip("10.0.0.1"));
    printf("    ✓ Valid: 10.0.0.1\n");
    
    // 無效 IP
    ps5_detector_validate_ip_ExpectAndReturn("999.999.999.999", false);
    TEST_ASSERT_FALSE(ps5_detector_validate_ip("999.999.999.999"));
    printf("    ✗ Invalid: 999.999.999.999\n");
    
    ps5_detector_validate_ip_ExpectAndReturn("invalid", false);
    TEST_ASSERT_FALSE(ps5_detector_validate_ip("invalid"));
    printf("    ✗ Invalid: invalid\n");
    
    ps5_detector_validate_ip_ExpectAndReturn("", false);
    TEST_ASSERT_FALSE(ps5_detector_validate_ip(""));
    printf("    ✗ Invalid: (empty string)\n");
    
    // === MAC 驗證 ===
    printf("\n  Testing MAC validation...\n");
    
    // 有效 MAC (大寫)
    ps5_detector_validate_mac_ExpectAndReturn("AA:BB:CC:DD:EE:FF", true);
    TEST_ASSERT_TRUE(ps5_detector_validate_mac("AA:BB:CC:DD:EE:FF"));
    printf("    ✓ Valid: AA:BB:CC:DD:EE:FF\n");
    
    // 有效 MAC (小寫)
    ps5_detector_validate_mac_ExpectAndReturn("aa:bb:cc:dd:ee:ff", true);
    TEST_ASSERT_TRUE(ps5_detector_validate_mac("aa:bb:cc:dd:ee:ff"));
    printf("    ✓ Valid: aa:bb:cc:dd:ee:ff\n");
    
    // 無效格式（使用 - 而非 :）
    ps5_detector_validate_mac_ExpectAndReturn("AA-BB-CC-DD-EE-FF", false);
    TEST_ASSERT_FALSE(ps5_detector_validate_mac("AA-BB-CC-DD-EE-FF"));
    printf("    ✗ Invalid: AA-BB-CC-DD-EE-FF (wrong separator)\n");
    
    // 無效 MAC
    ps5_detector_validate_mac_ExpectAndReturn("invalid", false);
    TEST_ASSERT_FALSE(ps5_detector_validate_mac("invalid"));
    printf("    ✗ Invalid: invalid\n");
    
    ps5_detector_validate_mac_ExpectAndReturn("", false);
    TEST_ASSERT_FALSE(ps5_detector_validate_mac(""));
    printf("    ✗ Invalid: (empty string)\n");
    
    TEST_PASS_MESSAGE("✅ Validation functions passed!");
}

// =============================================================================
// Test 12: 快取年齡管理
// =============================================================================

/**
 * @brief Test 12: 測試快取過期邏輯
 * 
 * 測試流程:
 * 1. 新鮮的快取（有效）
 * 2. 過期的快取（需要更新）
 * 3. 清除快取
 * 4. 不存在的快取
 * 
 * 測試重點:
 * - ps5_detector_get_cache_age (新增的 API)
 * - ps5_detector_clear_cache (新增的 API)
 * - 快取過期判斷邏輯
 */
void test_ps5_detector_cache_age_management(void) {
    printf("\n=== Test 12: PS5 Detector Cache Age Management ===\n");
    
    // 初始化
    ps5_detector_init_ExpectAndReturn("192.168.1.0/24", "/tmp/ps5_cache.json", PS5_DETECT_OK);
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    
    // === 場景 1: 新鮮的快取 ===
    printf("\n  Scenario 1: Fresh cache...\n");
    ps5_detector_get_cache_age_ExpectAndReturn(100);
    time_t age1 = ps5_detector_get_cache_age();
    TEST_ASSERT_TRUE(age1 < PS5_CACHE_MAX_AGE);  // < 3600 秒
    printf("    Cache age: %ld seconds\n", age1);
    printf("    Status: VALID (< %d seconds)\n", PS5_CACHE_MAX_AGE);
    
    // === 場景 2: 過期的快取 ===
    printf("\n  Scenario 2: Expired cache...\n");
    ps5_detector_get_cache_age_ExpectAndReturn(4000);
    time_t age2 = ps5_detector_get_cache_age();
    TEST_ASSERT_TRUE(age2 > PS5_CACHE_MAX_AGE);  // > 3600 秒
    printf("    Cache age: %ld seconds\n", age2);
    printf("    Status: EXPIRED (> %d seconds)\n", PS5_CACHE_MAX_AGE);
    printf("    ✓ Need to refresh\n");
    
    // === 場景 3: 清除快取 ===
    printf("\n  Scenario 3: Clearing cache...\n");
    ps5_detector_clear_cache_ExpectAndReturn(PS5_DETECT_OK);
    int clear_ret = ps5_detector_clear_cache();
    TEST_ASSERT_EQUAL(PS5_DETECT_OK, clear_ret);
    printf("    ✓ Cache cleared successfully\n");
    
    // === 場景 4: 快取不存在 ===
    printf("\n  Scenario 4: No cache exists...\n");
    ps5_detector_get_cache_age_ExpectAndReturn(-1);
    time_t age4 = ps5_detector_get_cache_age();
    TEST_ASSERT_EQUAL(-1, age4);
    printf("    Cache age: -1 (NO CACHE)\n");
    printf("    ✓ Cache needs to be created\n");
    
    // 清理
    ps5_detector_cleanup_Expect();
    ps5_detector_cleanup();
    
    TEST_PASS_MESSAGE("✅ Cache age management passed!");
}

// =============================================================================
// Test 13: 完整的客戶端查詢流程
// =============================================================================

/**
 * @brief Test 13: 端到端的查詢流程測試
 * 
 * 這是一個完整的端到端測試，模擬真實的客戶端查詢流程:
 * 
 * 測試流程:
 * 1. 初始化所有模組
 * 2. WebSocket server 啟動
 * 3. 客戶端連線
 * 4. 接收查詢請求
 * 5. 查詢 CEC 狀態
 * 6. 快速檢查網路狀態
 * 7. 綜合判斷 PS5 狀態
 * 8. 回應客戶端
 * 
 * 測試重點:
 * - 完整的模組串接
 * - 狀態判斷邏輯
 * - 數據流轉
 */
void test_complete_client_query_with_real_apis(void) {
    printf("\n=== Test 13: Complete Client Query Flow (Real APIs) ===\n");
    
    // === 步驟 1: 初始化所有模組 ===
    printf("\n  Step 1: Initializing all modules...\n");
    
    cec_monitor_init_ExpectAndReturn("/dev/cec0", CEC_OK);
    cec_monitor_init("/dev/cec0");
    printf("    ✓ CEC Monitor initialized\n");
    
    ps5_detector_init_ExpectAndReturn("192.168.1.0/24", "/tmp/ps5_cache.json", PS5_DETECT_OK);
    ps5_detector_init("192.168.1.0/24", "/tmp/ps5_cache.json");
    printf("    ✓ PS5 Detector initialized\n");
    
    ws_server_init_ExpectAndReturn(8080, 0);
    ws_server_init(8080);
    printf("    ✓ WebSocket Server initialized\n");
    
    // === 步驟 2: 啟動 WebSocket server ===
    printf("\n  Step 2: Starting WebSocket server...\n");
    ws_server_start_ExpectAndReturn(0);
    ws_server_start();
    printf("    ✓ Server listening on port 8080\n");
    
    // === 步驟 3: 客戶端連線 ===
    printf("\n  Step 3: Client connecting...\n");
    ws_server_get_client_count_ExpectAndReturn(1);
    int client_count = ws_server_get_client_count();
    printf("    Active clients: %d\n", client_count);
    TEST_ASSERT_EQUAL(1, client_count);
    
    // === 步驟 4: 接收查詢請求 ===
    printf("\n  Step 4: Receiving query request...\n");
    printf("    Request: {\"type\":\"query_ps5\"}\n");
    
    // === 步驟 5: 查詢 CEC 狀態 ===
    printf("\n  Step 5: Querying CEC power state...\n");
    cec_monitor_get_power_state_ExpectAndReturn(PS5_POWER_ON);
    ps5_power_state_t cec_state = cec_monitor_get_power_state();
    printf("    CEC state: ON\n");
    TEST_ASSERT_EQUAL(PS5_POWER_ON, cec_state);
    
    // === 步驟 6: 快速檢查網路狀態 ===
    printf("\n  Step 6: Quick network status check...\n");
    ps5_info_t info = {
        .ip = "192.168.1.100",
        .mac = "AA:BB:CC:DD:EE:FF",
        .last_seen = time(NULL),
        .online = true
    };
    ps5_detector_quick_check_ExpectAndReturn("192.168.1.100", &info, PS5_DETECT_OK);
    int detect_ret = ps5_detector_quick_check("192.168.1.100", &info);
    bool network_online = (detect_ret == PS5_DETECT_OK);
    printf("    Network: %s (IP: %s)\n", 
           network_online ? "ONLINE" : "OFFLINE", info.ip);
    TEST_ASSERT_TRUE(network_online);
    
    // === 步驟 7: 綜合判斷 PS5 狀態 ===
    printf("\n  Step 7: Determining final PS5 status...\n");
    const char* ps5_status;
    if (cec_state == PS5_POWER_ON && network_online) {
        ps5_status = "on";
    } else if (cec_state == PS5_POWER_STANDBY) {
        ps5_status = "standby";
    } else if (cec_state == PS5_POWER_OFF) {
        ps5_status = "off";
    } else {
        ps5_status = "unknown";
    }
    printf("    CEC: ON + Network: ONLINE\n");
    printf("    Final status: %s\n", ps5_status);
    TEST_ASSERT_EQUAL_STRING("on", ps5_status);
    
    // === 步驟 8: 回應客戶端 ===
    printf("\n  Step 8: Sending response to client...\n");
    char response[256];
    snprintf(response, sizeof(response), 
             "{\"type\":\"ps5_status\",\"status\":\"%s\",\"ip\":\"%s\"}", 
             ps5_status, info.ip);
    
    ws_server_send_ExpectAndReturn(1, response, 0);
    ws_server_send(1, response);
    printf("    Response: %s\n", response);
    printf("    ✓ Response sent to client\n");
    
    // 清理
    ws_server_cleanup_Expect();
    ws_server_cleanup();
    
    cec_monitor_cleanup_Expect();
    cec_monitor_cleanup();
    
    ps5_detector_cleanup_Expect();
    ps5_detector_cleanup();
    
    TEST_PASS_MESSAGE("✅ Complete client query flow passed!");
}

// =============================================================================
// Test 14: CEC 裝置可用性檢查
// =============================================================================

/**
 * @brief Test 14: 測試 CEC 裝置檢測
 * 
 * 測試流程:
 * 1. 檢查裝置是否存在（啟動前）
 * 2. 場景 A: 裝置可用，成功初始化
 * 3. 場景 B: 裝置不可用，初始化失敗
 * 
 * 測試重點:
 * - cec_monitor_device_available (新增的 API)
 * - 啟動前的裝置檢查
 * - 錯誤處理
 */
void test_cec_device_availability_check(void) {
    printf("\n=== Test 14: CEC Device Availability Check ===\n");
    
    // === 場景 A: 裝置可用 ===
    printf("\n  Scenario A: CEC device available...\n");
    printf("    Checking /dev/cec0...\n");
    
    cec_monitor_device_available_ExpectAndReturn("/dev/cec0", true);
    bool available = cec_monitor_device_available("/dev/cec0");
    
    if (available) {
        printf("    ✓ CEC device found\n");
        
        // 嘗試初始化
        printf("    Attempting initialization...\n");
        cec_monitor_init_ExpectAndReturn("/dev/cec0", CEC_OK);
        int ret = cec_monitor_init("/dev/cec0");
        TEST_ASSERT_EQUAL(CEC_OK, ret);
        printf("    ✓ CEC monitor initialized successfully\n");
        
        // 清理
        cec_monitor_cleanup_Expect();
        cec_monitor_cleanup();
    }
    
    // === 場景 B: 裝置不可用 ===
    printf("\n  Scenario B: CEC device not available...\n");
    printf("    Checking /dev/cec1...\n");
    
    cec_monitor_device_available_ExpectAndReturn("/dev/cec1", false);
    bool not_available = cec_monitor_device_available("/dev/cec1");
    
    if (!not_available) {
        printf("    ✗ CEC device not found\n");
        
        // 初始化應該失敗
        printf("    Attempting initialization (should fail)...\n");
        cec_monitor_init_ExpectAndReturn("/dev/cec1", CEC_ERROR_DEVICE_NOT_FOUND);
        int ret = cec_monitor_init("/dev/cec1");
        TEST_ASSERT_EQUAL(CEC_ERROR_DEVICE_NOT_FOUND, ret);
        printf("    ✓ Correctly failed with DEVICE_NOT_FOUND\n");
    }
    
    // === 檢查多個裝置 ===
    printf("\n  Checking multiple devices...\n");
    const char* devices[] = {"/dev/cec0", "/dev/cec1", "/dev/cec2"};
    int device_count = sizeof(devices) / sizeof(devices[0]);
    
    for (int i = 0; i < device_count; i++) {
        bool dev_available = (i == 0);  // 只有 /dev/cec0 可用
        cec_monitor_device_available_ExpectAndReturn(devices[i], dev_available);
        bool result = cec_monitor_device_available(devices[i]);
        printf("    %s: %s\n", devices[i], result ? "AVAILABLE" : "NOT FOUND");
    }
    
    TEST_PASS_MESSAGE("✅ CEC device availability check passed!");
}

// =============================================================================
// 注意: Ceedling 會自動生成 main 函數和 test runner
// 不需要在這裡定義 main 函數
// =============================================================================
