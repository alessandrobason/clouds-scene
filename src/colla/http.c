#include "http.h"

#include "warnings/colla_warn_beg.h"

#include <assert.h>
#include <stdio.h>

#include "arena.h"
#include "str.h"
#include "strstream.h"
#include "format.h"
#include "socket.h"
#include "tracelog.h"

#if COLLA_WIN 
    #if COLLA_CMT_LIB
    #pragma comment(lib, "Wininet")
    #endif

    #include <windows.h>
    #include <wininet.h>
#endif

static const TCHAR *https__get_method_str(http_method_e method);

static http_header_t *http__parse_headers(arena_t *arena, instream_t *in) {
    http_header_t *head = NULL;
    strview_t line = (strview_t){0};

    do {
        line = istrGetView(in, '\r');

        usize pos = strvFind(line, ':', 0);
        if (pos != STR_NONE) {
            http_header_t *h = alloc(arena, http_header_t);

            h->key   = strvSub(line, 0, pos);
            h->value = strvSub(line, pos + 2, SIZE_MAX);
            
            h->next = head;
            head = h;
        }

        istrSkip(in, 2); // skip \r\n
    } while (line.len > 2); // while line != "\r\n"

    return head;
}

const char *httpGetStatusString(int status) {
    switch (status) {
        case 200: return "OK";              
        case 201: return "CREATED";         
        case 202: return "ACCEPTED";        
        case 204: return "NO CONTENT";      
        case 205: return "RESET CONTENT";   
        case 206: return "PARTIAL CONTENT"; 

        case 300: return "MULTIPLE CHOICES";    
        case 301: return "MOVED PERMANENTLY";   
        case 302: return "MOVED TEMPORARILY";   
        case 304: return "NOT MODIFIED";        

        case 400: return "BAD REQUEST";             
        case 401: return "UNAUTHORIZED";            
        case 403: return "FORBIDDEN";               
        case 404: return "NOT FOUND";               
        case 407: return "RANGE NOT SATISFIABLE";   

        case 500: return "INTERNAL SERVER_ERROR";   
        case 501: return "NOT IMPLEMENTED";         
        case 502: return "BAD GATEWAY";             
        case 503: return "SERVICE NOT AVAILABLE";   
        case 504: return "GATEWAY TIMEOUT";         
        case 505: return "VERSION NOT SUPPORTED";   
    }
    
    return "UNKNOWN";
}

int httpVerNumber(http_version_t ver) {
    return (ver.major * 10) + ver.minor;
}

http_req_t httpParseReq(arena_t *arena, strview_t request) {
    http_req_t req = {0};
    instream_t in = istrInitLen(request.buf, request.len);

    strview_t method = strvTrim(istrGetView(&in, '/'));
    istrSkip(&in, 1); // skip /
    req.url          = strvTrim(istrGetView(&in, ' '));
    strview_t http   = strvTrim(istrGetView(&in, '\n'));

    istrSkip(&in, 1); // skip \n

    req.headers = http__parse_headers(arena, &in);

    req.body = strvTrim(istrGetViewLen(&in, SIZE_MAX));

    strview_t methods[] = { strv("GET"), strv("POST"), strv("HEAD"), strv("PUT"), strv("DELETE") };
    usize methods_count = arrlen(methods);

    for (usize i = 0; i < methods_count; ++i) {
        if (strvEquals(method, methods[i])) {
            req.method = (http_method_e)i;
            break;
        }
    }

    in = istrInitLen(http.buf, http.len);
    istrIgnoreAndSkip(&in, '/'); // skip HTTP/
    istrGetU8(&in, &req.version.major);
    istrSkip(&in, 1); // skip .
    istrGetU8(&in, &req.version.minor);

    return req;
}

