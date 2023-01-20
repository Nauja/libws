#include "test_config.h"

#define CLIENT_SERVER_PACKET "ping"
#define SERVER_CLIENT_PACKET "pong"
#define PACKET_SIZE 4

static int interrupted = 0;
static int answer_received = 0;

// User data allocated per client
typedef struct user_data
{
    int unused;
} user_data;

static void list_clients(ws *socket, size_t expected)
{
    size_t num_clients = ws_get_num_clients(socket);
    lwsl_user("list of connected clients (%lu):\n", num_clients);
    assert_int_equal(num_clients, expected);
    ws_client *connected;
    for (size_t i = 0; i < num_clients; ++i)
    {
        connected = ws_get_client(socket, i);
        assert_non_null(connected);
        lwsl_user("- %p\n", (void *)connected);
    }
}

// Receive events when server is running
static int server_callback(ws_client *client, ws_event event, void *user)
{
    assert_non_null(client);
    assert_non_null(user);
    // Get pointer to the server websocket
    ws *socket = ws_get_websocket(client);
    assert_non_null(socket);

    switch (event)
    {
    case LIBWS_EVENT_CONNECTED:
        // On client connected to server
        lwsl_user("client %p connected\n", (void *)client);
        // Iterate through all connected clients
        list_clients(socket, 1);
        break;
    case LIBWS_EVENT_RECEIVED:
    {
        // On message received from client
        char in_buf[PACKET_SIZE];
        size_t in_size = ws_receive(client, in_buf, PACKET_SIZE);
        lwsl_user("received %lu bytes: %s\n", in_size, in_buf);
        assert_int_equal(in_size, PACKET_SIZE);
        assert_memory_equal(in_buf, CLIENT_SERVER_PACKET, PACKET_SIZE);

        const char *out_buf = SERVER_CLIENT_PACKET;
        ws_send(client, out_buf, PACKET_SIZE);
    }
    break;
    case LIBWS_EVENT_CLOSED:
        // On client disconnected from server
        lwsl_user("client %p disconnected\n", (void *)client);
        // Iterate through all connected clients
        list_clients(socket, 0);
        // Stop the test
        interrupted = 1;
        break;
    default:
        break;
    }

    return 0;
}

// Receive events when client is running
static int client_callback(ws_client *client, ws_event event, void *user)
{
    assert_non_null(client);
    assert_non_null(user);
    // Get pointer to the client websocket
    ws *socket = ws_get_websocket(client);
    assert_non_null(socket);

    switch (event)
    {
    case LIBWS_EVENT_CONNECTED:
        // On connected to the server
        lwsl_user("LIBWS_EVENT_CONNECTED\n");
        const char *buf = CLIENT_SERVER_PACKET;
        ws_send(client, buf, PACKET_SIZE);
        break;
    case LIBWS_EVENT_RECEIVED:
    {
        // On message received from server
        char in_buf[PACKET_SIZE];
        size_t in_size = ws_receive(client, in_buf, PACKET_SIZE);
        lwsl_user("received %lu bytes: %s\n", in_size, in_buf);
        assert_int_equal(in_size, PACKET_SIZE);
        assert_memory_equal(in_buf, SERVER_CLIENT_PACKET, PACKET_SIZE);
        answer_received = 1;
    }
        return 1;
    case LIBWS_EVENT_CLOSED:
        // On disconnected from the server
        lwsl_user("LIBWS_EVENT_CLOSED\n");
        break;
    default:
        break;
    }

    return 0;
}

// Create the server websocket
static ws *create_server(lws_context *context)
{
    ws_listen_options listen_options;
    memset(&listen_options, 0, sizeof(ws_listen_options));
    listen_options.context = context;
    listen_options.callback = &server_callback;
    listen_options.per_client_data_size = sizeof(user_data);

    return ws_listen(&listen_options);
}

// Create the client websocket
static ws *create_client(lws_context *context, ws *server)
{
    ws_connect_options connect_options;
    memset(&connect_options, 0, sizeof(ws_connect_options));
    connect_options.context = context;
    connect_options.host = "127.0.0.1";
    connect_options.port = ws_get_port(server);
    connect_options.path = "/";
    connect_options.callback = &client_callback;
    connect_options.per_client_data_size = sizeof(user_data);

    return ws_connect(&connect_options);
}

static void test_client_server(void **state)
{
    lws_set_log_level(LLL_DEBUG | LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE, NULL);

    // Create a common lws_context
    lws_context_creation_info info;
    memset(&info, 0, sizeof(lws_context_creation_info));

    lws_context *context = lws_create_context(&info);
    assert_non_null(context);

    // Create the server and client
    ws *server = create_server(context);
    assert_non_null(server);
    ws *client = create_client(context, server);
    assert_non_null(client);

    // Run both
    size_t iter = 0;
    while (!interrupted && iter < 1000)
    {
        lws_service(context, 0);
        ++iter;
    }

    assert_int_equal(answer_received, 1);

    // Delete the server and client
    ws_delete(client);
    ws_delete(server);

    // Destroy the lws_context
    lws_context_destroy(context);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_client_server)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}