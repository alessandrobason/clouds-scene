#include "socket.h"

#if COLLA_WIN && COLLA_CMT_LIB
#pragma comment(lib, "Ws2_32")
#endif

#if COLLA_WIN
#include <winsock2.h>

typedef int socklen_t;

bool skInit(void) {
    WSADATA w;
    int error = WSAStartup(0x0202, &w);
    return error == 0;
}

bool skCleanup(void) {
    return WSACleanup() == 0;
}

bool skClose(socket_t sock) {
    return closesocket(sock) != SOCKET_ERROR;
}

int skPoll(skpoll_t *to_poll, int num_to_poll, int timeout) {
    return WSAPoll(to_poll, num_to_poll, timeout);
}

int skGetError(void) {
    return WSAGetLastError();
}

#else

#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h> // strerror
#include <poll.h>

bool skInit(void) {
    return true;
}

bool skCleanup(void) {
    return true;
}

bool skClose(socket_t sock) {
    return close(sock) != SOCKET_ERROR;
}

int skPoll(skpoll_t *to_poll, int num_to_poll, int timeout) {
    return poll(to_poll, num_to_poll, timeout);
}

int skGetError(void) {
    return errno;
}

const char *skGetErrorString(void) {
    return strerror(errno);
}

#endif

socket_t skOpen(sktype_e type) {
    int sock_type = 0;

    switch(type) {
        case SOCK_TCP: sock_type = SOCK_STREAM; break;
        case SOCK_UDP: sock_type = SOCK_DGRAM;  break;
        default: fatal("skType not recognized: %d", type); break;
    }

    return skOpenPro(AF_INET, sock_type, 0);
}

socket_t skOpenEx(const char *protocol) {
    struct protoent *proto = getprotobyname(protocol);
    if(!proto) {
        return (socket_t){0};
    }
    return skOpenPro(AF_INET, SOCK_STREAM, proto->p_proto);
}

socket_t skOpenPro(int af, int type, int protocol) {
    return socket(af, type, protocol);
}

bool skIsValid(socket_t sock) {
    return sock != SOCKET_ERROR;
}

skaddrin_t skAddrinInit(const char *ip, uint16_t port) {
    return (skaddrin_t){
        .sin_family = AF_INET,
        .sin_port = htons(port),
        // TODO use inet_pton instead
        .sin_addr.s_addr = inet_addr(ip),
    };
}

bool skBind(socket_t sock, const char *ip, uint16_t port) {
    skaddrin_t addr = skAddrinInit(ip, port);
    return skBindPro(sock, (skaddr_t *)&addr, sizeof(addr));
}

bool skBindPro(socket_t sock, const skaddr_t *name, int namelen) {
    return bind(sock, name, namelen) != SOCKET_ERROR;
}

bool skListen(socket_t sock) {
    return skListenPro(sock, 1);
}

bool skListenPro(socket_t sock, int backlog) {
    return listen(sock, backlog) != SOCKET_ERROR;
}

socket_t skAccept(socket_t sock) {
    skaddrin_t addr = {0};
    int addr_size = sizeof(addr);
    return skAcceptPro(sock, (skaddr_t *)&addr, &addr_size);
}

socket_t skAcceptPro(socket_t sock, skaddr_t *addr, int *addrlen) {
    return accept(sock, addr, (socklen_t *)addrlen);
}

bool skConnect(socket_t sock, const char *server, unsigned short server_port) {
    // TODO use getaddrinfo insetad
    struct hostent *host = gethostbyname(server);
    // if gethostbyname fails, inet_addr will also fail and return an easier to debug error
    const char *address = server;
    if(host) {
        address = inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);
    }
    
    skaddrin_t addr = skAddrinInit(address, server_port);

    return skConnectPro(sock, (skaddr_t *)&addr, sizeof(addr));
}

bool skConnectPro(socket_t sock, const skaddr_t *name, int namelen) {
    return connect(sock, name, namelen) != SOCKET_ERROR;
}

int skSend(socket_t sock, const void *buf, int len) {
    return skSendPro(sock, buf, len, 0);
}

int skSendPro(socket_t sock, const void *buf, int len, int flags) {
    return send(sock, buf, len, flags);
}

