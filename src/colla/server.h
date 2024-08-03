#pragma once

#include "collatypes.h"
#include "str.h"
#include "http.h"

typedef struct arena_t arena_t;
typedef struct server_t server_t;

typedef struct server_field_t {
    str_t key;
    str_t value;
    struct server_field_t *next;
} server_field_t;

typedef struct {
    http_method_e method;
    str_t page;
    server_field_t *page_fields;
    uint8 version_minor;
    uint8 version_major;
    server_field_t *fields;
    server_field_t *fields_tail;
    // buffer_t body;
} server_req_t;

typedef str_t (*server_route_f)(arena_t scratch, server_t *server, server_req_t *req, void *userdata);


server_t *serverSetup(arena_t *arena, uint16 port);
void serverRoute(arena_t *arena, server_t *server, strview_t page, server_route_f cb, void *userdata);
void serverRouteDefault(arena_t *arena, server_t *server, server_route_f cb, void *userdata);
void serverStart(arena_t scratch, server_t *server);
void serverStop(server_t *server);

str_t serverMakeResponse(arena_t *arena, int status_code, strview_t content_type, strview_t body);
socket_t serverGetClient(server_t *server);
void serverSetClient(server_t *server, socket_t client);