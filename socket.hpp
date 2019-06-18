#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "ny2.hpp"
#include <stddef.h>
#include <arpa/inet.h>
#include <sys/poll.h>

// #ifdef __cplusplus
// extern "C" {
// #endif

typedef unsigned char byte;


typedef enum {
	GenericFile
} GenericError;

typedef enum {
        SOCreation,
        SOBind,
        SOListening,
        SOAccept,
        SOSend,
        SORecv,
        SOAddr
} SOError;

typedef enum {
        PollGeneric,
        PollTimeout,
        PollRecvBusy,
        PollIn,
        PollOut
} PollError;

typedef enum {
        ImWrongSize,
        ImUnsopportedColorSpace
} ImError;

class SocketPiUy {
private:
    struct pollfd ufds[2];
    struct sockaddr_in server_addr;
    struct sockaddr_in pi_addr;
    struct sockaddr_in uy_addr;
    int fd_server;
    int fd_pi;
    int fd_uy;
    std::string iface = "eth0";
    unsigned int port;
    int getAddress(struct in_addr *addr, const char *iface);
public:
    SocketPiUy(const char *iface, unsigned int port) : iface(iface), port(port) {};
    int start();
    int waitConnection();
    int waitData(int is_recv);
    int send(byte *cbuf, size_t l, int flags);
    int recv(byte *cbuf, size_t l, int flags);
    int closeRead();
    int closeServer();
    int close();
};


// #ifdef __cplusplus
// }
// #endif
#endif