http_res_t httpParseRes(arena_t *arena, strview_t response) {
    http_res_t res = {0};
    instream_t in = istrInitLen(response.buf, response.len);

    strview_t http = istrGetViewLen(&in, 5);
    if (!strvEquals(http, strv("HTTP"))) {
        err("response doesn't start with 'HTTP', instead with %v", http);
        return (http_res_t){0};
    }
    istrSkip(&in, 1); // skip /
    istrGetU8(&in, &res.version.major);
    istrSkip(&in, 1); // skip .
    istrGetU8(&in, &res.version.minor);
    istrGetI32(&in, (int32*)&res.status_code);

    istrIgnore(&in, '\n');
    istrSkip(&in, 1); // skip \n

    res.headers = http__parse_headers(arena, &in);

    strview_t encoding = httpGetHeader(res.headers, strv("transfer-encoding"));
    if (!strvEquals(encoding, strv("chunked"))) {
        res.body = istrGetViewLen(&in, SIZE_MAX);
    }
    else {
        err("chunked encoding not implemented yet! body ignored");
    }

    return res;
}

str_t httpReqToStr(arena_t *arena, http_req_t *req) {
    outstream_t out = ostrInit(arena);

    const char *method = NULL;
    switch (req->method) {
        case HTTP_GET:    method = "GET";       break;
        case HTTP_POST:   method = "POST";      break;
        case HTTP_HEAD:   method = "HEAD";      break;
        case HTTP_PUT:    method = "PUT";       break;
        case HTTP_DELETE: method = "DELETE";    break;
        default: err("unrecognised method: %d", method); return (str_t){0};
    }

    ostrPrintf(
        &out, 
        "%s /%v HTTP/%hhu.%hhu\r\n",
        method, req->url, req->version.major, req->version.minor
    );

    http_header_t *h = req->headers;
    while (h) {
        ostrPrintf(&out, "%v: %v\r\n", h->key, h->value);
        h = h->next;
    }

    ostrPuts(&out, strv("\r\n"));
    ostrPuts(&out, req->body);

    return ostrAsStr(&out);
}

str_t httpResToStr(arena_t *arena, http_res_t *res) {
    outstream_t out = ostrInit(arena);

    ostrPrintf(
        &out,
        "HTTP/%hhu.%hhu %d %s\r\n",
        res->version.major, 
        res->version.minor,
        res->status_code, 
        httpGetStatusString(res->status_code)
    );
    ostrPuts(&out, strv("\r\n"));
    ostrPuts(&out, res->body);

    return ostrAsStr(&out);
}

bool httpHasHeader(http_header_t *headers, strview_t key) {
    http_header_t *h = headers;
    while (h) {
        if (strvEquals(h->key, key)) {
            return true;
        }
        h = h->next;
    }
    return false;
}

void httpSetHeader(http_header_t *headers, strview_t key, strview_t value) {
    http_header_t *h = headers;
    while (h) {
        if (strvEquals(h->key, key)) {
            h->value = value;
            break;
        }
        h = h->next;
    }
}

strview_t httpGetHeader(http_header_t *headers, strview_t key) {
    http_header_t *h = headers;
    while (h) {
        if (strvEquals(h->key, key)) {
            return h->value;
        }
        h = h->next;
    }
    return (strview_t){0};
}

str_t httpMakeUrlSafe(arena_t *arena, strview_t string) {
    strview_t chars = strv(" !\"#$%%&'()*+,/:;=?@[]");
    usize final_len = string.len;

    // find final string length first
    for (usize i = 0; i < string.len; ++i) {
        if (strvContains(chars, string.buf[i])) {
            final_len += 2;
        }
    }
    
    str_t out = {
        .buf = alloc(arena, char, final_len + 1),
        .len = final_len
    };
    usize cur = 0;
    // substitute characters
    for (usize i = 0; i < string.len; ++i) {
        if (strvContains(chars, string.buf[i])) {
            fmtBuffer(out.buf + cur, 4, "%%%X", string.buf[i]);
            cur += 3;
        }
        else {
            out.buf[cur++] = string.buf[i];
        }
    }

    return out;
}

