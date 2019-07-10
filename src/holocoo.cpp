
#include "holooj.hpp"
#include "holocoo.hpp"
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

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

#define RECV(rl, fd, buf, l, flags) if((rl = recv(fd, buf, l, flags)) == -1) return -1;
#define SEND(rl, fd, buf, l, flags) if((rl = send(fd, buf, l, flags)) == -1) return -1;

enum AtomicNCS {
	NCS_NOT_DOING,
	NCS_TODO,
	NCS_DOING,
	NCS_DONE,
};
// #define NOT_DOING 0
// #define TODO      1
// #define DOING     2
// #define DONE      3

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


int HoloCoo::getAddress(struct in_addr *addr, const char *iface) {

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


int HoloCoo::startServer() {
	SPDLOG_TRACE("Start.");
	int ret;
	char caddr[20];

	fd_server = socket(AF_INET, SOCK_STREAM, 0);
	REPORTSPD_ERRNO(0 > fd_server);
	
	ret = 1;
	ret = setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &ret, 4);
	REPORTSPD_ERRNO(0 > ret);

	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if(getAddress(&server_addr.sin_addr, iface.c_str())) return -1;
	inet_ntop(AF_INET, &(server_addr.sin_addr), caddr, INET_ADDRSTRLEN);
	SPDLOG_INFO("Socket Server started at: {}:{}", caddr, port);

	ret = bind(fd_server, (struct sockaddr *) &server_addr, sizeof(server_addr));
	REPORTSPD_ERRNO(0 > ret);

	ret = listen(fd_server, 1);
	REPORTSPD_ERRNO(0 > ret);

	SPDLOG_TRACE("End.");
	return 0;
}


