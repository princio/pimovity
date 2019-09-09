
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


int Coordinator::connectToPi() {
	int ret, fd;
    int addrlen = sizeof(pi_addr);
	char caddr[20];
	byte buf[12];
	struct sockaddr_in pi_addr;
	
    pi_addr.sin_family = AF_INET;
    pi_addr.sin_port = htons(port);

    ret = inet_pton(AF_INET, "192.168.1.176", &pi_addr.sin_addr);
	REPORTSPD_ERRNO(0 > ret);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	REPORTSPD_ERRNO(0 > fd);

    ret = connect(fd, (struct sockaddr *) &pi_addr, sizeof(pi_addr));
	REPORTSPD_ERRNO(0 > ret);

	ret = 27;
	ret = send(fd, &ret, 4, 0);
	REPORTSPD(ret < 4, "Too few bytes ({}).", ret);

	ret = recv(fd, &buf, 12, 0);
	REPORTSPD(ret < 12, "Too few bytes ({}).", ret);

	ncs->setSizes(*((int*) &buf[4]), *((int*) &buf[8]));

	fd_pi = fd;

	return 0;
}

int Coordinator::waitUnity() {
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

	fd_uy = fd;

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
	int ndev = 2;
	if(only_pi) {
		ndev = 1;
	}
	for(int i = 0; i < ndev; i++) {
		fd = accept(fd_server, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
		inet_ntop(AF_INET, &(addr.sin_addr), caddr, INET_ADDRSTRLEN);
		REPORTSPD_ERRNO(0 > fd);

		r = recv(fd, &buf, 12, 0);
		REPORTSPD(r < 4, "Too few bytes ({}).", r);

		id = *((int*) buf);
		if(id == 27) {
			if(r == 12) {
				fd_pi = fd;

				initCalibration(*((int*) &buf[4]), *((int*) &buf[8]));
	
				ncs->setSizes(roi.width, roi.height);
				
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


	REPORTSPD(fd_pi == -1, "Pi device not connected.");

	if(!only_pi) {
		REPORTSPD(fd_uy == -1, "Unity device not connected.");
		memcpy(buf, &STX, 4);
		memcpy(buf + 4, &roi.width, 4);
		memcpy(buf + 8, &roi.height, 4);
		r = send(fd_uy, buf, 12, 0);
		if(0 > r) return -1;

		ufds[1].fd = fd_uy;
		ufds[1].events = POLLIN | POLLOUT; // check for normal or out-of-band
	}

	ufds[0].fd = fd_pi;
	ufds[0].events = POLLIN | POLLOUT; // check for normal or out-of-band


	SPDLOG_TRACE("End.");
	return 0;
}


int Coordinator::initCalibration(int w, int h) {
	SPDLOG_TRACE("Start.");
	std::string filename;
	if(w == 1640 && h == 1232)  filename = "../data/xml/calib_2k.xml";
	else
	if(w == 3280 && h == 2464)  filename = "../data/xml/calib_4k.xml";
	else
	if(w == 1920 && h == 1080)  filename = "../data/xml/calib_1080.xml";
	else return -1;

	cv::Size size (w, h);


	jpeg_buffer.reserve(rpacket_buffer_size);

	cv::FileStorage fs = cv::FileStorage(filename, cv::FileStorage::READ);
	fs["camera_matrix"] >> cameraMatrix;
	fs["distortion_coefficients"] >> distCoeffs;

	
	cv::initUndistortRectifyMap(
		cameraMatrix, distCoeffs, cv::Mat(),
		cv::getOptimalNewCameraMatrix(cameraMatrix, distCoeffs, size, 1, size, &roi), size,
		CV_32FC1, map1, map2);


	printf("\n- ROI: (%d, %d, %d, %d)\n", roi.x, roi.y, roi.width, roi.height);

	SPDLOG_TRACE("Stop.");
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


int Coordinator::drawBbox(rgb_pixel *im, Box b, rgb_pixel color, int w, int h) {
	int wh, left_col, right_col, top_row, bot_row,
		left_overlap, top_overlap, right_overlap, bottom_overlap, thickness, 
		corner_top_left, corner_bot_left, corner_top_right, corner_bot_right,
		top_left_pixel, bot_left_pixel, box_width_pixel;

	REPORTSPD(w <= 0, "Width ({}) <= 0.", w);
	REPORTSPD(h <= 0, "Height ({}) <= 0.", h);

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


int Coordinator::undistortImage() {
	return 0;
}

int Coordinator::elaborateImage() {
	SPDLOG_TRACE("Start image elaboration ({} bytes).", rpacket->l);


    SPDLOG_DEBUG("images={}", counter);

	cv::Mat mat_jpeg(rpacket->l, 1, CV_8UC3, rpacket->image);
	mat_raw = cv::imdecode(mat_jpeg, cv::IMREAD_COLOR);
    cv::remap(mat_raw, mat_raw_calibrated, map1, map2, cv::INTER_LINEAR);
	mat_raw_cropped = mat_raw_calibrated(roi);
	mat_raw_final = mat_raw_cropped.clone();

	cv::putText(mat_raw_final, std::to_string(counter++), cv::Point(30,30), 
    		cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(200,200,250), 1, 4);


	cv::imencode(".jpg", mat_raw_final, jpeg_buffer);
	cv::imwrite("../phs/im.jpg", mat_raw_final);

	/** only with NCS */
	int expected = NCS_NOT_DOING;
	if(inference_atomic.compare_exchange_strong(expected, NCS_NOT_DOING)) {
		cv::resize(mat_raw_final, mat_raw_resized, cv::Size(ncs->nn.im_resized_cols, ncs->nn.im_resized_rows));
		inference_atomic = NCS_TODO;
		SPDLOG_DEBUG("Atomic: NCS_NOT_DOING -> NCS_TODO.");
	}


	SPDLOG_TRACE("Stop.");
	return 0;
}


int Coordinator::recvImage() {
	int rl = recv(fd_pi, (byte*) rpacket, 8, MSG_WAITALL);
	REPORTSPD(rpacket->stx != STX, "Wrong STX ({} instead of {}).", rpacket->stx, STX);

	if(rpacket_buffer_size < rpacket->l) {
		printf("Image too large: %ud > %d. Skipped.", rpacket_buffer_size, rpacket->l);
		return -2;
	}

	rl = recv(fd_pi, (byte*) rpacket->image, rpacket->l, MSG_WAITALL);
	REPORTSPD(rpacket->stx != STX, "Too few bytes received ({} instead of {}).", rl, rpacket->l);

	return 0;
}

int Coordinator::elaborate_ncs() {
	int expected;
	while(1) {
		expected = NCS_TODO;
		if(inference_atomic.compare_exchange_strong(expected, NCS_DOING)) {
			SPDLOG_DEBUG("Atomic: NCS_TODO -> NCS_DOING.");
			nbboxes = ncs->inference_byte(mat_raw_resized.data, 20);
			SPDLOG_DEBUG("Inference done.");

			
			++imcounter;
			for(int i = nbboxes-1; i >= 0; --i) {
				rgb_pixel color;
				byte c1 = 250 * i / 3;
				byte c2 = 250 * i / 3;
				byte c3 = 250 * i / 3;
				SPDLOG_INFO("\n{}:\t({:7.6f}, {:7.6f}, {:7.6f}, {:7.6f}), o={:7.6f}, p={:7.6f}:\t\t{}",
					imcounter, ncs->nn.bboxes[i].box.x, ncs->nn.bboxes[i].box.y, ncs->nn.bboxes[i].box.w, ncs->nn.bboxes[i].box.h, ncs->nn.bboxes[i].objectness, ncs->nn.bboxes[i].prob, ncs->nn.classes[ncs->nn.bboxes[i].cindex]);


				drawBbox((rgb_pixel*) mat_raw_final.data, ncs->nn.bboxes[i].box, color, ncs->nn.im_or_cols, ncs->nn.im_or_rows);
			}

			if(nbboxes > 0) {
				std::stringstream fname;
				fname << "/home/developer/Desktop/phs_w_bboxes/im_" << imcounter << ".jpg";
				try {
					cv::imwrite(fname.str(), mat_raw_final);
				}
				catch (std::runtime_error& ex) {
					fprintf(stderr, "Exception converting image to PNG format: %s\n", ex.what());
					return 1;
				}
			}
			
			inference_atomic = NCS_DONE;
			SPDLOG_DEBUG("Atomic: NCS_DOING -> NCS_DONE.");
		}
		else {
			//SPDLOG_DEBUG("Atomic: NCS_NOT_DOING.");
			usleep(100);
		}
	}
}

int Coordinator::recvImagesLoop() {
	int consecutive_wrong_packets = 0;
	int stxs[2] = { STX, STX };
	rpacket = (RecvPacket *) calloc(sizeof(RecvPacket) + rpacket_buffer_size, 1);
	rpacket->type = 0;

	memset(&spacket, 0, sizeof(SendPacket));
	spacket.stx = 27692;
	spacket.l = sizeof(spacket.bboxes);
	spacket.n = 0;
	spacket.type = 1;
	if(disable_ncs) {
		spacket.bboxes[0].box.x = 0.5;
		spacket.bboxes[0].box.y = 0.6;
		spacket.bboxes[0].box.w = 0.7;
		spacket.bboxes[0].box.h = 0.8;
		spacket.bboxes[0].objectness = 0.1;
		spacket.bboxes[0].prob = 0.2;
		spacket.bboxes[0].cindex = 21;
	}


	ncs->nn.bboxes = spacket.bboxes;
	

	if (0 > send(fd_pi, stxs, 8, 0)) return -1;


	inference_atomic = NCS_NOT_DOING;

	while(1) {
		int sl;
		REPORTSPD_ERRNO(0 > ioctl(fd_pi, FIONREAD, &sl));
		SPDLOG_TRACE("Bytes available: {}\n", sl);
		
		if(sl = recvImage()) {
			if(sl == -2) {
				continue;
			} else
			if(++consecutive_wrong_packets == 10) {
				SPDLOG_WARN("Wrong incoming packets from Pi for 10 consecutive times. Stop loop.");

				sl = send(fd_pi, (void*) "\0\0\0\0", 16, 0);
				break;
			} else {
				int bytes_available = -1;
				REPORTSPD_ERRNO(0 > ioctl(fd_pi, FIONREAD, &bytes_available));
				recv(fd_pi, (byte*) rpacket, bytes_available, 0);
				SPDLOG_WARN("Wrong incoming packet from Pi: skip to next while iteration (consecutives = {}, {}).", consecutive_wrong_packets, bytes_available);
				continue;
			}
		}
		consecutive_wrong_packets = 0;


		if(0 > elaborateImage()) return -1;
		sl = send(fd_pi, stxs, 8, 0);
		if(!only_pi) {
			rpacket->stx = 27692;
			rpacket->l = jpeg_buffer.size();
			rpacket->type = 0;
			sl = send(fd_uy, rpacket, 12, 0);
			sl = send(fd_uy, jpeg_buffer.data(), jpeg_buffer.size(), 0);
		}

		spacket.type = 1;
		int expected = NCS_DONE;
		if(disable_ncs) {
			sl = send(fd_uy, &spacket, sizeof(SendPacket), 0);
			SPDLOG_ERROR("Sent {} bytes to unity as bboxes.", sl);
		} else
		if(inference_atomic.compare_exchange_weak(expected, NCS_DONE)) {
			//sl = send(fd_uy, (void*) &spacket, sizeof(SendPacket), 0);
			if(nbboxes > 0) {
				spacket.n = nbboxes;
				sl = send(fd_uy, &spacket, sizeof(SendPacket), 0);
				SPDLOG_ERROR("Sent {} bytes to unity as bboxes.", sl);
			}
			inference_atomic = NCS_NOT_DOING;
			SPDLOG_DEBUG("Atomic: NCS_DONE -> NCS_NOT_DOING.");
		}
		

		SPDLOG_DEBUG("Jpeg vector size: {}.", jpeg_buffer.size());

	}
	
}

int Coordinator::recvImages() {
	SPDLOG_TRACE("Start.");
	auto images_loop = new std::thread(&Coordinator::recvImagesLoop, this);
	if(!disable_ncs) {
		inference_thread = new std::thread(&Coordinator::elaborate_ncs, this);
		inference_thread->join();
	}

	images_loop->join();

	free(rpacket);

	SPDLOG_TRACE("END.");
	return 0;
}


int Coordinator::init(const char *graph, const char *meta, float thresh, bool only_pi, bool disable_ncs) {

	this->only_pi = only_pi;
	this->disable_ncs = disable_ncs;

	ncs = new NCS(graph, meta, NCSNN_YOLOv2);
	ncs->initNN();
	ncs->nn.thresh = thresh;
}


int Coordinator::run(unsigned int port) {
	int ret;

	this->port = port;

	ret = INT32_MAX;


	SPDLOG_INFO("Starting server...");
	if(startServer()) return -1;
	SPDLOG_INFO("Waiting for Pi and Unity...");
	ret = waitPiAndUy();
	if(ret < 0) return -1;
	
	if(disable_ncs == false && ncs->initDevice()) exit(1);


	ret = recvImages();
	if(ret < 0) {
		return -1;
	}

	REPORTSPD(closeSockets() < 0, "Closing sockets error.")
	else
		SPDLOG_INFO("Sockets closed.");


	cv::destroyAllWindows();

	ncs->~NCS();

	return 0;
}


int Coordinator::closeSockets() {
	int r;
	
	r = close(fd_pi);
	REPORTSPD(r < 0, "Error during closing Pi socket.");

	if(!only_pi) {
		r = close(fd_uy);
		REPORTSPD(r < 0, "Error during closing Unity socket.");
	}

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