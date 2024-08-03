#include "server.h"

#include <stdio.h>

#include "socket.h"
#include "tracelog.h"
#include "strstream.h"
#include "arena.h"

#define SERVER_BUFSZ 4096

typedef enum {
    PARSE_REQ_BEGIN,
    PARSE_REQ_PAGE,
    PARSE_REQ_VERSION,
    PARSE_REQ_FIELDS,
    PARSE_REQ_FINISHED,
    PARSE_REQ_FAILED,
} server__req_status_e;

typedef struct {
    server_req_t req;
    server__req_status_e status;
    char fullbuf[SERVER_BUFSZ * 2];
    usize prevbuf_len;
} server__req_ctx_t;


typedef struct server__route_t {
    str_t page;
    server_route_f fn;
    void *userdata;
    struct server__route_t *next;
} server__route_t;

typedef struct server_t {
    socket_t socket;
    server__route_t *routes;
    server__route_t *routes_default;
    socket_t current_client;
    bool should_stop;
} server_t;

bool server__parse_chunk(arena_t *arena, server__req_ctx_t *ctx, char buffer[SERVER_BUFSZ], usize buflen) {
    memcpy(ctx->fullbuf + ctx->prevbuf_len, buffer, buflen);
    usize fulllen = ctx->prevbuf_len + buflen;

    instream_t in = istrInitLen(ctx->fullbuf, fulllen);

#define RESET_STREAM() in.cur = in.start + begin
#define BEGIN_STREAM() begin = istrTell(in)

    usize begin = istrTell(in);

    switch (ctx->status) {
        case PARSE_REQ_BEGIN:
        {
            BEGIN_STREAM();

            strview_t method = istrGetView(&in, ' ');
            if (istrGet(&in) != ' ') {
                RESET_STREAM();
                break;
            }

            if (strvEquals(method, strv("GET"))) {
                ctx->req.method = HTTP_GET;
            }
            else if(strvEquals(method, strv("POST"))) {
                ctx->req.method = HTTP_POST;
            }
            else {
                err("unknown method: (%.*s)", method.len, method.buf);
                ctx->status = PARSE_REQ_FAILED;
                break;
            }

            ctx->status = PARSE_REQ_PAGE;
        }
        // fallthrough
        case PARSE_REQ_PAGE:
        {
            BEGIN_STREAM();
            
            strview_t page = istrGetView(&in, ' ');
            if (istrGet(&in) != ' ') {
                RESET_STREAM();
                break;
            }

            ctx->req.page = str(arena, page);

            ctx->status = PARSE_REQ_VERSION;
        }
        // fallthrough
        case PARSE_REQ_VERSION:
        {
            BEGIN_STREAM();
            
            strview_t version = istrGetView(&in, '\n');
            if (istrGet(&in) != '\n') {
                RESET_STREAM();
                break;
            }

            if (version.len < 8) {
                err("version too short: (%.*s)", version.len, version.buf);
                ctx->status = PARSE_REQ_FAILED;
                break;
            }

            if (!strvEquals(strvSub(version, 0, 4), strv("HTTP"))) {
                err("version does not start with HTTP: (%.4s)", version.buf);
                ctx->status = PARSE_REQ_FAILED;
                break;
            }

            // skip HTTP
            version = strvRemovePrefix(version, 4);

            uint8 major, minor;
            int scanned = sscanf(version.buf, "/%hhu.%hhu", &major, &minor);

            if (scanned != 2) {
                err("could not scan both major and minor from version: (%.*s)", version.len, version.buf);
                ctx->status = PARSE_REQ_FAILED;
                break;
            }

            ctx->req.version_major = major;
            ctx->req.version_minor = minor;

            ctx->status = PARSE_REQ_FIELDS;
        }
        // fallthrough
        case PARSE_REQ_FIELDS:
        {
            bool finished_parsing = false;

            while (true) {
                BEGIN_STREAM();

                strview_t field = istrGetView(&in, '\n');
                if (istrGet(&in) != '\n') {
                    RESET_STREAM();
                    break;
                }

                instream_t field_in = istrInitLen(field.buf, field.len);

                strview_t key = istrGetView(&field_in, ':');
                if (istrGet(&field_in) != ':') {
                    err("field does not have ':' (%.*s)", field.len, field.buf);
                    ctx->status = PARSE_REQ_FAILED;
                    break;
                }

                istrSkipWhitespace(&field_in);

                strview_t value = istrGetView(&field_in, '\r');
                if (istrGet(&field_in) != '\r') {
                    warn("field does not finish with \\r: (%.*s)", field.len, field.buf);
                    RESET_STREAM();
                    break;
                }

                server_field_t *new_field = alloc(arena, server_field_t);
                new_field->key = str(arena, key);
                new_field->value = str(arena, value);

                if (!ctx->req.fields) {
                    ctx->req.fields = new_field;
                }
                
                if (ctx->req.fields_tail) {
                    ctx->req.fields_tail->next = new_field;
                }

                ctx->req.fields_tail = new_field;

                // check if we finished parsing the fields
                if (istrGet(&in) == '\r') {
                    if (istrGet(&in) == '\n') {
                        finished_parsing = true;
                        break;
                    }
                    else {
                        istrRewindN(&in, 2);
                        warn("should have finished parsing field, but apparently not?: (%.*s)", in.cur, istrRemaining(in));
                    }
                }
                else {
                    istrRewindN(&in, 1);
                }
            }

            if (!finished_parsing) {
                break;
            }

            ctx->status = PARSE_REQ_FINISHED;
            break;
        }
        default: break;
    }

#undef RESET_STREAM

    ctx->prevbuf_len = istrRemaining(in);

    memmove(ctx->fullbuf, ctx->fullbuf + istrTell(in), ctx->prevbuf_len);

    return ctx->status >= PARSE_REQ_FINISHED;
}

