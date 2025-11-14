/**
 * @file test_websocket_server.c
 * @brief WebSocket Server 單元測試
 */

// POSIX headers for strdup
#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "websocket_server.h"
#include <string.h>
#include <stdlib.h>

// 測試回調計數器
static int g_message_count = 0;
static int g_connect_count = 0;
static int g_disconnect_count = 0;
static char g_last_message[256] = {0};

// 測試輔助函數宣告 (在 websocket_server.c 中實作)
#ifdef TESTING
extern int ws_server_test_add_client(const char *ip, uint16_t port);
extern int ws_server_test_remove_client(int client_id);
extern char* ws_server_test_handle_message(int client_id, const char *message);
#endif

void setUp(void) {
    // 每個測試前初始化
    ws_server_init(8080);
    g_message_count = 0;
    g_connect_count = 0;
    g_disconnect_count = 0;
    memset(g_last_message, 0, sizeof(g_last_message));
}

void tearDown(void) {
    // 每個測試後清理
    ws_server_cleanup();
}

// ============================================
// 測試回調函數
// ============================================

static char* test_message_handler(int client_id, ws_message_type_t msg_type,
                                   const char *payload, void *user_data) {
    (void)user_data;
    g_message_count++;
    snprintf(g_last_message, sizeof(g_last_message), "client_%d:%d", client_id, msg_type);
    
    // 根據訊息類型返回不同回應
    if (msg_type == WS_MSG_QUERY_PS5) {
        return strdup("{\"type\":\"ps5_status\",\"status\":\"on\"}");
    } else if (msg_type == WS_MSG_WAKE_PS5) {
        return strdup("{\"type\":\"wake_response\",\"status\":\"success\"}");
    } else if (msg_type == WS_MSG_PING) {
        return strdup("{\"type\":\"pong\"}");
    }
    
    return NULL;
}

static void test_connect_callback(int client_id, const char *client_ip, void *user_data) {
    (void)client_id;
    (void)client_ip;
    (void)user_data;
    g_connect_count++;
}

static void test_disconnect_callback(int client_id, void *user_data) {
    (void)client_id;
    (void)user_data;
    g_disconnect_count++;
}

// ============================================
// 初始化測試
// ============================================

void test_ws_server_init_should_succeed(void) {
    ws_server_cleanup();
    
    int result = ws_server_init(8080);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(WS_SERVER_STOPPED, ws_server_get_state());
    TEST_ASSERT_EQUAL(8080, ws_server_get_port());
}

void test_ws_server_init_with_zero_port_should_use_default(void) {
    ws_server_cleanup();
    
    int result = ws_server_init(0);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(WS_SERVER_DEFAULT_PORT, ws_server_get_port());
}

