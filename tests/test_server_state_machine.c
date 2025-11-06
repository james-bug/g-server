/**
 * @file test_server_state_machine.c
 * @brief Server State Machine 單元測試
 */

// POSIX headers
#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "server_state_machine.h"
#include <string.h>

// 測試用上下文
static server_context_t g_ctx;

void setUp(void) {
    // 每個測試前初始化
    server_sm_init(&g_ctx);
}

void tearDown(void) {
    // 每個測試後清理
    server_sm_cleanup(&g_ctx);
}

// ============================================
// 初始化測試
// ============================================

void test_server_sm_init_should_succeed(void) {
    server_context_t ctx;
    
    int result = server_sm_init(&ctx);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_TRUE(ctx.initialized);
    TEST_ASSERT_TRUE(ctx.running);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, ctx.state);
    
    server_sm_cleanup(&ctx);
}

void test_server_sm_init_with_null_should_fail(void) {
    int result = server_sm_init(NULL);
    
    TEST_ASSERT_EQUAL(-1, result);
}

void test_server_sm_init_should_initialize_ps5_status(void) {
    TEST_ASSERT_EQUAL(PS5_POWER_UNKNOWN, g_ctx.ps5_status.cec_state);
    TEST_ASSERT_FALSE(g_ctx.ps5_status.network_online);
}

// ============================================
// 狀態轉換測試
// ============================================

void test_server_sm_idle_to_monitoring_on_cec_change(void) {
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
    
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_CEC_CHANGE);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_MONITORING, server_sm_get_state(&g_ctx));
}

void test_server_sm_idle_to_querying_on_client_query(void) {
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
    
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_CLIENT_QUERY);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_QUERYING, server_sm_get_state(&g_ctx));
}

void test_server_sm_idle_to_waking_on_wake_request(void) {
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
    
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_WAKE_REQUEST);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_WAKING, server_sm_get_state(&g_ctx));
}

void test_server_sm_monitoring_to_broadcasting_on_completed(void) {
    server_sm_handle_event(&g_ctx, SERVER_EVENT_CEC_CHANGE);
    TEST_ASSERT_EQUAL(SERVER_STATE_MONITORING, server_sm_get_state(&g_ctx));
    
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_BROADCASTING, server_sm_get_state(&g_ctx));
}

void test_server_sm_monitoring_to_error_on_error_event(void) {
    server_sm_handle_event(&g_ctx, SERVER_EVENT_CEC_CHANGE);
    TEST_ASSERT_EQUAL(SERVER_STATE_MONITORING, server_sm_get_state(&g_ctx));
    
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_ERROR);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_ERROR, server_sm_get_state(&g_ctx));
}

void test_server_sm_querying_to_idle_on_completed(void) {
    server_sm_handle_event(&g_ctx, SERVER_EVENT_CLIENT_QUERY);
    TEST_ASSERT_EQUAL(SERVER_STATE_QUERYING, server_sm_get_state(&g_ctx));
    
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
}

void test_server_sm_waking_to_monitoring_on_completed(void) {
    server_sm_handle_event(&g_ctx, SERVER_EVENT_WAKE_REQUEST);
    TEST_ASSERT_EQUAL(SERVER_STATE_WAKING, server_sm_get_state(&g_ctx));
    
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_MONITORING, server_sm_get_state(&g_ctx));
}

void test_server_sm_broadcasting_to_idle_on_completed(void) {
    // 先到 MONITORING
    server_sm_handle_event(&g_ctx, SERVER_EVENT_CEC_CHANGE);
    // 再到 BROADCASTING
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    TEST_ASSERT_EQUAL(SERVER_STATE_BROADCASTING, server_sm_get_state(&g_ctx));
    
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
}

void test_server_sm_error_to_idle_on_none_event(void) {
    // 先進入錯誤狀態
    server_sm_handle_event(&g_ctx, SERVER_EVENT_CEC_CHANGE);
    server_sm_handle_event(&g_ctx, SERVER_EVENT_ERROR);
    TEST_ASSERT_EQUAL(SERVER_STATE_ERROR, server_sm_get_state(&g_ctx));
    
    // 發送 NONE 事件來重置
    int result = server_sm_handle_event(&g_ctx, SERVER_EVENT_NONE);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
}

// ============================================
// CEC 狀態更新測試
// ============================================

void test_server_sm_update_cec_state_should_succeed(void) {
    int result = server_sm_update_cec_state(&g_ctx, PS5_POWER_ON);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(PS5_POWER_ON, g_ctx.ps5_status.cec_state);
}

void test_server_sm_update_cec_state_should_trigger_event(void) {
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
    
    server_sm_update_cec_state(&g_ctx, PS5_POWER_ON);
    
    // 應該轉到 MONITORING 狀態
    TEST_ASSERT_EQUAL(SERVER_STATE_MONITORING, server_sm_get_state(&g_ctx));
}

