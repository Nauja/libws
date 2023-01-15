#include "ws.h"

#include "libwebsockets.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

typedef enum lws_write_protocol lws_write_protocol;
typedef struct lws lws;
typedef struct lws_ring lws_ring;
typedef struct lws_vhost lws_vhost;
typedef struct lws_context lws_context;
typedef struct lws_protocols lws_protocols;
typedef struct lws_protocol_vhost_options lws_protocol_vhost_options;
typedef struct lws_context_creation_info lws_context_creation_info;
typedef struct lws_client_connect_info lws_client_connect_info;
typedef struct lws_sorted_usec_list lws_sorted_usec_list;

typedef enum ws_event ws_event;
typedef struct ws_hooks ws_hooks;
typedef struct ws_connect_options ws_connect_options;
typedef struct ws_listen_options ws_listen_options;
typedef int (*ws_callback)(struct ws_client *client, enum ws_event event, void *user);

#define LIBWS_RX_SIZE 1024
#define RING_DEPTH 4096
#define LIBWS_TRUE 1
#define LIBWS_FALSE 0
#define LIBWS_BUFFER_SIZE 1024
#define LIBWS_UNUSED(x) (void)(x)

#if defined(_MSC_VER)
/* work around MSVC error C2322: '...' address of dllimport '...' is not static */
static void *LIBWS_CDECL internal_malloc(size_t size)
{
	return LIBWS_MALLOC(size);
}

static void LIBWS_CDECL internal_free(void *pointer)
{
	LIBWS_FREE(pointer);
}
#else
#define internal_malloc LIBWS_MALLOC
#define internal_free LIBWS_FREE
#endif

static ws_hooks ws_global_hooks = {
	internal_malloc,
	internal_free};

LIBWS_PUBLIC(void)
ws_init_hooks(ws_hooks *hooks)
{
	ws_global_hooks.malloc_fn = hooks->malloc_fn;
	ws_global_hooks.free_fn = hooks->free_fn;
}

#define _LIBWS_MALLOC ws_global_hooks.malloc_fn
#define _LIBWS_FREE ws_global_hooks.free_fn

typedef struct ws_msg
{
	void *data[LIBWS_BUFFER_SIZE + LWS_PRE];
	size_t size;
} ws_msg;

static ws_msg msg;

typedef struct ws_client
{
	/* Parent websocket */
	struct ws *ws;
	/* Queued packets to receive from client. */
	lws_ring *receive_queue;
	/* Queued packets to send to client. */
	lws_ring *send_queue;
	lws_sorted_usec_list_t sul; /* schedule connection retry */
	struct lws *wsi;			/* related wsi if any */
	uint16_t retry_count;		/* count of consequetive retries */
	/* User data. */
	void *user;
} ws_client;

typedef struct ws_vhd
{
	lws_context *context;
	lws_vhost *vhost;
	lws *client_wsi;
	struct ws *ws;

	lws_sorted_usec_list_t sul;

	int *interrupted;
	int *options;
	const char **url;
	const char **ads;
	const char **iface;
	int *port;
} ws_vhd;

typedef struct ws
{
	lws_context *context;
	/* Virtual host. */
	lws_vhost *vhost;
	/* Callback for events. */
	ws_callback callback;
	/* Path to connect to. */
	const char *path;
	/* Host to connect to. */
	const char *host;
	/* Port to listen or connect. */
	int port;
	/* User data to allocate per client. */
	size_t per_client_data_size;
} ws;

static int ws_client_init(ws_client *client)
{
	client->receive_queue = lws_ring_create(
		sizeof(ws_msg),
		RING_DEPTH,
		NULL);

	if (!client->receive_queue)
	{
		return LIBWS_FALSE;
	}

	client->send_queue = lws_ring_create(
		sizeof(ws_msg),
		RING_DEPTH,
		NULL);

	if (!client->send_queue)
	{
		lws_ring_destroy(client->receive_queue);
		return LIBWS_FALSE;
	}

	client->user = _LIBWS_MALLOC(client->ws->per_client_data_size);
	if (!client->user)
	{
		lws_ring_destroy(client->send_queue);
		lws_ring_destroy(client->receive_queue);
		return LIBWS_FALSE;
	}

	memset(client->user, 0, client->ws->per_client_data_size);

	return LIBWS_TRUE;
}