void test_ws_server_init_multiple_times_should_fail(void) {
    int result = ws_server_init(8080);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// 啟動/停止測試
// ============================================

void test_ws_server_start_should_succeed(void) {
    int result = ws_server_start();
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(WS_SERVER_RUNNING, ws_server_get_state());
}

void test_ws_server_start_without_init_should_fail(void) {
    ws_server_cleanup();
    
    int result = ws_server_start();
    
    TEST_ASSERT_EQUAL(-1, result);
}

void test_ws_server_stop_should_succeed(void) {
    ws_server_start();
    
    ws_server_stop();
    
    TEST_ASSERT_EQUAL(WS_SERVER_STOPPED, ws_server_get_state());
}

void test_ws_server_start_stop_multiple_times(void) {
    for (int i = 0; i < 3; i++) {
        int result = ws_server_start();
        TEST_ASSERT_EQUAL(0, result);
        TEST_ASSERT_EQUAL(WS_SERVER_RUNNING, ws_server_get_state());
        
        ws_server_stop();
        TEST_ASSERT_EQUAL(WS_SERVER_STOPPED, ws_server_get_state());
    }
}

// ============================================
// 客戶端管理測試
// ============================================

void test_ws_server_add_client_should_succeed(void) {
    ws_server_start();
    
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    
    TEST_ASSERT_TRUE(client_id > 0);
    TEST_ASSERT_EQUAL(1, ws_server_get_client_count());
}

void test_ws_server_add_multiple_clients(void) {
    ws_server_start();
    
    int id1 = ws_server_test_add_client("192.168.1.101", 12345);
    int id2 = ws_server_test_add_client("192.168.1.102", 12346);
    int id3 = ws_server_test_add_client("192.168.1.103", 12347);
    
    TEST_ASSERT_TRUE(id1 > 0);
    TEST_ASSERT_TRUE(id2 > 0);
    TEST_ASSERT_TRUE(id3 > 0);
    TEST_ASSERT_NOT_EQUAL(id1, id2);
    TEST_ASSERT_NOT_EQUAL(id2, id3);
    TEST_ASSERT_EQUAL(3, ws_server_get_client_count());
}

void test_ws_server_remove_client_should_succeed(void) {
    ws_server_start();
    
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    TEST_ASSERT_EQUAL(1, ws_server_get_client_count());
    
    int result = ws_server_test_remove_client(client_id);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(0, ws_server_get_client_count());
}

void test_ws_server_get_clients_list(void) {
    ws_server_start();
    
    ws_server_test_add_client("192.168.1.101", 12345);
    ws_server_test_add_client("192.168.1.102", 12346);
    
    ws_client_info_t clients[10];
    int count = ws_server_get_clients(clients, 10);
    
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_TRUE(clients[0].active);
    TEST_ASSERT_TRUE(clients[1].active);
}

// ============================================
// 回調測試
// ============================================

void test_ws_server_connect_callback_should_be_called(void) {
    ws_server_set_connect_callback(test_connect_callback, NULL);
    ws_server_start();
    
    ws_server_test_add_client("192.168.1.100", 12345);
    
    TEST_ASSERT_EQUAL(1, g_connect_count);
}

void test_ws_server_disconnect_callback_should_be_called(void) {
    ws_server_set_disconnect_callback(test_disconnect_callback, NULL);
    ws_server_start();
    
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    ws_server_test_remove_client(client_id);
    
    TEST_ASSERT_EQUAL(1, g_disconnect_count);
}

void test_ws_server_message_handler_should_be_called(void) {
    ws_server_set_message_handler(test_message_handler, NULL);
    ws_server_start();
    
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    
    const char *message = "{\"type\":\"query_ps5\"}";
    char *response = ws_server_test_handle_message(client_id, message);
    
    TEST_ASSERT_EQUAL(1, g_message_count);
    TEST_ASSERT_NOT_NULL(response);
    free(response);
}

// ============================================
// 訊息處理測試
// ============================================

void test_ws_server_handle_query_ps5_message(void) {
    ws_server_set_message_handler(test_message_handler, NULL);
    ws_server_start();
    
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    
    const char *message = "{\"type\":\"query_ps5\"}";
    char *response = ws_server_test_handle_message(client_id, message);
    
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "ps5_status") != NULL);
    free(response);
}

void test_ws_server_handle_wake_ps5_message(void) {
    ws_server_set_message_handler(test_message_handler, NULL);
    ws_server_start();
    
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    
    const char *message = "{\"type\":\"wake_ps5\"}";
    char *response = ws_server_test_handle_message(client_id, message);
    
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "wake_response") != NULL);
    free(response);
}

void test_ws_server_handle_ping_message(void) {
    ws_server_set_message_handler(test_message_handler, NULL);
    ws_server_start();
    
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    
    const char *message = "{\"type\":\"ping\"}";
    char *response = ws_server_test_handle_message(client_id, message);
    
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "pong") != NULL);
    free(response);
}

// ============================================
// 發送訊息測試
// ============================================

void test_ws_server_send_to_client_should_succeed(void) {
    ws_server_start();
    
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    
    int result = ws_server_send(client_id, "{\"type\":\"test\"}");
    
    TEST_ASSERT_EQUAL(0, result);
}

void test_ws_server_send_to_invalid_client_should_fail(void) {
    ws_server_start();
    
    int result = ws_server_send(999, "{\"type\":\"test\"}");
    
    TEST_ASSERT_EQUAL(-2, result);
}

void test_ws_server_broadcast_should_send_to_all_clients(void) {
    ws_server_start();
    
    ws_server_test_add_client("192.168.1.101", 12345);
    ws_server_test_add_client("192.168.1.102", 12346);
    ws_server_test_add_client("192.168.1.103", 12347);
    
    int sent_count = ws_server_broadcast("{\"type\":\"broadcast_test\"}");
    
    TEST_ASSERT_EQUAL(3, sent_count);
}

