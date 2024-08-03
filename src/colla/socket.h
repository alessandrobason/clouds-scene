#pragma once

#include "collatypes.h"

#if COLLA_WIN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#else
#include <sys/socket.h> 
#include <netinet/in.h> 
#endif

typedef uintptr_t socket_t;
typedef struct sockaddr skaddr_t;
typedef struct sockaddr_in skaddrin_t;
typedef struct pollfd skpoll_t;

#define SOCKET_ERROR (-1)

typedef enum {
    SOCK_TCP,
    SOCK_UDP,
} sktype_e;

// Initialize sockets, returns true on success
bool skInit(void);
// Terminates sockets, returns true on success
bool skCleanup(void);

// Opens a socket, check socket_t with skValid
socket_t skOpen(sktype_e type);
// Opens a socket using 'protocol', options are 
// ip, icmp, ggp, tcp, egp, pup, udp, hmp, xns-idp, rdp
// check socket_t with skValid
socket_t skOpenEx(const char *protocol);
// Opens a socket, check socket_t with skValid
socket_t skOpenPro(int af, int type, int protocol);

// Checks that a opened socket is valid, returns true on success
bool skIsValid(socket_t sock);

// Closes a socket, returns true on success
bool skClose(socket_t sock);

// Fill out a sk_addrin_t structure with "ip" and "port"
skaddrin_t skAddrinInit(const char *ip, uint16_t port);

// Associate a local address with a socket
bool skBind(socket_t sock, const char *ip, uint16_t port);
// Associate a local address with a socket
bool skBindPro(socket_t sock, const skaddr_t *name, int namelen);

// Place a socket in a state in which it is listening for an incoming connection
bool skListen(socket_t sock);
// Place a socket in a state in which it is listening for an incoming connection
bool skListenPro(socket_t sock, int backlog);

// Permits an incoming connection attempt on a socket
socket_t skAccept(socket_t sock);
// Permits an incoming connection attempt on a socket
socket_t skAcceptPro(socket_t sock, skaddr_t *addr, int *addrlen);

// Connects to a server (e.g. "127.0.0.1" or "google.com") with a port(e.g. 1234), returns true on success
bool skConnect(socket_t sock, const char *server, unsigned short server_port);
// Connects to a server, returns true on success
bool skConnectPro(socket_t sock, const skaddr_t *name, int namelen);

// Sends data on a socket, returns true on success
int skSend(socket_t sock, const void *buf, int len);
// Sends data on a socket, returns true on success
int skSendPro(socket_t sock, const void *buf, int len, int flags);
// Sends data to a specific destination
int skSendTo(socket_t sock, const void *buf, int len, const skaddrin_t *to);
// Sends data to a specific destination
int skSendToPro(socket_t sock, const void *buf, int len, int flags, const skaddr_t *to, int tolen);
// Receives data from a socket, returns byte count on success, 0 on connection close or -1 on error
int skReceive(socket_t sock, void *buf, int len);
// Receives data from a socket, returns byte count on success, 0 on connection close or -1 on error
int skReceivePro(socket_t sock, void *buf, int len, int flags);
// Receives a datagram and stores the source address. 
int skReceiveFrom(socket_t sock, void *buf, int len, skaddrin_t *from);
// Receives a datagram and stores the source address. 
int skReceiveFromPro(socket_t sock, void *buf, int len, int flags, skaddr_t *from, int *fromlen);

// Wait for an event on some sockets
int skPoll(skpoll_t *to_poll, int num_to_poll, int timeout);

// Returns latest socket error, returns 0 if there is no error
int skGetError(void);
// Returns a human-readable string from a skGetError
const char *skGetErrorString(void);
