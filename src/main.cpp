


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

	file2bytes2("/home/developer/dog.jpg", &im, &n);


	cv::Mat mat_im(1, n, CV_8UC3, im);
    cv::Mat mat_dst;
	auto mat = cv::imdecode(mat_im, cv::IMREAD_COLOR);
	cv::resize(mat_im, mat_dst, cv::Size(100, 200));
	std::cout << mat_dst.type() << std::endl;
	mat_dst.convertTo(mat_dst, CV_32FC3);
	std::cout << mat_dst.type() << std::endl;

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