void test_ws_server_broadcast_with_no_clients(void) {
    ws_server_start();
    
    int sent_count = ws_server_broadcast("{\"type\":\"test\"}");
    
    TEST_ASSERT_EQUAL(0, sent_count);
}

// ============================================
// Service 測試
// ============================================

void test_ws_server_service_should_succeed(void) {
    ws_server_start();
    
    int result = ws_server_service(100);
    
    TEST_ASSERT_EQUAL(0, result);
}

void test_ws_server_service_without_start_should_fail(void) {
    int result = ws_server_service(100);
    
    TEST_ASSERT_EQUAL(-1, result);
}

// ============================================
// 字串轉換測試
// ============================================

void test_ws_message_type_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("query_ps5", ws_message_type_to_string(WS_MSG_QUERY_PS5));
    TEST_ASSERT_EQUAL_STRING("wake_ps5", ws_message_type_to_string(WS_MSG_WAKE_PS5));
    TEST_ASSERT_EQUAL_STRING("ping", ws_message_type_to_string(WS_MSG_PING));
    TEST_ASSERT_EQUAL_STRING("pong", ws_message_type_to_string(WS_MSG_PONG));
}

void test_ws_server_state_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("STOPPED", ws_server_state_to_string(WS_SERVER_STOPPED));
    TEST_ASSERT_EQUAL_STRING("RUNNING", ws_server_state_to_string(WS_SERVER_RUNNING));
}

void test_ws_server_error_string(void) {
    const char *msg = ws_server_error_string(0);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("Success", msg);
}

// ============================================
// 整合測試
// ============================================

void test_ws_server_full_workflow(void) {
    // 1. 設定回調
    ws_server_set_message_handler(test_message_handler, NULL);
    ws_server_set_connect_callback(test_connect_callback, NULL);
    ws_server_set_disconnect_callback(test_disconnect_callback, NULL);
    
    // 2. 啟動伺服器
    int result = ws_server_start();
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(WS_SERVER_RUNNING, ws_server_get_state());
    
    // 3. 客戶端連線
    int client_id = ws_server_test_add_client("192.168.1.100", 12345);
    TEST_ASSERT_TRUE(client_id > 0);
    TEST_ASSERT_EQUAL(1, g_connect_count);
    TEST_ASSERT_EQUAL(1, ws_server_get_client_count());
    
    // 4. 處理訊息
    char *response = ws_server_test_handle_message(client_id, "{\"type\":\"query_ps5\"}");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(1, g_message_count);
    free(response);
    
    // 5. 發送訊息
    result = ws_server_send(client_id, "{\"type\":\"status_update\"}");
    TEST_ASSERT_EQUAL(0, result);
    
    // 6. 客戶端斷線
    result = ws_server_test_remove_client(client_id);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, g_disconnect_count);
    TEST_ASSERT_EQUAL(0, ws_server_get_client_count());
    
    // 7. 停止伺服器
    ws_server_stop();
    TEST_ASSERT_EQUAL(WS_SERVER_STOPPED, ws_server_get_state());
}

void test_ws_server_multiple_clients_workflow(void) {
    ws_server_set_message_handler(test_message_handler, NULL);
    ws_server_start();
    
    // 添加多個客戶端
    int id1 = ws_server_test_add_client("192.168.1.101", 12345);
    int id2 = ws_server_test_add_client("192.168.1.102", 12346);
    int id3 = ws_server_test_add_client("192.168.1.103", 12347);
    
    // 驗證所有客戶端 ID 有效
    TEST_ASSERT_TRUE(id1 > 0);
    TEST_ASSERT_TRUE(id2 > 0);
    TEST_ASSERT_TRUE(id3 > 0);
    
    TEST_ASSERT_EQUAL(3, ws_server_get_client_count());
    
    // 廣播訊息
    int sent = ws_server_broadcast("{\"type\":\"server_message\"}");
    TEST_ASSERT_EQUAL(3, sent);
    
    // 移除一個客戶端
    ws_server_test_remove_client(id2);
    TEST_ASSERT_EQUAL(2, ws_server_get_client_count());
    
    // 再次廣播
    sent = ws_server_broadcast("{\"type\":\"server_message\"}");
    TEST_ASSERT_EQUAL(2, sent);
}