int HoloCoo::waitHololens() {
	SPDLOG_TRACE("Start.");
	int r, id, fd;
	struct sockaddr_in addr;
    int addrlen = sizeof(addr);
	char caddr[20];

	fd = socket(AF_INET, SOCK_STREAM, 0);
	REPORTSPD_ERRNO(0 > fd);

	fd = accept(fd_server, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
	inet_ntop(AF_INET, &(addr.sin_addr), caddr, INET_ADDRSTRLEN);
	REPORTSPD_ERRNO(0 > fd);
	SPDLOG_TRACE("Accepted.");

	fd_uy = fd;

	int ret;
	byte buf[16];

	ret = recv(fd_uy, &buf, 16, 0); //REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.
	REPORTSPD(ret < 16, "Too few bytes ({} instead of 16).");

	ncs->nn.im_or_cols  = (*((int*) buf));
	ncs->nn.im_or_rows  = (*((int*) &buf[4]));

	ncs->nn.im_resized_cols = (*((int*) &buf[8]));
	ncs->nn.im_resized_rows = (*((int*) &buf[12]));

	ncs->nn.im_resized_size = ncs->nn.im_resized_cols * ncs->nn.im_resized_rows;
	ncs->nn.im_resized_size_bytes = 3 * ncs->nn.im_resized_size;

	im_bmp = (byte*) calloc(ncs->nn.im_resized_size_bytes, 1);

    ncs->nn.input_letterbox = &ncs->nn.input[((ncs->nn.in_h - ncs->nn.im_resized_rows) >> 1) * ncs->nn.in_w * 3];

	SPDLOG_CRITICAL("\n{}x{}\n{}x{}\n{}*3={}", ncs->nn.im_or_cols, ncs->nn.im_or_rows, ncs->nn.im_resized_cols, ncs->nn.im_resized_rows, ncs->nn.im_resized_size, ncs->nn.im_resized_size_bytes);

	memcpy(buf,     &STX, 4);
	memcpy(buf + 4, &ncs->nn.classes_buffer_length, 4);
	memcpy(buf + 8, &ncs->nn.nclasses, 4);
	ret = send(fd_uy, buf, 12, 0); //REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.

	ret = send(fd_uy, ncs->nn.classes_buffer, ncs->nn.classes_buffer_length, 0); //REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.
	REPORTSPD(ret < ncs->nn.classes_buffer_length, "Too few bytes ({} instead of {}).", ncs->nn.classes_buffer_length);


	return 0;
}


int HoloCoo::saveImage2Jpeg(byte *im, int index) {
    int ret;
    unsigned long imjl;
    byte *imj;
    tjhandle tjh;


    imjl = 200000;
    imj = tjAlloc((int) imjl);
    tjh = tjInitCompress();
    ret = tjCompress2(tjh, im, ncs->nn.im_or_cols, 3 * ncs->nn.im_or_cols, ncs->nn.im_or_rows, TJPF_BGR, &imj, &imjl, TJSAMP_444, 100, 0);
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


int HoloCoo::drawBbox(rgb_pixel *im, Box b, rgb_pixel color) {
	int w, h, wh, left_col, right_col, top_row, bot_row,
		left_overlap, top_overlap, right_overlap, bottom_overlap, thickness, 
		corner_top_left, corner_bot_left, corner_top_right, corner_bot_right,
		top_left_pixel, bot_left_pixel, box_width_pixel;

    w = ncs->nn.im_or_cols;
    h = ncs->nn.im_or_rows;
	wh = w*h;

    left_col  = (b.x-b.w/2.)*w;
    right_col = (b.x+b.w/2.)*w;
    top_row   = (b.y-b.h/2.)*h;
    bot_row   = (b.y+b.h/2.)*h;

	left_overlap = 0;
	top_overlap = 0;
	right_overlap = 0;
	bottom_overlap = 0;
    thickness = 5; //border width in pixel minus 1
    if(left_col < 0) left_col = 0;
    else if(left_col > w-thickness) left_col = w-thickness;
    if(right_col > w-thickness) right_col = w-thickness;
    if(top_row < 0) top_row = 0;
    if(top_row >= h-thickness) top_row = h-thickness;
    if(bot_row >= h-thickness) bot_row = h-thickness;

	left_overlap   = -1 * (left_col  - thickness);
	top_overlap    = -1 * (top_row   - thickness);
	right_overlap  = -1 * (w - right_col - thickness);
	bottom_overlap = -1 * (h - bot_row   - thickness);

	left_overlap   = left_overlap < 0   ? 0 : left_overlap;
	top_overlap    = top_overlap < 0    ? 0 : top_overlap;
	right_overlap  = right_overlap < 0  ? 0 : right_overlap;
	bottom_overlap = bottom_overlap < 0 ? 0 : bottom_overlap;
	


	corner_top_left  = w * top_row + left_col;
	corner_bot_left  = w * bot_row + left_col;
	corner_top_right = w * top_row + right_col + thickness;
	corner_bot_right = w * bot_row + right_col + thickness;
	if(corner_top_left < 0 || corner_top_left > wh) {
		SPDLOG_WARN("Wrong TOP LEFT corner of horizontal borders: {} not in [0, {}].", corner_top_left, wh);
	}
	if(corner_bot_left < 0 || corner_bot_left > wh) {
		SPDLOG_WARN("Wrong BOTTOM LEFT corner of horizontal borders: {} not in [0, {}].", corner_bot_left, wh);
	}
	if(corner_top_right < 0 || corner_top_right > wh) {
		SPDLOG_WARN("Wrong TOP RIGHT corner of horizontal borders: {} not in [0, {}].", corner_top_right, wh);
	}
	if(corner_bot_right < 0 || corner_bot_right > wh) {
		SPDLOG_WARN("Wrong BOTTOM RIGHT corner of horizontal borders: {} not in [0, {}].", corner_bot_right, wh);
	}



	top_left_pixel = w * top_row + left_col;
	bot_left_pixel = w * (bot_row + thickness) + left_col;
	box_width_pixel = right_col - left_col + thickness - left_overlap - right_overlap;
	for(int r = 0; r < thickness; r++) {
		for(int p = 0; p < box_width_pixel; p++) {
			if(r >= top_overlap || p/thickness % 2 == 0) {
				im[top_left_pixel + p] = color;
				if(top_left_pixel + p < 0 || top_left_pixel + p > wh) {
					SPDLOG_WARN("Overflow: {0} < 0 || {0} > {1}.", top_left_pixel + p, wh);
					return -1;
				}
			}
			if(r >= bottom_overlap || p/thickness % 2 == 0) {
				im[bot_left_pixel + p] = color;
				if(bot_left_pixel + p < 0 || bot_left_pixel + p > wh) {
					SPDLOG_WARN("Overflow: {0} < 0 || {0} > {1}", bot_left_pixel + p, wh);
					return -1;
				}
			}
		}
		top_left_pixel += w;
		bot_left_pixel += w;
	}



	top_row += thickness;
	bot_row += 5;
	corner_top_left  = w * top_row + left_col;
	corner_bot_left  = w * bot_row + left_col;
	corner_top_right = w * top_row + left_col + thickness;
	corner_bot_right = w * bot_row + left_col + thickness;
	if(corner_top_left < 0 || corner_top_left > wh) {
		SPDLOG_WARN("Wrong TOP LEFT corner of vertical borders: {} not in [0, {}].", corner_top_left, wh);
	}
	if(corner_bot_left < 0 || corner_bot_left > wh) {
		SPDLOG_WARN("Wrong BOTTOM LEFT corner of vertical borders: {} not in [0, {}].", corner_bot_left, wh);
	}
	if(corner_top_right < 0 || corner_top_right > wh) {
		SPDLOG_WARN("Wrong TOP RIGHT corner of vertical borders: {} not in [0, {}].", corner_top_right, wh);
	}
	if(corner_bot_right < 0 || corner_bot_right > wh) {
		SPDLOG_WARN("Wrong BOTTOM RIGHT corner of vertical borders: {} not in [0, {}].", corner_bot_right, wh);
	}
    for(int r = top_row; r <= bot_row; r++) {
		for(int b = 0; b < thickness; b++) {
			if(b < thickness - left_overlap || (r - top_row)/thickness % 2 == 0) {
				im[r*w + left_col + b] = color;
				if(r*w + left_col + b < 0  || r*w + left_col + b > wh) {
					SPDLOG_WARN("Overflow: {0} < 0 || {0} > {1}", r*w + left_col + b, wh);
					return -1;
				}
			}
			if(b >= right_overlap || (r - top_row)/thickness % 2 == 0) {
				im[r*w + right_col + b] = color;
				if(r*w + right_col + b < 0 || r*w + right_col + b > wh) {
					SPDLOG_WARN("Overflow: {0} < 0 || {0} > {1}", r*w + right_col + b, wh);
					return -1;
				}
			}
		}
    }
}


int HoloCoo::elaborateImage() {
	SPDLOG_DEBUG("Start image elaboration ({} bytes).", rpacket->l);


    SPDLOG_WARN("images={}", counter);

	//decode_jpeg(rpacket->image, rpacket->l, im_bmp);

	SPDLOG_TRACE("Stop.");
	return 0;
}


int HoloCoo::recvImage() {
	SPDLOG_TRACE("Start.");

	SPDLOG_DEBUG("Waiting image...");
	int rl = recv(fd_uy, rpacket, 12, MSG_WAITALL);


	if(rpacket->type == 1 && (int) rpacket_buffer_size < rpacket->l) {
		SPDLOG_WARN("Buffer not enough large for incoming packet: {} > {}. Skipped.", rpacket->l, rpacket_buffer_size);
		rl = recv(fd_uy, rpacket->image, rpacket->l, 0);
		return -2;
	}

	rl = recv(fd_uy, rpacket->image, rpacket->l/* OVERHEAD_SIZE + ncs->nn.im_resized_size_bytes*/, MSG_WAITALL);
	REPORTSPD(rpacket->stx != STX, "Wrong STX ({} instead of {}).", rpacket->stx, STX);

	if(rpacket->type == 0 && (int) rl != rpacket->l) {
		SPDLOG_WARN("Wrong image size for streaming: {} != {}. Skipped.", rpacket->l, rpacket_buffer_size);
		return -3;
	}

	SPDLOG_TRACE("Stop.");
	return 0;
}

int HoloCoo::elaborate_ncs() {
	SPDLOG_TRACE("Start.");
	int expected, nbbox;

	memcpy(im_bmp, rpacket->image, rpacket->l);

	nbbox = ncs->inference_byte(im_bmp, 3, true);

	SPDLOG_DEBUG("Inference done: found {} nbbox", nbbox);

	spacket.stx = STX;
	spacket.n = nbbox;
	send(fd_uy, (byte*) &spacket, sizeof(SendPacket), 0);

	for(int i = nbbox-1; i >= 0; --i) {
		rgb_pixel color;
		byte c1 = 250 * i / 3;
		byte c2 = 250 * i / 3;
		byte c3 = 250 * i / 3;
		SPDLOG_INFO("\n\t({:7.6f}, {:7.6f}, {:7.6f}, {:7.6f}), o={:7.6f}, p={:7.6f}:\t\t{}",
			ncs->nn.bboxes[i].box.x, ncs->nn.bboxes[i].box.y, ncs->nn.bboxes[i].box.w, ncs->nn.bboxes[i].box.h, ncs->nn.bboxes[i].objectness, ncs->nn.bboxes[i].prob, ncs->nn.classes[ncs->nn.bboxes[i].cindex]);

		//drawBbox((rgb_pixel*) im_bmp, ncs->nn.bboxes[i].box, color);
	}
	SPDLOG_TRACE("Stop {}.", nbbox);
	return nbbox;
}

int HoloCoo::recvImagesLoop() {
	SPDLOG_TRACE("Start.");
	int consecutive_wrong_packets = 0;
	int stxs[2] = { STX, STX };
	rpacket = (RecvPacket *) calloc(sizeof(RecvPacket) + rpacket_buffer_size, 1);
	memset(&spacket, 0, sizeof(SendPacket));
	spacket.stx = STX;
	ncs->nn.bboxes = spacket.bboxes;

	

	while(1) {
		int nbbox, sl, ba;

		sl = recvImage();
		if(sl) {
			if(sl == -2) {
				continue;
			} else
			if(++consecutive_wrong_packets == 10) {
				SPDLOG_WARN("Wrong incoming packets from Pi for 10 consecutive times. Stop loop.");

				sl = send(fd_pi, (void*) "\0\0\0\0", 16, 0);
				break;
			} else {
				int bytes_available = -1;
				SPDLOG_WARN("Wrong incoming packet from Pi: skip to next while iteration (consecutives = {}, {}).", consecutive_wrong_packets, bytes_available);
				continue;
			}
		}
		consecutive_wrong_packets = 0;

		//if(0 > elaborateImage()) break;
		if(rpacket->type == 1) {
			if(0 > elaborate_ncs())  break;
		} else
		if(rpacket->type == 0) {
			cv::Mat mat_jpeg(rpacket->l, 1, CV_8UC3, rpacket->image);
			auto mat_bmp = cv::imdecode(mat_jpeg, cv::IMREAD_COLOR);
			cv::imshow("streaming_window", mat_bmp);
			cv::waitKey(1);
			// std::stringstream fname;
			// fname << "/home/developer/pimovity-holo/phs/holo_" << counter++ << "%d.jpg";
			// FILE*f = fopen(fname.str().c_str(), "wb");
			// int fr = fwrite( rpacket->image, 1, rpacket->l, f );
			// if(fr < 0) {
			// 	SPDLOG_ERROR("Error during writing.");
			// } else
			// if(fr != rpacket->l) {
			// 	SPDLOG_ERROR("Not all bytes written to file.");
			// }
			// fclose(f);
		}
	}

	int to_discard = 0;
	REPORTSPD_ERRNO(0 > ioctl(fd_uy, FIONREAD, &to_discard));
	while(to_discard > rpacket_buffer_size) {
		to_discard -= recv(fd_uy, rpacket->image, to_discard > rpacket_buffer_size ? rpacket_buffer_size : to_discard, 0);
	}

	free(rpacket);
	SPDLOG_TRACE("END.");
	return -1;
}

int HoloCoo::init(const char *graph, const char *meta, float thresh) {
	ncs = new NCS(graph, meta, NCSNN_YOLOv2);
	ncs->initNN();
	ncs->nn.thresh = thresh;

}


int HoloCoo::run(unsigned int port) {
	int ret;

	this->port = port;

	ret = INT32_MAX;

	SPDLOG_INFO("Starting server...");
	if(startServer()) return -1;


    cv::namedWindow("streaming_window", cv::WINDOW_NORMAL  );
	cv::resizeWindow("streaming_window", 1280, 720);

	if(ncs->initDevice()) exit(1);

	while(1) {
		SPDLOG_INFO("Waiting for Hololens...");
		ret = waitHololens();
		if(ret < 0) {
			SPDLOG_INFO("«waitHololens» error: closing socket...");
			closeClients();
			continue;
		}

		ret = recvImagesLoop();
		if(ret < 0) {
			SPDLOG_INFO("recvImagesLoop error: closing socket...");
			closeClients();
			continue;
		}
	}

	ncs->~NCS();

	cv::destroyAllWindows();

	return 0;
}


int HoloCoo::closeServer() {
	int r;
	
	if(fd_server > 0) {
		r = close(fd_server);
		REPORTSPD(r < 0, "Error during closing Server socket.");
	}

	return 0;
}

int HoloCoo::closeClients() {
	int r;
	
	if(fd_pi > 0) {
		r = close(fd_pi);
		REPORTSPD(r < 0, "Error during closing Pi socket.");
	}

	if(fd_uy > 0) {
		r = close(fd_uy);
		REPORTSPD(r < 0, "Error during closing Unity socket.");
	}

	return 0;
}

int HoloCoo::closeSockets() {
	int r;
	
	if(fd_pi > 0) {
		r = close(fd_pi);
		REPORTSPD(r < 0, "Error during closing Pi socket.");
	}

	if(fd_uy > 0) {
		r = close(fd_uy);
		REPORTSPD(r < 0, "Error during closing Unity socket.");
	}

	if(fd_server > 0) {
		r = close(fd_server);
		REPORTSPD(r < 0, "Error during closing Server socket.");
	}

	return 0;
}

int HoloCoo::recv(int fd, void*buf, size_t len, int flags) {
	int r = ::recv(fd, buf, len, flags);

	REPORTSPD_ERRNO(r < 0)
	else {
		SPDLOG_DEBUG("Recv {} bytes from {}.", r, fd == fd_pi ? "Pi" : "Unity");
	}
	
	return r;
}

int HoloCoo::send(int fd, void*buf, size_t len, int flags) {
	int r = ::send(fd, buf, len, flags);

	REPORTSPD_ERRNO(r < 0)
	else {
		SPDLOG_DEBUG("Sent {} bytes to {}.", r, fd == fd_pi ? "Pi" : "Unity");
	}

	return r;
}
