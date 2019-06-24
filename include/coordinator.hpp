#ifndef __COORDINATOR_HPP__
#define __COORDINATOR_HPP__

#include "holooj.hpp"
#include "ny2.hpp"
#include "ncs.hpp"
#include <string>
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <ifaddrs.h>


// typedef enum {
// 	GenericFile
// } GenericError;

// typedef enum {
//         SOCreation,
//         SOBind,
//         SOListening,
//         SOAccept,
//         SOSend,
//         SORecv,
//         SOAddr
// } SOError;

// typedef enum {
//         PollGeneric,
//         PollTimeout,
//         PollRecvBusy,
//         PollIn,
//         PollOut
// } PollError;

// typedef enum {
//         ImJpegDecoding,
//         ImWrongSize,
//         ImUnsopportedColorSpace
// } ImError;


typedef struct RecvPacket
{
    int stx;
    int l;
    byte image[];
} RecvPacket;

typedef struct SendPacket
{
    int stx;
    int index;
    int n;
    bbox bboxes[5];
} SendPacket;

class Coordinator {
    private:
        int imcounter = -1;
        float thresh = 0.5;
        const int OH_SIZE = 12;

        int impixel_size = 3;
        float *ncs_pointer;

        /** socket **/
        int timeouts = 10;
        int fd_server;
        int fd_pi = -1;
        int fd_uy = -1;
        struct pollfd ufds[1];
        struct sockaddr_in server_addr;
        struct sockaddr_in pi_addr;
        struct sockaddr_in uy_addr;
    public:
        unsigned int rpacket_buffer_size = 500*1024;
        NCS *ncs;
        RecvPacket *rpacket;
        SendPacket  spacket;
        const int STX = 27692;//767590;
        std::string iface;
        unsigned int port;
        bool isBMP = false;
        Coordinator(const char *iface, int port) : iface(iface), port(port) {};
        int recv(int fd, void* buf, size_t len, int flags);
        int send(int fd, void* buf, size_t len, int flags);
        int getAddress(struct in_addr *addr, const char *iface);
        int startServer();
        int waitPiAndUy();
        int closeSockets();
        int recvConfig();
        int recvImage();
        int recvImages();
        int elaborate();
        int drawBbox(rgb_pixel *im, Box b, rgb_pixel color);
        int saveImage2Jpeg(byte *im, int index);
        int undistortImage();
        int run();
        int init(const char *graph, const char *meta, float thresh);
};

#endif //__COORDINATOR_HPP__