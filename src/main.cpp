


#include "holooj.hpp"
#include "coordinator.hpp"

#include <stdio.h>
#include <string.h>
#include <turbojpeg.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>


#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#ifdef DEBUG
#define CHECK(i) if(i < 0 || i > wh) printf("Overflow!");
#else
#define CHECK(i) void(0);
#endif

int drawBbox(cv::Mat mat, Box b, rgb_pixel color) {
	rgb_pixel *im = (rgb_pixel*) mat.data;

    int w = 768;
    int h = 576;
	int wh = w*h;


    int left_col  = (b.x-b.w/2.)*w;
    int right_col = (b.x+b.w/2.)*w;
    int top_row   = (b.y-b.h/2.)*h;
    int bot_row   = (b.y+b.h/2.)*h;



	int left_overlap = 0;
	int top_overlap = 0;
	int right_overlap = 0;
	int bottom_overlap = 0;
    int thickness = 5; //border width in pixel minus 1
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
	

	/**
	 * horizontal row
	 */

	int corner_top_left  = w * top_row + left_col;
	int corner_bot_left  = w * bot_row + left_col;
	int corner_top_right = w * top_row + right_col + thickness;
	int corner_bot_right = w * bot_row + right_col + thickness;
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
	int top_left_pixel = w * top_row + left_col;
	int bot_left_pixel = w * (bot_row + thickness) + left_col;
	int box_width_pixel = right_col - left_col + thickness*2 - left_overlap - right_overlap;
	for(int r = 0; r < thickness; r++) {
		for(int p = 0; p < box_width_pixel; p++) {
			if(r < top_overlap || p/thickness % 2 == 0) {
				im[top_left_pixel + p] = color;
				CHECK(top_left_pixel + p);
			}
			if(r >= bottom_overlap || p/thickness % 2 == 0) {
				im[bot_left_pixel + p] = color;
				CHECK(bot_left_pixel + p);
			}
		}
		top_left_pixel += w;
		bot_left_pixel += w;
	}

	top_row += thickness+1;
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
				CHECK(r*w + left_col + b);
			}
			if(b >= right_overlap || (r - top_row)/thickness % 2 == 0) {
				im[r*w + right_col + b] = color;
				CHECK(r*w + right_col + b);
			}
		}
    }
}



int file2bytes2(const char *filename, char **buf, unsigned int *n_bytes) {
    SPDLOG_DEBUG("Started for «{}»", filename);

    FILE *f = fopen(filename, "r");


    REPORTSPD(f == NULL, "Failed to open file “{}”: «[{}] {}»", filename, errno, strerror(errno));

    unsigned int to_read, has_read;
    fseek(f, 0, SEEK_END);
    to_read = ftell(f);
    rewind(f);

    if(!(*buf = (char*) malloc(to_read))) {
        fclose(f);
        REPORTSPD(1, "«couldn't allocate buffer».");
    }

    has_read = fread(*buf, 1, to_read, f);

    if(has_read != to_read) {
        fclose(f);
        free(*buf);
        *buf = NULL;
        REPORTSPD(1, "«read wrong bytes number ({} != {})».", has_read, to_read);
    }

    REPORTSPD(fclose(f) != 0, "«[{}] {}».", errno, strerror(errno));

    if(n_bytes) *n_bytes = has_read;
    SPDLOG_DEBUG("Done «file2bytes» ({} bytes).", has_read);
    return 0;
}


/*
+--------+----+----+----+----+------+------+------+------+
|        | C1 | C2 | C3 | C4 | C(5) | C(6) | C(7) | C(8) |
+--------+----+----+----+----+------+------+------+------+
| CV_8U  |  0 |  8 | 16 | 24 |   32 |   40 |   48 |   56 |
| CV_8S  |  1 |  9 | 17 | 25 |   33 |   41 |   49 |   57 |
| CV_16U |  2 | 10 | 18 | 26 |   34 |   42 |   50 |   58 |
| CV_16S |  3 | 11 | 19 | 27 |   35 |   43 |   51 |   59 |
| CV_32S |  4 | 12 | 20 | 28 |   36 |   44 |   52 |   60 |
| CV_32F |  5 | 13 | 21 | 29 |   37 |   45 |   53 |   61 |
| CV_64F |  6 | 14 | 22 | 30 |   38 |   46 |   54 |   62 |
+--------+----+----+----+----+------+------+------+------+
 */