str_t httpDecodeUrlSafe(arena_t *arena, strview_t string) {
    usize final_len = string.len;

    for (usize i = 0; i < string.len; ++i) {
        if (string.buf[i] == '%') {
            final_len -= 2;
            i += 2;
        }
    }

    assert(final_len <= string.len);

    str_t out = {
        .buf = alloc(arena, char, final_len + 1),
        .len = final_len
    };

    usize k = 0;

    for (usize i = 0; i < string.len; ++i) {
        if (string.buf[i] == '%') {
            // skip %
            ++i;

            unsigned int ch = 0;
            int result = sscanf(string.buf + i, "%02X", &ch);
            if (result != 1 || ch > UINT8_MAX) {
                err("malformed url at %zu (%s)", i, string.buf + i);
                return (str_t){0};
            }
            out.buf[k++] = (char)ch;
            
            // skip first char of hex
            ++i;
        }
        else {
            out.buf[k++] = string.buf[i];
        }
    }

    return out;
}

http_url_t httpSplitUrl(strview_t url) {
    http_url_t out = {0};

    if (strvStartsWithView(url, strv("https://"))) {
        url = strvRemovePrefix(url, 8);
    }
    else if (strvStartsWithView(url, strv("http://"))) {
        url = strvRemovePrefix(url, 7);
    }

    out.host = strvSub(url, 0, strvFind(url, '/', 0));
    out.uri = strvSub(url, out.host.len, SIZE_MAX);

    return out;
}

http_res_t httpRequest(http_request_desc_t *request) {
    usize arena_begin = arenaTell(request->arena);

    http_req_t req = {
        .version = (http_version_t){ 1, 1 },
        .url = request->url,
        .body = request->body,
        .method = request->request_type,
    };

    http_header_t *h = NULL;

    for (int i = 0; i < request->header_count; ++i) {
        http_header_t *header = request->headers + i;
        header->next = h;
        h = header;
    }

    req.headers = h;
    
    http_url_t url = httpSplitUrl(req.url);
    
    if (strvEndsWith(url.host, '/')) {
        url.host = strvRemoveSuffix(url.host, 1);
    }

    if (!httpHasHeader(req.headers, strv("Host"))) {
        httpSetHeader(req.headers, strv("Host"), url.host);
    }
    if (!httpHasHeader(req.headers, strv("Content-Length"))) {
        char tmp[16] = {0};
        fmtBuffer(tmp, arrlen(tmp), "%zu", req.body.len);
        httpSetHeader(req.headers, strv("Content-Length"), strv(tmp));
    }
    if (req.method == HTTP_POST && !httpHasHeader(req.headers, strv("Content-Type"))) {
        httpSetHeader(req.headers, strv("Content-Type"), strv("application/x-www-form-urlencoded"));
    }
    if (!httpHasHeader(req.headers, strv("Connection"))) {
        httpSetHeader(req.headers, strv("Connection"), strv("close"));
    }

    if (!skInit()) {
        err("couldn't initialise sockets: %s", skGetErrorString());
        goto error;
    }

    socket_t sock = skOpen(SOCK_TCP);
    if (!skIsValid(sock)) {
        err("couldn't open socket: %s", skGetErrorString());
        goto error;
    }

    char hostname[64] = {0};
    assert(url.host.len < arrlen(hostname));
    memcpy(hostname, url.host.buf, url.host.len);

    const uint16 DEFAULT_HTTP_PORT = 80;
    if (!skConnect(sock, hostname, DEFAULT_HTTP_PORT)) {
        err("Couldn't connect to host %s: %s", hostname, skGetErrorString());
        goto error;
    }

    str_t reqstr = httpReqToStr(request->arena, &req);
    if (strIsEmpty(reqstr)) {
        err("couldn't get string from request");
        goto error;
    }

    if (skSend(sock, reqstr.buf, (int)reqstr.len) == SOCKET_ERROR) {
        err("couldn't send request to socket: %s", skGetErrorString());
        goto error;
    }

    outstream_t response = ostrInit(request->arena);
    char buffer[4096];
    int read = 0;
    do {
        read = skReceive(sock, buffer, arrlen(buffer));
        if (read == SOCKET_ERROR) {
            err("couldn't get the data from the server: %s", skGetErrorString());
            goto error;
        }
        ostrPuts(&response, strv(buffer, read));
    } while (read != 0);

    if (!skClose(sock)) {
        err("couldn't close socket: %s", skGetErrorString());
    }

    if (!skCleanup()) {
        err("couldn't clean up sockets: %s", skGetErrorString());
    }

    return httpParseRes(request->arena, ostrAsView(&response));

error:
    arenaRewind(request->arena, arena_begin);
    skCleanup();
    return (http_res_t){0};
}

