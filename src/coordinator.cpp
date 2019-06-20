
#include "holooj.hpp"
#include "coordinator.hpp"
#include "ncs.hpp"

#include <stdio.h>
#include <string.h>
#include <turbojpeg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
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

#define RECV(rl, fd, buf, l, flags) if((rl = recv(fd, buf, l, flags)) == -1) return -1;
#define SEND(rl, fd, buf, l, flags) if((rl = send(fd, buf, l, flags)) == -1) return -1;

typedef enum PckError {
        PckGeneric,
        PckSTX,
        PckTooSmall
} PckError;

typedef enum TjError {
        TjGeneric,
        TjHeader,
        TjDecompress,
        TjCompress,
        TjDestroy
} TjError;


int decode_jpeg(byte *imj, int imjl, byte *im) {
    tjhandle tjh;
    int w, h, ss, cl, ret;
	std::string color_space;
    
    tjh = tjInitDecompress();
	REPORTSPD(tjh == NULL, "Tj init error: «{}».", tjGetErrorStr());
		
    ret = tjDecompressHeader3(tjh, imj, imjl, &w, &h, &ss, &cl);
	REPORTSPD(ret, "Tj decompress header error: «{}».", tjGetErrorStr());
    switch(cl) {
        case TJCS_CMYK  :    color_space = "CMYK"  ;    break;
        case TJCS_GRAY  :    color_space = "GRAY"  ;    break;
        case TJCS_RGB   :    color_space = "RGB"   ;    break;
        case TJCS_YCbCr :    color_space = "YCbCr" ;    break;
        case TJCS_YCCK  :    color_space = "YCCK"  ;    break;
    }
	//REPORTSPD(cl != TJCS_RGB, "Tj error: «Wrong color space (only RGB is supported?), {} instead of {}.»", cl, TJCS_RGB);
	SPDLOG_INFO("Color space is {}", color_space);

	ret = tjDecompress2(tjh, imj, imjl, im, 0, 0, 0, TJPF_RGB, 0);
	REPORTSPD(ret, "Tj decompress error: «{}»", tjGetErrorStr());

    REPORTSPD(tjDestroy(tjh), "Tj destroying error: «{}»", tjGetErrorStr());

    return 0;
}