void test_server_sm_update_cec_state_with_same_value_should_not_trigger(void) {
    // 第一次更新
    server_sm_update_cec_state(&g_ctx, PS5_POWER_ON);
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);  // 回到 BROADCASTING
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);  // 回到 IDLE
    
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
    
    // 第二次更新相同值
    server_sm_update_cec_state(&g_ctx, PS5_POWER_ON);
    
    // 不應該觸發狀態變化
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
}

void test_server_sm_update_cec_state_with_null_should_fail(void) {
    int result = server_sm_update_cec_state(NULL, PS5_POWER_ON);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// 網路狀態更新測試
// ============================================

void test_server_sm_update_network_state_should_succeed(void) {
    int result = server_sm_update_network_state(&g_ctx, true);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_TRUE(g_ctx.ps5_status.network_online);
}

void test_server_sm_update_network_state_with_null_should_fail(void) {
    int result = server_sm_update_network_state(NULL, true);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// PS5 資訊更新測試
// ============================================

void test_server_sm_update_ps5_info_should_succeed(void) {
    ps5_info_t info;
    memset(&info, 0, sizeof(ps5_info_t));
    snprintf(info.ip, sizeof(info.ip), "192.168.1.100");
    snprintf(info.mac, sizeof(info.mac), "AA:BB:CC:DD:EE:FF");
    info.online = true;
    
    int result = server_sm_update_ps5_info(&g_ctx, &info);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", g_ctx.ps5_status.info.ip);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", g_ctx.ps5_status.info.mac);
    TEST_ASSERT_TRUE(g_ctx.ps5_status.network_online);
}

void test_server_sm_update_ps5_info_with_null_should_fail(void) {
    int result = server_sm_update_ps5_info(NULL, NULL);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// PS5 狀態判斷測試
// ============================================

void test_server_sm_get_ps5_status_cec_on_network_online(void) {
    server_sm_update_cec_state(&g_ctx, PS5_POWER_ON);
    server_sm_update_network_state(&g_ctx, true);
    
    const char *status = server_sm_get_ps5_status(&g_ctx);
    
    TEST_ASSERT_EQUAL_STRING("on", status);
}

void test_server_sm_get_ps5_status_cec_on_network_offline(void) {
    server_sm_update_cec_state(&g_ctx, PS5_POWER_ON);
    server_sm_update_network_state(&g_ctx, false);
    
    const char *status = server_sm_get_ps5_status(&g_ctx);
    
    TEST_ASSERT_EQUAL_STRING("starting", status);
}

void test_server_sm_get_ps5_status_cec_standby(void) {
    server_sm_update_cec_state(&g_ctx, PS5_POWER_STANDBY);
    
    const char *status = server_sm_get_ps5_status(&g_ctx);
    
    TEST_ASSERT_EQUAL_STRING("standby", status);
}

void test_server_sm_get_ps5_status_cec_off(void) {
    server_sm_update_cec_state(&g_ctx, PS5_POWER_OFF);
    
    const char *status = server_sm_get_ps5_status(&g_ctx);
    
    TEST_ASSERT_EQUAL_STRING("off", status);
}

void test_server_sm_get_ps5_status_cec_unknown_network_online(void) {
    server_sm_update_cec_state(&g_ctx, PS5_POWER_UNKNOWN);
    server_sm_update_network_state(&g_ctx, true);
    
    const char *status = server_sm_get_ps5_status(&g_ctx);
    
    TEST_ASSERT_EQUAL_STRING("on", status);
}

void test_server_sm_get_ps5_status_all_unknown(void) {
    const char *status = server_sm_get_ps5_status(&g_ctx);
    
    TEST_ASSERT_EQUAL_STRING("unknown", status);
}

// ============================================
// 錯誤狀態測試
// ============================================

void test_server_sm_is_error_should_return_false_when_normal(void) {
    TEST_ASSERT_FALSE(server_sm_is_error(&g_ctx));
}

void test_server_sm_is_error_should_return_true_when_error_state(void) {
    server_sm_handle_event(&g_ctx, SERVER_EVENT_CEC_CHANGE);
    server_sm_handle_event(&g_ctx, SERVER_EVENT_ERROR);
    
    TEST_ASSERT_TRUE(server_sm_is_error(&g_ctx));
}

void test_server_sm_is_error_with_null_should_return_true(void) {
    TEST_ASSERT_TRUE(server_sm_is_error(NULL));
}

// ============================================
// 停止與清理測試
// ============================================

void test_server_sm_stop_should_stop_running(void) {
    TEST_ASSERT_TRUE(g_ctx.running);
    
    server_sm_stop(&g_ctx);
    
    TEST_ASSERT_FALSE(g_ctx.running);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
}

void test_server_sm_cleanup_should_reset_context(void) {
    server_sm_update_cec_state(&g_ctx, PS5_POWER_ON);
    
    server_sm_cleanup(&g_ctx);
    
    TEST_ASSERT_FALSE(g_ctx.initialized);
    TEST_ASSERT_FALSE(g_ctx.running);
}

// ============================================
// 更新機制測試
// ============================================

void test_server_sm_update_should_succeed(void) {
    int result = server_sm_update(&g_ctx);
    
    TEST_ASSERT_EQUAL(0, result);
}

void test_server_sm_update_with_null_should_fail(void) {
    int result = server_sm_update(NULL);
    
    TEST_ASSERT_EQUAL(-1, result);
}

void test_server_sm_update_when_not_running_should_fail(void) {
    server_sm_stop(&g_ctx);
    
    int result = server_sm_update(&g_ctx);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// 字串轉換測試
// ============================================

void test_server_state_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("IDLE", server_state_to_string(SERVER_STATE_IDLE));
    TEST_ASSERT_EQUAL_STRING("MONITORING", server_state_to_string(SERVER_STATE_MONITORING));
    TEST_ASSERT_EQUAL_STRING("DETECTING", server_state_to_string(SERVER_STATE_DETECTING));
    TEST_ASSERT_EQUAL_STRING("QUERYING", server_state_to_string(SERVER_STATE_QUERYING));
    TEST_ASSERT_EQUAL_STRING("WAKING", server_state_to_string(SERVER_STATE_WAKING));
    TEST_ASSERT_EQUAL_STRING("BROADCASTING", server_state_to_string(SERVER_STATE_BROADCASTING));
    TEST_ASSERT_EQUAL_STRING("ERROR", server_state_to_string(SERVER_STATE_ERROR));
}

void test_server_event_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("NONE", server_event_to_string(SERVER_EVENT_NONE));
    TEST_ASSERT_EQUAL_STRING("CEC_CHANGE", server_event_to_string(SERVER_EVENT_CEC_CHANGE));
    TEST_ASSERT_EQUAL_STRING("CLIENT_QUERY", server_event_to_string(SERVER_EVENT_CLIENT_QUERY));
    TEST_ASSERT_EQUAL_STRING("WAKE_REQUEST", server_event_to_string(SERVER_EVENT_WAKE_REQUEST));
    TEST_ASSERT_EQUAL_STRING("COMPLETED", server_event_to_string(SERVER_EVENT_COMPLETED));
    TEST_ASSERT_EQUAL_STRING("ERROR", server_event_to_string(SERVER_EVENT_ERROR));
}

void test_server_sm_error_string(void) {
    const char *msg = server_sm_error_string(0);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("Success", msg);
}

// ============================================
// 整合測試
// ============================================

void test_server_sm_full_query_workflow(void) {
    // 1. 初始狀態: IDLE
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
    
    // 2. 客戶端查詢
    server_sm_handle_event(&g_ctx, SERVER_EVENT_CLIENT_QUERY);
    TEST_ASSERT_EQUAL(SERVER_STATE_QUERYING, server_sm_get_state(&g_ctx));
    
    // 3. 查詢完成
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
}

void test_server_sm_full_wake_workflow(void) {
    // 1. 初始狀態: IDLE
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
    
    // 2. 喚醒請求
    server_sm_handle_event(&g_ctx, SERVER_EVENT_WAKE_REQUEST);
    TEST_ASSERT_EQUAL(SERVER_STATE_WAKING, server_sm_get_state(&g_ctx));
    
    // 3. 喚醒完成,進入監控
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    TEST_ASSERT_EQUAL(SERVER_STATE_MONITORING, server_sm_get_state(&g_ctx));
    
    // 4. 監控完成,廣播
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    TEST_ASSERT_EQUAL(SERVER_STATE_BROADCASTING, server_sm_get_state(&g_ctx));
    
    // 5. 廣播完成,回到 IDLE
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
}

void test_server_sm_full_cec_change_workflow(void) {
    // 1. 更新 CEC 狀態
    server_sm_update_cec_state(&g_ctx, PS5_POWER_ON);
    TEST_ASSERT_EQUAL(SERVER_STATE_MONITORING, server_sm_get_state(&g_ctx));
    
    // 2. 更新網路狀態
    server_sm_update_network_state(&g_ctx, true);
    
    // 3. 檢查綜合狀態
    const char *status = server_sm_get_ps5_status(&g_ctx);
    TEST_ASSERT_EQUAL_STRING("on", status);
    
    // 4. 監控完成
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    TEST_ASSERT_EQUAL(SERVER_STATE_BROADCASTING, server_sm_get_state(&g_ctx));
    
    // 5. 廣播完成
    server_sm_handle_event(&g_ctx, SERVER_EVENT_COMPLETED);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
}

void test_server_sm_error_recovery_workflow(void) {
    // 1. 進入錯誤狀態
    server_sm_handle_event(&g_ctx, SERVER_EVENT_CEC_CHANGE);
    server_sm_handle_event(&g_ctx, SERVER_EVENT_ERROR);
    TEST_ASSERT_EQUAL(SERVER_STATE_ERROR, server_sm_get_state(&g_ctx));
    TEST_ASSERT_TRUE(server_sm_is_error(&g_ctx));
    
    // 2. 恢復
    server_sm_handle_event(&g_ctx, SERVER_EVENT_NONE);
    TEST_ASSERT_EQUAL(SERVER_STATE_IDLE, server_sm_get_state(&g_ctx));
    TEST_ASSERT_FALSE(server_sm_is_error(&g_ctx));
}
