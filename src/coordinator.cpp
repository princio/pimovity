
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

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

#define RECV(rl, fd, buf, l, flags) if((rl = recv(fd, buf, l, flags)) == -1) return -1;
#define SEND(rl, fd, buf, l, flags) if((rl = send(fd, buf, l, flags)) == -1) return -1;


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
	SPDLOG_TRACE("Start.");
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

	SPDLOG_TRACE("End.");
	return 0;
}


int Coordinator::waitPiAndUy() {
	SPDLOG_TRACE("Start.");
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

				ncs->setSizes(*((int*) &buf[4]), *((int*) &buf[8]));
				
				SPDLOG_INFO("Connected to Pi: {}", caddr);
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

	SPDLOG_TRACE("End.");
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


int Coordinator::drawBbox(rgb_pixel *im, Box b, rgb_pixel color) {
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
				if(top_left_pixel + p < 0 || top_left_pixel + p > wh) SPDLOG_WARN("Overflow: {0} < 0 || {0} > {1}.", top_left_pixel + p, wh);
			}
			if(r >= bottom_overlap || p/thickness % 2 == 0) {
				im[bot_left_pixel + p] = color;
				if(bot_left_pixel + p < 0 || bot_left_pixel + p > wh) SPDLOG_WARN("Overflow: {0} < 0 || {0} > {1}", bot_left_pixel + p, wh);
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
				if(r*w + left_col + b < 0  || r*w + left_col + b > wh)  SPDLOG_WARN("Overflow: {0} < 0 || {0} > {1}", r*w + left_col + b, wh);
			}
			if(b >= right_overlap || (r - top_row)/thickness % 2 == 0) {
				im[r*w + right_col + b] = color;
				if(r*w + right_col + b < 0 || r*w + right_col + b > wh) SPDLOG_WARN("Overflow: {0} < 0 || {0} > {1}", r*w + right_col + b, wh);
			}
		}
    }
}


int Coordinator::undistortImage() {
	return 0;
}


int Coordinator::elaborate() {
	SPDLOG_DEBUG("Start elaborating {} bytes.", rpacket->l);
	int nbbox;
	cv::Mat mat_raw;
	cv::Mat mat_raw_resized;

	printf("##%p\n", (void*) rpacket->image);

	if(!isBMP) {
		cv::Mat mat_jpeg(rpacket->l, 1, CV_8UC3, rpacket->image);
		mat_raw = cv::imdecode(mat_jpeg, cv::IMREAD_COLOR);
		cv::resize(mat_raw, mat_raw_resized, cv::Size(ncs->nn.im_resized_cols, ncs->nn.im_resized_rows));
		// mat_raw_resized.convertTo(mat_raw_resized, CV_8UC3);
		// mat_raw_resized.cv
	}
	REPORTSPD(mat_raw_resized.total() != ncs->nn.im_resized_size, "Resizing sizes not matching yolov2 input: {} instead of {}.", mat_raw_resized.total(), ncs->nn.im_resized_size);

	//memcpy(ncs_pointer, mat_raw_resized.data, imsize_resized*4);
	cv::imshow("nn_input", mat_raw_resized);
	cv::waitKey(0);
	nbbox = ncs->inference_byte(mat_raw_resized.data, 5);
	SPDLOG_INFO("Found {} bboxes.", nbbox);


	for(int i = nbbox-1; i >= 0; --i) {
    	rgb_pixel color;
		byte c1 = 250 * i / 3;
		byte c2 = 250 * i / 3;
		byte c3 = 250 * i / 3;
		SPDLOG_INFO("\n\t({:7.6f}, {:7.6f}, {:7.6f}, {:7.6f}), o={:7.6f}, p={:7.6f}:\t\t{}",
			ncs->nn.bboxes[i].box.x, ncs->nn.bboxes[i].box.y, ncs->nn.bboxes[i].box.w, ncs->nn.bboxes[i].box.h, ncs->nn.bboxes[i].objectness, ncs->nn.bboxes[i].prob, ncs->nn.classes[ncs->nn.bboxes[i].cindex]);

		drawBbox((rgb_pixel*) mat_raw.data, ncs->nn.bboxes[i].box, color);
	}
	cv::Mat mat_nn_input (416, 416, CV_32FC3, ncs->nn.input);
	cv::imshow("nn_input", mat_nn_input);
	cv::waitKey(0);

	if(nbbox >= 0) {
		std::stringstream fname;
		SPDLOG_WARN("Showing image.");
		fname << "/home/developer/Desktop/pr2/phs/im_" << ++imcounter << ".jpg";
		cv::imwrite(fname.str(), mat_raw);
		try {
			cv::imwrite(fname.str(), mat_raw);
		}
		catch (std::runtime_error& ex) {
			fprintf(stderr, "Exception converting image to PNG format: %s\n", ex.what());
			return 1;
		}
		cv::imshow("original", mat_raw);
		cv::waitKey(0);
	}

	SPDLOG_DEBUG("Finished elaborating: found {} nbboxes.", nbbox);
	return nbbox;
}


int Coordinator::recvImage() {
	int rl = recv(fd_pi, (byte*) rpacket, 8, 0);
	REPORTSPD(rpacket->stx != STX, "Wrong STX ({} instead of {}).", rpacket->stx, STX);

	if(rpacket_buffer_size < rpacket->l) {
		printf("Image too large: %ud > %d. Skipped.", rpacket_buffer_size, rpacket->l);
		return -2;
	}

	rl = recv(fd_pi, (byte*) rpacket->image, rpacket->l, MSG_WAITALL);
	REPORTSPD(rpacket->stx != STX, "Too few bytes received ({} instead of {}).", rl, rpacket->l);

	return 0;
}


int Coordinator::recvImages() {
	int consecutive_wrong_packets = 0, r;
	rpacket = (RecvPacket *) calloc(sizeof(RecvPacket) + rpacket_buffer_size, 1);
	memset(&spacket, 0, sizeof(SendPacket));
	spacket.stx = STX;
	ncs->nn.bboxes = spacket.bboxes;

	SEND(r, fd_pi, (void*) &STX, 4, 0);
	REPORTSPD(r != 4, "Sent wrong bytes number to Pi ({} instead of 4)", r);
	SEND(r, fd_uy, (void*) &STX, 4, 0);
	REPORTSPD(r != 4, "Sent wrong bytes number to Unity ({} instead of 4)", r);
	SPDLOG_INFO("Sent STX to Pi and Unity to start exchage.");

	while(1) {
		int nbbox, sl;

		if(sl = recvImage()) {
			if(sl == -2) {
				continue;
			} else
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


int Coordinator::init(const char *graph, const char *meta, float thresh) {
	ncs = new NCS(graph, meta, NCSNN_YOLOv2);
	ncs->initNN();
	ncs->nn.thresh = thresh;

	cv::namedWindow("original", cv::WINDOW_AUTOSIZE );// Create a window for display.
	cv::namedWindow("nn_input", cv::WINDOW_AUTOSIZE );// Create a window for display.
}


int Coordinator::run() {
	int ret;

	ret = INT32_MAX;
	while(1) {
		if(startServer()) exit(1);

		SPDLOG_INFO("Socket is waiting new incoming tcp connection at port {}...", port);
		ret = waitPiAndUy();
        if(ret < 0) continue;


		if(ncs->initDevice()) exit(1);

		ret = recvImages();
        if(ret >= 0) {
			break;
		}

		REPORTSPD(closeSockets() < 0, "Closing sockets error.")
		else
			SPDLOG_INFO("Sockets closed.");

	}

	cv::destroyAllWindows();

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