#if COLLA_WIN

buffer_t httpsRequest(http_request_desc_t *req) {
    HINTERNET internet = InternetOpen(
        TEXT("COLLA"), 
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0
    );
    if (!internet) {
        fatal("call to InternetOpen failed: %u", GetLastError());
    }

    http_url_t split = httpSplitUrl(req->url);
    strview_t server = split.host;
    strview_t page = split.uri;

    if (strvStartsWithView(server, strv("http://"))) {
        server = strvRemovePrefix(server, 7);
    }

    if (strvStartsWithView(server, strv("https://"))) {
        server = strvRemovePrefix(server, 8);
    }

    arena_t scratch = *req->arena;
    const TCHAR *tserver = strvToTChar(&scratch, server);
    const TCHAR *tpage = strvToTChar(&scratch, page);

    HINTERNET connection = InternetConnect(
        internet,
        tserver,
        INTERNET_DEFAULT_HTTPS_PORT,
        NULL,
        NULL,
        INTERNET_SERVICE_HTTP,
        0,
        (DWORD_PTR)NULL // userdata
    );
    if (!connection) {
        fatal("call to InternetConnect failed: %u", GetLastError());
    }

    const TCHAR *accepted_types[] = { TEXT("*/*"), NULL };

    HINTERNET request = HttpOpenRequest(
        connection,
        https__get_method_str(req->request_type),
        tpage,
        TEXT("HTTP/1.1"),
        NULL,
        accepted_types,
        INTERNET_FLAG_SECURE,
        (DWORD_PTR)NULL // userdata
    );
    if (!request) {
        fatal("call to HttpOpenRequest failed: %u", GetLastError());
    }

    outstream_t header = ostrInit(&scratch);

    for (int i = 0; i < req->header_count; ++i) {
        http_header_t *h = &req->headers[i];
        ostrClear(&header);
        ostrPrintf(
            &header,
            "%.*s: %.*s\r\n", 
            h->key.len, h->key.buf, 
            h->value.len, h->value.buf
        );
        str_t header_str = ostrAsStr(&header);
        HttpAddRequestHeadersA(
            request,
            header_str.buf,
            (DWORD)header_str.len,
            0
        );
    }

    BOOL request_sent = HttpSendRequest(
        request,
        NULL,
        0,
        (void *)req->body.buf,
        (DWORD)req->body.len
    );
    if (!request_sent) {
        fatal("call to HttpSendRequest failed: %u", GetLastError());
    }

    outstream_t out = ostrInit(req->arena);

    while (true) {
        DWORD bytes_read = 0;
        char buffer[4096];
        BOOL read = InternetReadFile(
            request,
            buffer,
            sizeof(buffer),
            &bytes_read
        );
        if (!read || bytes_read == 0) {
            break;
        }
        ostrPuts(&out, strv(buffer, bytes_read));
    }

    InternetCloseHandle(request);
    InternetCloseHandle(connection);
    InternetCloseHandle(internet);

    str_t outstr = ostrAsStr(&out);

    return (buffer_t) {
        .data = (uint8 *)outstr.buf,
        .len = outstr.len
    };
}

static const TCHAR *https__get_method_str(http_method_e method) {
    switch (method) {
        case HTTP_GET:    return TEXT("GET");
        case HTTP_POST:   return TEXT("POST");
        case HTTP_HEAD:   return TEXT("HEAD");
        case HTTP_PUT:    return TEXT("PUT");
        case HTTP_DELETE: return TEXT("DELETE");
    }
    // default GET
    return NULL;
}
#endif

#include "warnings/colla_warn_end.h"