int Coordinator::getAddress(struct in_addr *addr, const char *iface) {

	struct ifaddrs *ifaddr, *ifa;
	REPORTSPD_ERRNO(-1 == getifaddrs(&ifaddr));

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

int Coordinator::startServer() {
	SPDLOG_DEBUG("Start.");
	int ret;
	char caddr[20];

	fd_server = socket(AF_INET, SOCK_STREAM, 0);
	REPORTSPD_ERRNO(0 > this->fd_server);

	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if(getAddress(&server_addr.sin_addr, iface.c_str())) return -1;
	inet_ntop(AF_INET, &(server_addr.sin_addr), caddr, INET_ADDRSTRLEN);
	SPDLOG_INFO("Socket Server started at: {}:{}", caddr, port);

	ret = bind(this->fd_server, (struct sockaddr *) &server_addr, sizeof(server_addr));
	REPORTSPD_ERRNO(0 > ret);

	ret = listen(this->fd_server, 3);
	REPORTSPD_ERRNO(0 > ret);

	SPDLOG_DEBUG("End.");
	return 0;
}

int Coordinator::waitPiAndUy() {
	SPDLOG_DEBUG("Start.");
	int r, id, fd;
	struct sockaddr_in addr;
    int addrlen = sizeof(addr);
	char caddr[20];
	byte buf[12];
	timeouts = 0;
	
	for(int i = 0; i < 2; i++) {
		fd = accept(this->fd_server, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
		inet_ntop(AF_INET, &(addr.sin_addr), caddr, INET_ADDRSTRLEN);
		REPORTSPD_ERRNO(0 > fd);

		r = recv(fd, &buf, 12, 0);
		REPORTSPD(r < 4, "Too few bytes ({}).", r);

		id = *((int*) buf);
		if(id == 27) {
			if(r == 12) {
				fd_pi = fd;
				imcols = *((int*) &buf[4]);
				imrows = *((int*) &buf[8]);
				imsize = imrows*imcols*3;
				SPDLOG_INFO("Connected to Pi: {}", caddr);
				SPDLOG_INFO("Image sizes: {}x{} => raw size {}", imcols, imrows, imsize);
			} else {
				SPDLOG_ERROR("Connected to Pi but received {} instead of 12 bytes.", r);
				return -1;
			}
		} else {
			REPORTSPD(id != 54, "Connected to some device but received wrong id ({}).", id);
			REPORTSPD(r != 4, "Connected to Unity but received {} instead of 4 bytes.", r);
			SPDLOG_INFO("Connected to Unity: {}", caddr);
			fd_uy = fd;
		}
	}

	REPORTSPD(fd_uy == -1, "Unity device not connected.");
	REPORTSPD(fd_pi == -1, "Pi device not connected.");

	ufds[0].fd = this->fd_pi;
	ufds[0].events = POLLIN | POLLOUT; // check for normal or out-of-band

	ufds[1].fd = this->fd_uy;
	ufds[1].events = POLLIN | POLLOUT; // check for normal or out-of-band

	SPDLOG_DEBUG("End.");
	return 0;
}

int Coordinator::saveImage2Jpeg(byte *im, int index) {
    int ret;
    unsigned long imjl;
    byte *imj;
    tjhandle tjh;


    imjl = 200000;
    imj = tjAlloc((int) imjl);
    tjh = tjInitCompress();
    ret = tjCompress2(tjh, im, imcols, 3 * imcols, imrows, TJPF_BGR, &imj, &imjl, TJSAMP_444, 100, 0);
    REPORTSPD(ret, "image2jpeg: Tj compress error: «{}»", tjGetErrorStr());
    REPORTSPD(tjDestroy(tjh), "image2jpeg: Tj destroy Error: «{}»)", tjGetErrorStr());
    char filename[30];
    sprintf(filename, "./phs/ph_%d.jpg", index);
    FILE *f = fopen(filename, "wb");
    REPORTSPD(f == NULL, "image2jpeg: file {} creating error[{}]: «{}»", filename, errno, strerror(errno));
    ret = fwrite(imj, 1, imjl, f);
    REPORTSPD(ret < (int) imjl, "Saving image2jpeg file writing error: «{}» %d instead of %lu)", ret, imjl);
    REPORTSPD(fclose(f), "image2jpeg: file closing error: «{}» %d instead of %lu)", ret, imjl);

	tjFree(imj);


    return 0;
}


void Coordinator::drawBbox(byte *im, Box b, byte color[3]) {
    int w = imcols;
    int h = imrows;


    int left  = (b.x-b.w/2.)*w;
    int right = (b.x+b.w/2.)*w;
    int top   = (b.y-b.h/2.)*h;
    int bot   = (b.y+b.h/2.)*h;


    int thickness = 2; //border width in pixel minus 1
    if(left < 0) left = thickness;
    if(right > w-thickness) right = w-thickness;
    if(top < 0) top = thickness;
    if(bot >= h-thickness) bot = h-thickness;

    int top_row = 3*top*w;
    int bot_row = 3*bot*w;
    int left_col = 3*left;
    int right_col = 3*right;
    for(int k = left_col; k < right_col; k+=3) {
        for(int wh = 0; wh <= thickness; wh++) {
            int border_line = wh*w*3;

			int a = k + top_row + border_line;
			int b = k + bot_row + border_line;

            memcpy(&im[k + top_row - border_line], color, 3);
            memcpy(&im[k + bot_row + border_line], color, 3); 

			if(a < 0 || a > imcols*imrows*3) {
				printf("a: %d", a);
			}
			if(b < 0 || b > imcols*imrows*3) {
				printf("b: %d", b);
			}
        }
    }
	int pixel_left = top*3*w + left_col;
	int pixel_right = top*3*w + right_col;
    for(int k = top; k < bot; k++) {
        for(int wh = 0; wh <= thickness*3; wh+=3) {
			int c = pixel_left  - wh;
			int d = pixel_right + wh;
            memcpy(&im[pixel_left  - wh], color, 3);
            memcpy(&im[pixel_right + wh], color, 3);
			if(c < 0 || c > imcols*imrows*3) {
				printf("c: %d", c);
			}
			if(d < 0 || d > imcols*imrows*3) {
				printf("d: %d", d);
			}
        }
		pixel_left += 3*w;
		pixel_right += 3*w;
    }
}


int Coordinator::undistortImage() {
	return 0;
}

int Coordinator::elaborate() {
	SPDLOG_DEBUG("Start elaborating {} bytes.", rpacket->l);
	int nbbox;
	if(!isBMP) {
		if(0 > decode_jpeg(rpacket->image, rpacket->l, imraw)) return -1;
		SPDLOG_DEBUG("Jpeg image decoded.");
	}

	nbbox = ncs->inference_byte(imraw, 3);

    byte color[3] = {250, 0, 0};
	for(int i = nbbox-1; i >= 0; --i) {
		SPDLOG_INFO("\n\t({:7.6f}, {:7.6f}, {:7.6f}, {:7.6f}), o={:7.6f}, p={:7.6f}:\t\t{}",
			ncs->nn.bboxes[i].box.x, ncs->nn.bboxes[i].box.y, ncs->nn.bboxes[i].box.w, ncs->nn.bboxes[i].box.h, ncs->nn.bboxes[i].objectness, ncs->nn.bboxes[i].prob, ncs->nn.classes[ncs->nn.bboxes[i].cindex]);

		drawBbox(imraw, ncs->nn.bboxes[i].box, color);
	}

	if(nbbox >= 0) {
#ifdef OPENCV
		IplImage *iplim = cvCreateImage(cvSize(config.cols, config.rows), IPL_DEPTH_8U, 3);
  		memcpy(iplim->imageData, im, config.cols*config.rows*3);
		cvShowImage("bibo", iplim);
		cvUpdateWindow("bibo");
		// cvWaitKey(10);
	    cvReleaseImage(&iplim);
#endif
		// show_image_cv(im, "bibo2", config.rows, config.cols, 0);
        saveImage2Jpeg(imraw, ++imcounter);
	}

	SPDLOG_DEBUG("Finished elaborating: found {} nbboxes.", nbbox);
	return nbbox;
}	


// int recv_config() {
// 	int ret;

// 	ret = spu->recv((byte *) &config, sizeof(Config), 0);//REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.
	
// 	REPORT_ERRNO(ret < 0, -1, "");
// 	REPORT(ret == 0, -1, "No config received");

// 	ncs->nn.im_cols = config.cols;
// 	ncs->nn.im_rows = config.rows;

// 	buffer_size = (OH_SIZE + config.rows * config.cols * 3) >> (config.isBMP ? 0 : 4);
// 	image_size = config.rows * config.cols * 3;


// 	int l_tot_byte = 0;
// 	for(int i = 0; i < ncs->nn.nclasses; i++) {
// 		l_tot_byte += strlen(ncs->nn.classes[i]) + 1;
// 	}

// 	char cls[12 + l_tot_byte];
// 	int cls_p = 12;
// 	for(int i = 0; i < ncs->nn.nclasses; i++) {
// 		int sl = strlen(ncs->nn.classes[i]);
// 		memcpy(cls + cls_p, ncs->nn.classes[i], sl);
// 		cls[cls_p + sl] = '\0';
// 		cls_p += sl + 1;
// 	}

//     memcpy(cls,		&config.STX, 	4);
//     memcpy(cls + 4,	&l_tot_byte, 			4);
//     memcpy(cls + 8,	&ncs->nn.nclasses, 			4);

//     ret = spu->send((byte *) cls, 12, 0);
// 	REPORT_ERRNO(ret < 0, -1, "");

//     ret = spu->send((byte *) cls + 12, l_tot_byte, 0);
// 	REPORT_ERRNO(ret < 0, -1, "");

// 	return ret;
// }

int Coordinator::recvImage() {
	int rl = recv(fd_pi, (byte*) rpacket, 8, 0);
	REPORTSPD(rpacket->stx != STX, "Wrong STX ({} instead of {}).", rpacket->stx, STX);
	rl = recv(fd_pi, (byte*) rpacket->image, rpacket->l, MSG_WAITALL);
	REPORTSPD(rpacket->stx != STX, "Too few bytes received ({} instead of {}).", rl, rpacket->l);
	return 0;
}

int Coordinator::recvImages() {
	int consecutive_wrong_packets = 0, r;
	rpacket = (RecvPacket *) calloc(buffer_size, 1);
	memset(&spacket, 0, sizeof(SendPacket));
	spacket.stx = STX;
	ncs->nn.bboxes = spacket.bboxes;
	if(!isBMP) {
		imraw = (byte*) calloc(imsize, 1);
	}
    else {
        imraw = rpacket->image;
    }

	SEND(r, fd_pi, (void*) &STX, 4, 0);
	REPORTSPD(r != 4, "Sent wrong bytes number to Pi ({} instead of 4)", r);
	SEND(r, fd_uy, (void*) &STX, 4, 0);
	REPORTSPD(r != 4, "Sent wrong bytes number to Unity ({} instead of 4)", r);
	SPDLOG_INFO("Sent STX to Pi and Unity to start exchage.");

	while(1) {
		int nbbox, sl;

		if(recvImage()) {
			if(++consecutive_wrong_packets == 10) {
				SPDLOG_WARN("Wrong incoming packets from Pi for 10 consecutive times. Stop loop.");
				break;
			} else {
				SPDLOG_WARN("Wrong incoming packet from Pi: skip to next while iteration (consecutives = {}).", consecutive_wrong_packets);
				continue;
			}
		}
		consecutive_wrong_packets = 0;

		sl = send(fd_uy, rpacket, rpacket->l + 8, 0);
		// REPORTSPD(sl < rl, "({} > {}).", rpacket->l + OH_SIZE, rl);

		nbbox = elaborate();

		if(nbbox > 0) {
			int size = 12 + nbbox * sizeof(bbox);
			SEND(sl, fd_uy, &spacket, size, 0);
			SPDLOG_INFO("Sent {} bboxes ({} bytes) to Unity.", nbbox, sl);
			continue;
		} else
		if(nbbox == 0) {
			SPDLOG_INFO("No bbox found.", nbbox, sl);
		} else {
			return -1;
		}
		usleep(1000000);
	}
	free(rpacket);
	return -1;
}


int Coordinator::run(const char *graph, const char *meta, float thresh) {
	int ret;
	ncs = new NCS(graph, meta, NCSNN_YOLOv2);



	ncs->nn.thresh = thresh;

	ret = INT32_MAX;
	while(1) {
		if(startServer()) exit(1);

		SPDLOG_INFO("Socket is waiting new incoming tcp connection at port {}...", port);
		ret = waitPiAndUy();
        if(ret < 0) continue;


		if(ncs->init()) exit(1);

		ret = recvImages();
        if(ret >= 0) {
			break;
		}

		REPORTSPD(closeSockets() < 0, "Closing sockets error.")
		else
			SPDLOG_INFO("Sockets closed.");

	}

	ncs->~NCS();

	return 0;
}


int Coordinator::closeSockets() {
	int r;
	
	r = close(fd_pi);
	REPORTSPD(r < 0, "Error during closing Pi socket.");

	r = close(fd_uy);
	REPORTSPD(r < 0, "Error during closing Unity socket.");

	r = close(fd_server);
	REPORTSPD(r < 0, "Error during closing Server socket.");

	return 0;
}

int Coordinator::recv(int fd, void*buf, size_t len, int flags) {
	int r = ::recv(fd, buf, len, flags);

	REPORTSPD_ERRNO(r < 0)
	else {
		SPDLOG_DEBUG("Recv {} bytes from {}.", r, fd == fd_pi ? "Pi" : "Unity");
	}
	
	return r;
}

int Coordinator::send(int fd, void*buf, size_t len, int flags) {
	int r = ::send(fd, buf, len, flags);

	REPORTSPD_ERRNO(r < 0)
	else {
		SPDLOG_DEBUG("Sent {} bytes to {}.", r, fd == fd_pi ? "Pi" : "Unity");
	}

	return r;
}