static void ws_client_delete(ws_client *client)
{
	lws_ring_destroy(client->send_queue);
	lws_ring_destroy(client->receive_queue);
	_LIBWS_FREE(client->user);
}

static void ws_try_connect(lws_sorted_usec_list *sul)
{
	ws_vhd *vhd = lws_container_of(sul, ws_vhd, sul);
	ws *ws = vhd->ws;
	struct lws_client_connect_info i;
	char host[128];

	lws_snprintf(host, sizeof(host), "%s:%u", ws->host, ws->port);

	memset(&i, 0, sizeof(i));

	i.context = vhd->context;
	i.port = ws->port;
	i.address = ws->host;
	i.path = ws->path;
	i.host = host;
	i.origin = host;
	i.ssl_connection = 0;
	i.vhost = vhd->vhost;
	i.iface = NULL;
	// i.protocol = ;
	i.pwsi = NULL;

	lwsl_user("connecting to %s:%d/%s\n", i.address, i.port, i.path);

	if (!lws_client_connect_via_info(&i))
	{

		lwsl_user("lws_client_connect_via_info failed\n");
		lws_sul_schedule(vhd->context, 0, &vhd->sul,
						 ws_try_connect, 10 * LWS_US_PER_SEC);
		return;
	}

	lwsl_user("lws_client_connect_via_info done\n");
}

#define LIBWS_CALLBACK(client, event)                             \
	if (client->ws && client->ws->callback)                       \
	{                                                             \
		return client->ws->callback(client, event, client->user); \
	}

static int ws_client_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	ws_client *client = (ws_client *)user;
	ws_vhd *vhd = (ws_vhd *)
		lws_protocol_vh_priv_get(lws_get_vhost(wsi),
								 lws_get_protocol(wsi));
	const ws_msg *write_packet;
	ws_msg read_packet;
	int m;

	switch (reason)
	{
	case LWS_CALLBACK_PROTOCOL_INIT:
		lwsl_user("LWS_CALLBACK_PROTOCOL_INIT\n");

		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
										  lws_get_protocol(wsi),
										  sizeof(ws_vhd));
		if (!vhd)
			return -1;

		vhd->context = lws_get_context(wsi);
		vhd->vhost = lws_get_vhost(wsi);
		vhd->ws = (ws *)lws_get_protocol(wsi)->user;

		ws_try_connect(&vhd->sul);

		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		lwsl_user("LWS_CALLBACK_PROTOCOL_DESTROY\n");

		lws_sul_cancel(&vhd->sul);

		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_user("LWS_CALLBACK_CLIENT_ESTABLISHED\n");

		client->ws = (ws *)lws_get_protocol(wsi)->user;
		if (!ws_client_init(client))
		{
			return 1;
		}

		client->wsi = wsi;

		LIBWS_CALLBACK(client, LIBWS_EVENT_CONNECTED);

		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		lwsl_user("LWS_CALLBACK_CLIENT_WRITEABLE\n");

		write_packet = lws_ring_get_element(client->send_queue, 0);
		if (!write_packet)
		{
			break;
		}

		/* Notice we allowed for LWS_PRE in the payload already */
		m = lws_write(wsi, (unsigned char *)&write_packet->data[LWS_PRE], write_packet->size, LWS_WRITE_BINARY);
		if (m < (int)write_packet->size)
		{
			lwsl_err("ERROR %d writing to ws socket\n", m);
			return -1;
		}

		lws_ring_consume(client->send_queue, NULL, NULL, 1);
		lws_callback_on_writable(wsi);

		LIBWS_CALLBACK(client, LIBWS_EVENT_SENT);

		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE: %4d (rpp %5d)\n",
				  (int)len, (int)lws_remaining_packet_payload(wsi));

		/* notice we over-allocate by LWS_PRE */
		if (len > LIBWS_BUFFER_SIZE)
		{
			lwsl_user("OOM: packet too large\n");
			break;
		}

		/* Copy incoming data to read buffer */
		memcpy(read_packet.data, in, len);
		read_packet.size = len;
		lws_ring_insert(client->receive_queue, &read_packet, 1);

		LIBWS_CALLBACK(client, LIBWS_EVENT_RECEIVED);

		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
		vhd->client_wsi = NULL;

		LIBWS_CALLBACK(client, LIBWS_EVENT_CONNECTION_ERROR);

		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_user("LWS_CALLBACK_CLIENT_CLOSED\n");

		LIBWS_CALLBACK(client, LIBWS_EVENT_CLOSED);

		ws_client_delete(client);
		break;

	default:
		break;
	}

	return 0;
}

