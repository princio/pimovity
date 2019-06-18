#ifndef __COORDINATOR_HPP__
#define __COORDINATOR_HPP__

#include "holooj.h"
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
        ImJpegDecoding,
        ImWrongSize,
        ImUnsopportedColorSpace
} ImError;


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
        NCS *ncs;
        RecvPacket *rpacket;
        SendPacket  spacket;
        byte *imraw;
        int imcols;
        int imrows;
        int imsize;
        int imcounter = -1;
        float thresh = 0.5;
        const int OH_SIZE = 12;
        int buffer_size = 1000*1024;

        /** socket **/
        int timeouts = 10;
        int fd_server;
        int fd_pi;
        int fd_uy;
        struct pollfd ufds[1];
        struct sockaddr_in server_addr;
        struct sockaddr_in pi_addr;
        struct sockaddr_in uy_addr;
    public:
        const int STX = 767590;
        std::string iface;
        unsigned int port;
        bool isBMP = false;
        Coordinator(const char *iface, int port) : iface(iface), port(port) {};
        int getAddress(struct in_addr *addr, const char *iface);
        int startServer();
        int waitPiAndUy();
        int closeSockets();
        int recvConfig();
        int recvImages();
        int elaborate();
        int saveImage2Jpeg(byte *im, int index);
        int undistortImage();
        void drawBbox(byte *im, Box b, byte color[3]);
        int run(const char *graph, const char *meta, float thresh);
};

#endif //__COORDINATOR_HPP__