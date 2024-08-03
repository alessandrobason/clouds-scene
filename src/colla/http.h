#pragma once

#include "collatypes.h"
#include "str.h"

typedef struct arena_t arena_t;
typedef uintptr_t socket_t;

typedef enum {
    HTTP_GET,   
    HTTP_POST,    
    HTTP_HEAD,    
    HTTP_PUT,    
    HTTP_DELETE 
} http_method_e;

const char *httpGetStatusString(int status);

typedef struct {
    uint8 major;
    uint8 minor;
} http_version_t;

// translates a http_version_t to a single readable number (e.g. 1.1 -> 11, 1.0 -> 10, etc)
int httpVerNumber(http_version_t ver);

typedef struct http_header_t {
    strview_t key;
    strview_t value;
    struct http_header_t *next;
} http_header_t;

typedef struct {
    http_method_e method;
    http_version_t version;
    http_header_t *headers;
    strview_t url;
    strview_t body;
} http_req_t;

typedef struct {
    int status_code;
    http_version_t version;
    http_header_t *headers;
    strview_t body;
} http_res_t;

// strview_t request needs to be valid for http_req_t to be valid!
http_req_t httpParseReq(arena_t *arena, strview_t request);
http_res_t httpParseRes(arena_t *arena, strview_t response);

str_t httpReqToStr(arena_t *arena, http_req_t *req);
str_t httpResToStr(arena_t *arena, http_res_t *res);

bool httpHasHeader(http_header_t *headers, strview_t key);
void httpSetHeader(http_header_t *headers, strview_t key, strview_t value);
strview_t httpGetHeader(http_header_t *headers, strview_t key);

str_t httpMakeUrlSafe(arena_t *arena, strview_t string);
str_t httpDecodeUrlSafe(arena_t *arena, strview_t string);

typedef struct {
    strview_t host;
    strview_t uri;
} http_url_t;

http_url_t httpSplitUrl(strview_t url);

typedef struct {
    arena_t *arena;
    strview_t url;
    http_method_e request_type;
    http_header_t *headers; 
    int header_count;
    strview_t body;
} http_request_desc_t;

// arena_t *arena, strview_t url, [ http_header_t *headers, int header_count, strview_t body ]
#define httpGet(arena, url, ...) httpRequest(&(http_request_desc_t){ arena, url, .request_type = HTTP_GET, __VA_ARGS__ })
#define httpsGet(arena, url, ...) httpsRequest(&(http_request_desc_t){ arena, url, .request_type = HTTP_GET, __VA_ARGS__ })

http_res_t httpRequest(http_request_desc_t *request);
buffer_t httpsRequest(http_request_desc_t *request);