int main (int argc, char** argv) {

	setvbuf(stdout, NULL, _IONBF, 0);

	spdlog::set_pattern("*** %^[%S.%f::%5l::%@:%!]%$: %v  ***");

	char *im;
	unsigned int n;

	file2bytes2("/home/developer/dog_or.jpg", &im, &n);

 	cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );// Create a window for display.
	cv::Mat mat_im(1, n, CV_8UC3, im);
    cv::Mat mat_dst;
	auto mat = cv::imdecode(mat_im, cv::IMREAD_COLOR);
	// cv::resize(mat_im, mat_dst, cv::Size(100, 200));
	// std::cout << mat_dst.type() << std::endl;
	// mat_dst.convertTo(mat_dst, CV_32FC3);
	// std::cout << mat_dst.type() << std::endl;
	Box b;
	rgb_pixel color = { 0, 0, 250 };

	int r;
	srand(time(0)); 

	b.w = 1;
	b.h = 1;
	b.x = 0.0;
	b.y = .0;

	color.r = rand() % 255;
	color.g = rand() % 255;
	color.b = rand() % 255;

	drawBbox(mat, b, color);


	// for (float i = 0; i <= 0; i+=0.2) {
	// 	for (float j = 0; j <= 0; j+=0.2) {
	// 		for (float k = 0; k <= 0; k+=0.05) {
	// 			for (float l = 0; l <= 0; l+=0.05) {
	// 				b.w = i;
	// 				b.h = j;
	// 				b.x = k;
	// 				b.y = l;

	// 				color.r = rand() % 255;
	// 				color.g = rand() % 255;
	// 				color.b = rand() % 255;

	// 				drawBbox((rgb_pixel*) mat.data, b, color);

	// 				printf("%f %f %f %f\n", i,j,k,l);
	// 			}
	// 		}
	// 	}
	// }

 	cv::imshow( "Display window", mat );
	cv::waitKey(0);               


    const char *graph = "../data/yolov2/original/yolov2-tiny-original.graph";
    const char *meta = "../data/yolov2/original/yolov2-tiny-original.meta";
    std::string iface = "wlan0";
    unsigned int port = 8000;
	float thresh = 0.5;

	// int r;
	// srand(time(0)); 
	// r = rand() % 1000;
	// port = r + 50000;

	// port = 56789;

	if(argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printf("HoloOj for Raspberry.\n\t--graph\t\t\tthe path to the graph file.\n\t--meta\t\t\tthe path to the meta file.\n\t--iface\t\t\tthe network interface to use.\n\t--help, -h\t\tthis help.\n");
		exit(0);
	}
	auto log_level = spdlog::level::debug;
	if(argc >= 3) {
		for(int i = 1; i < argc; i++) {
			if(!strcmp(argv[i], "--iface")) {
				iface = argv[i+1];
			}
			if(!strcmp(argv[i], "--graph")) {
				graph = argv[i+1];
			}
			if(!strcmp(argv[i], "--meta")) {
				meta  = argv[i+1];
			}
			if(!strcmp(argv[i], "--port")) {
				port  = atoi(argv[i+1]);
			}
			if(!strcmp(argv[i], "--thresh")) {
				thresh  = atof(argv[i+1]);
			}
			if(!strcmp(argv[i], "-v")) {
				int l = atoi(argv[i+1]);
				if(l < 0 || l > 6) log_level = spdlog::level::off;
				else {
					log_level = (spdlog::level::level_enum) l;
				}
			}
		}
	}
	spdlog::set_level(log_level);

	printf("LOG LEVEL = %d", log_level);


    printf("HoloOj for Raspberry:\n\t%6s = %s\n\t%6s = %u\n\t%6s = %s\n\t%6s = %s\t%6s = %s\n",
	"iface", iface.c_str(), "port", port, "graph", graph, "meta", meta, "thresh", meta);


	Coordinator coo(iface.c_str(), port);


    coo.run(graph, meta, thresh);

}