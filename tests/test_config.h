#ifndef LIBWS_TESTUTILS__h
#define LIBWS_TESTUTILS__h

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include "libwebsockets.h"
#include "ws.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct lws_context_creation_info lws_context_creation_info;
    typedef struct lws_context lws_context;

    typedef enum ws_event ws_event;
    typedef struct ws_connect_options ws_connect_options;
    typedef struct ws_listen_options ws_listen_options;
    typedef struct ws_client ws_client;
    typedef struct ws ws;

#ifdef __cplusplus
}
#endif

#endif