static int ws_server_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	LIBWS_UNUSED(wsi);
	LIBWS_UNUSED(reason);
	LIBWS_UNUSED(user);
	LIBWS_UNUSED(in);
	LIBWS_UNUSED(len);
	return 0;
}

static struct ws *ws_create(void)
{
	struct ws *o = (struct ws *)_LIBWS_MALLOC(sizeof(struct ws));
	if (!o)
	{
		return NULL;
	}

	memset(o, 0, sizeof(struct ws));
	return o;
}

LIBWS_PUBLIC(struct ws *)
ws_connect(const ws_connect_options *options)
{
	struct ws *ws = ws_create();
	if (!ws)
	{
		return NULL;
	}

	lws_protocols protocols[] = {
		{"ws", ws_client_callback, sizeof(ws_client), LIBWS_RX_SIZE, 0, (void *)ws, 0},
		{NULL, NULL, 0, 0, 0, NULL, 0} /* terminator */
	};

	lws_context_creation_info info;
	memset(&info, 0, sizeof(lws_context_creation_info));
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	ws->vhost = lws_create_vhost(options->context, &info);
	if (!ws->vhost)
	{
		ws_delete(ws);
		return NULL;
	}

	ws->path = options->path;
	ws->host = options->host;
	ws->port = options->port;

	return ws;
}

LIBWS_PUBLIC(struct ws *)
ws_listen(const ws_listen_options *options)
{
	struct ws *ws = ws_create();
	if (!ws)
	{
		return NULL;
	}

	lws_protocols protocols[] = {
		{"ws", ws_server_callback, sizeof(ws_client), LIBWS_RX_SIZE, 0, (void *)ws, 0},
		{NULL, NULL, 0, 0, 0, NULL, 0} /* terminator */
	};

	lws_context_creation_info info;
	memset(&info, 0, sizeof(lws_context_creation_info));
	info.port = options->port;
	info.protocols = protocols;
	ws->vhost = lws_create_vhost(options->context, &info);
	if (!ws->vhost)
	{
		ws_delete(ws);
		return NULL;
	}

	ws->port = lws_get_vhost_listen_port(ws->vhost);

	return ws;
}

LIBWS_PUBLIC(void)
ws_send(ws_client *client, const void *buf, size_t size)
{
	if (!(int)lws_ring_get_count_free_elements(client->send_queue))
	{
		lwsl_err("Send queue is full\n");
		return;
	}

	if (size > msg.size)
	{
		lwsl_err("Packet too large to send %ld > %ld, try increase LIBWS_BUFFER_SIZE\n", size, msg.size);
		return;
	}

	memcpy(&msg.data[LWS_PRE], buf, size);
	msg.size = size;

	if (!lws_ring_insert(client->send_queue, &msg, 1))
	{
		lwsl_err("Failed to add to send queue\n");
		return;
	}

	lws_callback_on_writable(client->wsi);
}

LIBWS_PUBLIC(size_t)
ws_receive(ws_client *client, void *buf, size_t size)
{
	const ws_msg *pmsg = lws_ring_get_element(client->receive_queue, 0);
	if (!pmsg)
	{
		return 0;
	}

	if (pmsg->size > size)
	{
		size = pmsg->size;
	}

	memcpy(buf, pmsg->data, size);
	lws_ring_consume(client->receive_queue, NULL, NULL, 1);
	return pmsg->size;
}

LIBWS_PUBLIC(void)
ws_delete(struct ws *ws)
{
	_LIBWS_FREE(ws);
}
