
#include "socket.hpp"
#include "holooj.h"
#include "ny2.hpp"


#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <cstring>
#include <cstdio>
#include <stdlib.h>
#include <time.h>
#include <turbojpeg.h>
#include <unistd.h>
#include <stdexcept>

#ifdef OPENCV
#include <cv.h>
#include <highgui.h>
#endif


#define PORT_IN 56789
#define QUEUE_LENGTH 3

#define MOV
#define SHOWIMAGE

#define RECV 1
#define SEND 0

#define RECV_CONTINUE { usleep(1000); continue; }




int timeouts = 0;


int SocketPiUy::getAddress(struct in_addr *addr, const char *iface) {

	struct ifaddrs *ifaddr, *ifa;
	REPORT(-1 == getifaddrs(&ifaddr), SOAddr, "");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		int condition = !strcmp(ifa->ifa_name, iface) && ifa->ifa_addr && (ifa->ifa_addr->sa_family == AF_INET);

		addr->s_addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
	    if (condition) {
	        break;
	    }
	}
	freeifaddrs(ifaddr);

	return 0;
}

int SocketPiUy::start() {
	int ret;

	this->fd_server = socket(AF_INET, SOCK_STREAM, 0);
	REPORT_ERRNO(0 > this->fd_server, SOCreation, "");

	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if(getAddress(&server_addr.sin_addr, iface.c_str)) return -1;

	ret = bind(this->fd_server, (struct sockaddr *) &server_addr, sizeof(server_addr));
	REPORT_ERRNO(0 > ret, SOBind, "");

	ret = listen(this->fd_server, 3);
	REPORT_ERRNO(0 > ret, SOListening, "");

	return 0;
}


int SocketPiUy::waitConnection() {
	int r, id1, id2;
    int addrlen = sizeof(pi_addr);
	timeouts = 0;
	
	this->fd_pi = accept(this->fd_server, (struct sockaddr *)&pi_addr, (socklen_t*)&addrlen);
	REPORT_ERRNO(0 > this->fd_pi, SOAccept, "");
	r = ::recv(this->fd_pi, &id1, 4, 0);
	REPORT_ERRNO(4 != r, SORecv, "");

	this->fd_uy = accept(this->fd_server, (struct sockaddr *)&pi_addr, (socklen_t*)&addrlen);
	REPORT_ERRNO(0 > this->fd_uy, SOAccept, "");
	r = ::recv(this->fd_uy, &id1, 4, 0);
	REPORT_ERRNO(4 != r, SORecv, "");


	REPORT_ERRNO(id1 != 27 || id1 != 54, SOAccept, "");
	REPORT_ERRNO(id2 != 27 || id2 != 54, SOAccept, "");

	if(id1 == 54 && id2 == 27) {
		int temp = this->fd_pi;
		this->fd_pi = this->fd_uy;
		this->fd_uy = temp;
	}

	ufds[0].fd = this->fd_pi;
	ufds[0].events = POLLIN | POLLOUT; // check for normal or out-of-band

	ufds[1].fd = this->fd_uy;
	ufds[1].events = POLLIN | POLLOUT; // check for normal or out-of-band

	return 0;
}


int SocketPiUy::recv(byte *buf, size_t l, int flags) {
	int is_recv = 1;
	int ret;

	if(waitData(is_recv)) return -1;
	ret = ::recv(fd_pi, buf, l, flags);
	REPORT_ERRNO(ret < 0, SORecv, "");
	return ret;
}

int SocketPiUy::send(byte *buf, size_t l, int flags) {
	int is_recv = 0;
	int ret;

	if(waitData(is_recv)) return -1;
	ret = ::send(fd_pi, buf, l, flags);
	REPORT_ERRNO(ret < 0, SOSend, "");
	return ret;
}

int SocketPiUy::waitData(int is_recv) {
	int rv = poll(ufds, 1, 100);
	REPORT_ERRNO(rv == -1, PollGeneric, "");
	if(rv==0) ++timeouts;
	REPORT_ERRNO(rv == 0,  PollTimeout, "");
	if(is_recv) {
		rv = ufds[0].revents | POLLIN;
		REPORT_ERRNO(!rv,  PollIn, "");
	} else {
		rv = ufds[0].revents | POLLOUT;
		REPORT_ERRNO(!rv,  PollOut, "");
	}
	return !rv;
}

int SocketPiUy::closeRead() {
	if(::close(fd_pi)) {
		printf("\nError during closing read socket [errno=%d].\n", errno);
		return -1;
	}
	return 0;
}

int SocketPiUy::closeServer() {
	if(::close(fd_server)) {
		printf("\nError during closing server socket [errno=%d].\n", errno);
		return -1;
	}
	return 0;
}

int SocketPiUy::close() {
	if(this->closeRead()) {
		return -1;
	}
	if(this->closeServer()) {
		return -1;
	}
	return 0;
}