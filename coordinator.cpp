



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

#include "holooj.h"
#include "coordinator.hpp"
#include "ncs.hpp"

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
    
    tjh = tjInitDecompress();
		
    if(tjDecompressHeader3(tjh, imj, imjl, &w, &h, &ss, &cl)) {
        REPORT(1, TjHeader, "(%s)", tjGetErrorStr());
    }
    switch(cl) {
        case TJCS_CMYK: printf("CMYK"); break;
        case TJCS_GRAY: printf("GRAY"); break;
        case TJCS_RGB: printf("RGB"); break;
        case TJCS_YCbCr: printf("YCbCr"); break;
        case TJCS_YCCK: printf("YCCK"); break;
    }
	REPORT(cl != TJCS_RGB, ImUnsopportedColorSpace, "");
	ret = tjDecompress2(tjh, imj, imjl, im, 0, 0, 0, TJPF_RGB, 0);
	REPORT(ret, TjDecompress, "(%s)", tjGetErrorStr());
    REPORT(tjDestroy(tjh), TjDestroy, "(%s)", tjGetErrorStr());

    return 0;
}

int Coordinator::getAddress(struct in_addr *addr, const char *iface) {

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

int Coordinator::startServer() {
	int ret;

	fd_server = socket(AF_INET, SOCK_STREAM, 0);
	REPORT_ERRNO(0 > this->fd_server, SOCreation, "");

	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if(getAddress(&server_addr.sin_addr, iface.c_str())) return -1;

	ret = bind(this->fd_server, (struct sockaddr *) &server_addr, sizeof(server_addr));
	REPORT_ERRNO(0 > ret, SOBind, "");

	ret = listen(this->fd_server, 3);
	REPORT_ERRNO(0 > ret, SOListening, "");

	return 0;
}

int Coordinator::waitPiAndUy() {
	int r, id1, id2;
    int addrlen = sizeof(pi_addr);
	byte buf[12];
	timeouts = 0;
	
	this->fd_pi = accept(this->fd_server, (struct sockaddr *)&pi_addr, (socklen_t*)&addrlen);
	REPORT_ERRNO(0 > this->fd_pi, SOAccept, "");
	r = recv(this->fd_pi, &buf, 12, 0);
	REPORT_ERRNO(12 != r, SORecv, "");

	id1 = *((int*) buf);
	imcols = *((int*) &buf[4]);
	imrows = *((int*) &buf[8]);
	imsize = imrows*imcols;

	this->fd_uy = accept(this->fd_server, (struct sockaddr *)&pi_addr, (socklen_t*)&addrlen);
	REPORT_ERRNO(0 > this->fd_uy, SOAccept, "");
	r = recv(this->fd_uy, &id2, 4, 0);
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

int Coordinator::saveImage2Jpeg(byte *im, int index) {
    int ret;
    unsigned long imjl;
    byte *imj;
    tjhandle tjh;


    imjl = 200000;
    imj = tjAlloc((int) imjl);
    tjh = tjInitCompress();
    ret = tjCompress2(tjh, im, imcols, 3 * imcols, imrows, TJPF_BGR, &imj, &imjl, TJSAMP_444, 100, 0);
    REPORT(ret, TjCompress, "(%s)", tjGetErrorStr());
    REPORT(tjDestroy(tjh), TjDestroy, "(%s)", tjGetErrorStr());
    char filename[30];
    sprintf(filename, "./phs/ph_%d.jpg", index);
    FILE *f = fopen(filename, "wb");
    REPORT(f == NULL, GenericFile, "(fopen error)");
    ret = fwrite(imj, 1, imjl, f);
    REPORT(ret < (int) imjl, GenericFile, "(fwrite error: %d instead of %lu)", ret, imjl);
    REPORT(fclose(f), GenericFile, "(fclose error)");

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
	int nbbox;
	if(!isBMP) {
		int ret = decode_jpeg(rpacket->image, rpacket->l, imraw);
		REPORT(ret, ImJpegDecoding, "Jpeg decoding failed.");
	}

	nbbox = ncs->inference_byte(imraw, 3);

    byte color[3] = {250, 0, 0};
	for(int i = nbbox-1; i >= 0; --i) {
		printf("\t(%7.6f, %7.6f, %7.6f, %7.6f), o=%7.6f, p=%7.6f:\t\t%s\n",
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

int Coordinator::recvImages() {
	int nodata = 0;
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

	while(1) {
		int rl = recv(fd_pi, (byte*) rpacket, buffer_size, isBMP ? MSG_WAITALL : 0);
		if(rl >= 0 ) {
			// printf("RECV:\t%5d\t%5d | %5d\n", rl, rpacket->index, rpacket->l);
			if(rl > 0) {
				int nbbox, sl;

				REPORT(rpacket->stx != STX, PckSTX, "(%d != %d).", rpacket->stx, STX);
				REPORT(rpacket->l + OH_SIZE > rl, PckTooSmall, "(%d > %d).", rpacket->l + OH_SIZE, rl);

				sl = send(fd_uy, rpacket, buffer_size, isBMP ? MSG_WAITALL : 0);
				REPORT(sl < rl, PckTooSmall, "(%d > %d).", rpacket->l + OH_SIZE, rl);

				nbbox = elaborate();
				if(nbbox >= 0) {
					sl = send(fd_uy, &spacket, 12 + nbbox * sizeof(bbox), 0);
					if(!sl) {
						printf("SEND:\t%-5d\t%5d | %5d\n\n", sl, spacket.index, spacket.n);
						continue;
					}
				}
			}
			else
			if(rl == 0) {
				if(++nodata == 10) break;
				printf("[Warning %s::%d]: recv return 0.\n", __FILE__, __LINE__);
			}
		}
		else {
			break;
		}
		usleep(1000000);
	}
	free(rpacket);
	return -1;
}


int Coordinator::run(const char *graph, const char *meta, float thresh) {
	int ret;
	ncs = new NCS(graph, meta, NCSNN_YOLOv2);

	printf("\nNCS initialization...");
	if(ncs->init()) exit(1);
	printf("OK\n");


	ncs->nn.thresh = thresh;

	ret = INT32_MAX;
	while(1) {
		printf("\nSocket is starting...");
		if(startServer()) exit(1);
		printf("OK\n");

		// if(ret != INT32_MAX) {
		// 	if(spu.closeRead()) {
		// 		break;
		// 	}
		// }

		printf("\nSocket is waiting new incoming tcp connection at port %d...", port);
		ret = waitPiAndUy();
        if(ret < 0) continue;
		printf("OK\n");

		printf("Receiving started...\n");
		ret = recvImages();
        if(ret >= 0) {
			break;
		}

		printf("\n\nClosing all...");
		closeSockets();
		printf("OK\n");

	}

	ncs->~NCS();

	return 0;
}


int Coordinator::closeSockets() {
	int ret = 0;
	ret += close(fd_pi);
	if(ret < 0) printf("Error during closing Pi socket.");
	ret += close(fd_uy);
	if(ret < 0) printf("Error during closing Unity socket.");
	ret += close(fd_server);
	if(ret < 0) printf("Error during closing server socket.");

	return ret;
}