int skSendTo(socket_t sock, const void *buf, int len, const skaddrin_t *to) {
    return skSendToPro(sock, buf, len, 0, (skaddr_t*)to, sizeof(skaddrin_t));
}

int skSendToPro(socket_t sock, const void *buf, int len, int flags, const skaddr_t *to, int tolen) {
    return sendto(sock, buf, len, flags, to, tolen);
}

int skReceive(socket_t sock, void *buf, int len) {
    return skReceivePro(sock, buf, len, 0);
}

int skReceivePro(socket_t sock, void *buf, int len, int flags) {
    return recv(sock, buf, len, flags);
}

int skReceiveFrom(socket_t sock, void *buf, int len, skaddrin_t *from) {
    int fromlen = sizeof(skaddr_t);
    return skReceiveFromPro(sock, buf, len, 0, (skaddr_t*)from, &fromlen);
}

int skReceiveFromPro(socket_t sock, void *buf, int len, int flags, skaddr_t *from, int *fromlen) {
    return recvfrom(sock, buf, len, flags, from, (socklen_t *)fromlen);
}

// put this at the end of file to not make everything else unreadable
#if COLLA_WIN
const char *skGetErrorString(void) {
    switch(skGetError()) {
        case WSA_INVALID_HANDLE: return "Specified event object handle is invalid.";
        case WSA_NOT_ENOUGH_MEMORY: return "Insufficient memory available.";
        case WSA_INVALID_PARAMETER: return "One or more parameters are invalid.";
        case WSA_OPERATION_ABORTED: return "Overlapped operation aborted.";
        case WSA_IO_INCOMPLETE: return "Overlapped I/O event object not in signaled state.";
        case WSA_IO_PENDING: return "Overlapped operations will complete later.";
        case WSAEINTR: return "Interrupted function call.";
        case WSAEBADF: return "File handle is not valid.";
        case WSAEACCES: return "Permission denied.";
        case WSAEFAULT: return "Bad address.";
        case WSAEINVAL: return "Invalid argument.";
        case WSAEMFILE: return "Too many open files.";
        case WSAEWOULDBLOCK: return "Resource temporarily unavailable.";
        case WSAEINPROGRESS: return "Operation now in progress.";
        case WSAEALREADY: return "Operation already in progress.";
        case WSAENOTSOCK: return "Socket operation on nonsocket.";
        case WSAEDESTADDRREQ: return "Destination address required.";
        case WSAEMSGSIZE: return "Message too long.";
        case WSAEPROTOTYPE: return "Protocol wrong type for socket.";
        case WSAENOPROTOOPT: return "Bad protocol option.";
        case WSAEPROTONOSUPPORT: return "Protocol not supported.";
        case WSAESOCKTNOSUPPORT: return "Socket type not supported.";
        case WSAEOPNOTSUPP: return "Operation not supported.";
        case WSAEPFNOSUPPORT: return "Protocol family not supported.";
        case WSAEAFNOSUPPORT: return "Address family not supported by protocol family.";
        case WSAEADDRINUSE: return "Address already in use.";
        case WSAEADDRNOTAVAIL: return "Cannot assign requested address.";
        case WSAENETDOWN: return "Network is down.";
        case WSAENETUNREACH: return "Network is unreachable.";
        case WSAENETRESET: return "Network dropped connection on reset.";
        case WSAECONNABORTED: return "Software caused connection abort.";
        case WSAECONNRESET: return "Connection reset by peer.";
        case WSAENOBUFS: return "No buffer space available.";
        case WSAEISCONN: return "Socket is already connected.";
        case WSAENOTCONN: return "Socket is not connected.";
        case WSAESHUTDOWN: return "Cannot send after socket shutdown.";
        case WSAETOOMANYREFS: return "Too many references.";
        case WSAETIMEDOUT: return "Connection timed out.";
        case WSAECONNREFUSED: return "Connection refused.";
        case WSAELOOP: return "Cannot translate name.";
        case WSAENAMETOOLONG: return "Name too long.";
        case WSAEHOSTDOWN: return "Host is down.";
        case WSAEHOSTUNREACH: return "No route to host.";
        case WSAENOTEMPTY: return "Directory not empty.";
        case WSAEPROCLIM: return "Too many processes.";
        case WSAEUSERS: return "User quota exceeded.";
        case WSAEDQUOT: return "Disk quota exceeded.";
        case WSAESTALE: return "Stale file handle reference.";
        case WSAEREMOTE: return "Item is remote.";
        case WSASYSNOTREADY: return "Network subsystem is unavailable.";
        case WSAVERNOTSUPPORTED: return "Winsock.dll version out of range.";
        case WSANOTINITIALISED: return "Successful WSAStartup not yet performed.";
        case WSAEDISCON: return "Graceful shutdown in progress.";
        case WSAENOMORE: return "No more results.";
        case WSAECANCELLED: return "Call has been canceled.";
        case WSAEINVALIDPROCTABLE: return "Procedure call table is invalid.";
        case WSAEINVALIDPROVIDER: return "Service provider is invalid.";
        case WSAEPROVIDERFAILEDINIT: return "Service provider failed to initialize.";
        case WSASYSCALLFAILURE: return "System call failure.";
        case WSASERVICE_NOT_FOUND: return "Service not found.";
        case WSATYPE_NOT_FOUND: return "Class type not found.";
        case WSA_E_NO_MORE: return "No more results.";
        case WSA_E_CANCELLED: return "Call was canceled.";
        case WSAEREFUSED: return "Database query was refused.";
        case WSAHOST_NOT_FOUND: return "Host not found.";
        case WSATRY_AGAIN: return "Nonauthoritative host not found.";
        case WSANO_RECOVERY: return "This is a nonrecoverable error.";
        case WSANO_DATA: return "Valid name, no data record of requested type.";
        case WSA_QOS_RECEIVERS: return "QoS receivers.";
        case WSA_QOS_SENDERS: return "QoS senders.";
        case WSA_QOS_NO_SENDERS: return "No QoS senders.";
        case WSA_QOS_NO_RECEIVERS: return "QoS no receivers.";
        case WSA_QOS_REQUEST_CONFIRMED: return "QoS request confirmed.";
        case WSA_QOS_ADMISSION_FAILURE: return "QoS admission error.";
        case WSA_QOS_POLICY_FAILURE: return "QoS policy failure.";
        case WSA_QOS_BAD_STYLE: return "QoS bad style.";
        case WSA_QOS_BAD_OBJECT: return "QoS bad object.";
        case WSA_QOS_TRAFFIC_CTRL_ERROR: return "QoS traffic control error.";
        case WSA_QOS_GENERIC_ERROR: return "QoS generic error.";
        case WSA_QOS_ESERVICETYPE: return "QoS service type error.";
        case WSA_QOS_EFLOWSPEC: return "QoS flowspec error.";
        case WSA_QOS_EPROVSPECBUF: return "Invalid QoS provider buffer.";
        case WSA_QOS_EFILTERSTYLE: return "Invalid QoS filter style.";
        case WSA_QOS_EFILTERTYPE: return "Invalid QoS filter type.";
        case WSA_QOS_EFILTERCOUNT: return "Incorrect QoS filter count.";
        case WSA_QOS_EOBJLENGTH: return "Invalid QoS object length.";
        case WSA_QOS_EFLOWCOUNT: return "Incorrect QoS flow count.";
        case WSA_QOS_EUNKOWNPSOBJ: return "Unrecognized QoS object.";
        case WSA_QOS_EPOLICYOBJ: return "Invalid QoS policy object.";
        case WSA_QOS_EFLOWDESC: return "Invalid QoS flow descriptor.";
        case WSA_QOS_EPSFLOWSPEC: return "Invalid QoS provider-specific flowspec.";
        case WSA_QOS_EPSFILTERSPEC: return "Invalid QoS provider-specific filterspec.";
        case WSA_QOS_ESDMODEOBJ: return "Invalid QoS shape discard mode object.";
        case WSA_QOS_ESHAPERATEOBJ: return "Invalid QoS shaping rate object.";
        case WSA_QOS_RESERVED_PETYPE: return "Reserved policy QoS element type.";
    }

    return "(nothing)";
}
#endif