void server__parse_req_url(arena_t *arena, server_req_t *req) {
    instream_t in = istrInitLen(req->page.buf, req->page.len);
    istrIgnore(&in, '?');
    // no fields in url
    if (istrGet(&in) != '?') {
        return;
    }

    req->page.len = istrTell(in) - 1;
    req->page.buf[req->page.len] = '\0';

    while (!istrIsFinished(in)) {
        strview_t field = istrGetView(&in, '&');
        istrSkip(&in, 1); // skip &
        usize pos = strvFind(field, '=', 0);
        if (pos == SIZE_MAX) {
            fatal("url parameter does not include =: %.*s", field.buf, field.len);
        }
        strview_t key = strvSub(field, 0, pos);
        strview_t val = strvSub(field, pos + 1, SIZE_MAX);

        server_field_t *f = alloc(arena, server_field_t);
        f->key = str(arena, key);
        f->value = str(arena, val);
        f->next = req->page_fields;
        req->page_fields = f;
    }
}

server_t *serverSetup(arena_t *arena, uint16 port) {
    if (!skInit()) {
        fatal("couldn't initialise sockets: %s", skGetErrorString());
    }

    socket_t sk = skOpen(SOCK_TCP);
    if (!skIsValid(sk)) {
        fatal("couldn't open socket: %s", skGetErrorString());
    }

    skaddrin_t addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port),
    };

    if (!skBindPro(sk, (skaddr_t *)&addr, sizeof(addr))) {
        fatal("could not bind socket: %s", skGetErrorString());
    }

    if (!skListenPro(sk, 10)) {
        fatal("could not listen on socket: %s", skGetErrorString());
    }

    server_t *server = alloc(arena, server_t);

    server->socket = sk;
    
    return server;
}

void serverRoute(arena_t *arena, server_t *server, strview_t page, server_route_f cb, void *userdata) {
    // check if a route for that page already exists
    server__route_t *r = server->routes;

    while (r) {
        if (strvEquals(strv(r->page), page)) {
            r->fn = cb;
            r->userdata = userdata;
            break;
        }
        r = r->next;
    }
    
    // no route found, make a new one
    if (!r) {
        r = alloc(arena, server__route_t);
        r->next = server->routes;
        server->routes = r;

        r->page = str(arena, page);
    }

    r->fn = cb;
    r->userdata = userdata;
}

void serverRouteDefault(arena_t *arena, server_t *server, server_route_f cb, void *userdata) {
    server__route_t *r = server->routes_default;

    if (!r) {
        r = alloc(arena, server__route_t);
        server->routes_default = r;
    }

    r->fn = cb;
    r->userdata = userdata;
}

void serverStart(arena_t scratch, server_t *server) {
    usize start = arenaTell(&scratch);

    while (!server->should_stop) {
        socket_t client = skAccept(server->socket);
        if (!skIsValid(client)) {
            err("accept failed");
            continue;
        }

        arenaRewind(&scratch, start);

        server__req_ctx_t req_ctx = {0};

        char buffer[SERVER_BUFSZ];
        int read = 0;
        do {
            read = skReceive(client, buffer, sizeof(buffer));
            if(read == SOCKET_ERROR) {
                fatal("couldn't get the data from the server: %s", skGetErrorString());
            }
            if (server__parse_chunk(&scratch, &req_ctx, buffer, read)) {
                break;
            }
        } while(read != 0);

        if (req_ctx.status == PARSE_REQ_FAILED || req_ctx.status == PARSE_REQ_BEGIN) {
            err("failed to parse request!");
            goto end_connection;
        }

        server_req_t req = req_ctx.req;

        server__parse_req_url(&scratch, &req);

        server__route_t *route = server->routes;
        while (route) {
            if (strEquals(route->page, req.page)) {
                break;
            }
            route = route->next;
        }

        if (!route) {
            route = server->routes_default;
        }

        server->current_client = client;
        str_t response = route->fn(scratch, server, &req, route->userdata);

        if (!skIsValid(server->current_client)) {
            continue;
        }

        skSend(client, response.buf, (int)response.len);

end_connection:
        skClose(client);
    }

    if (!skClose(server->socket)) {
        fatal("couldn't close socket: %s", skGetErrorString());
    }
}

void serverStop(server_t *server) {
    server->should_stop = true;
}

str_t serverMakeResponse(arena_t *arena, int status_code, strview_t content_type, strview_t body) {
    return strFmt(
        arena,
        
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %v\r\n"
        "\r\n"
        "%v",

        status_code, httpGetStatusString(status_code),
        content_type,
        body
    );
}

socket_t serverGetClient(server_t *server) {
    return server->current_client;
}

void serverSetClient(server_t *server, socket_t client) {
    server->current_client = client;
}


#undef SERVER_BUFSZ