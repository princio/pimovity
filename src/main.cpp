


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
// #include <opencv2/imgcodecs.hpp>
// #include <opencv2/imgproc.hpp>
// #include <opencv2/core/core.hpp>
// #include <opencv2/highgui/highgui.hpp>

#ifdef DEBUG
#define CHECK(i) if(i < 0 || i > wh) printf("Overflow!");
#else
#define CHECK(i) void(0);
#endif


const char *graph = "../data/yolov2/original/yolov2-tiny-original.graph";
const char *meta = "../data/yolov2/original/yolov2-tiny-original.meta";
std::string iface = "wlan0";
unsigned int port = 8001;
float thresh = 0.7;
bool only_pi = false;
bool disable_ncs = false;



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
	printf("##%p\n", (void*) *buf);
    return 0;
}

void printHelp() {

	char temp[200];
	getcwd(temp, sizeof(temp));

	printf("\n\t--%-10s\tPort to which the socket is binded.", "port");
	printf("\n\t\t\tDefault is «%d».", port);

	printf("\n\t--%-10s\tSet NN threshold..", "thresh");
	printf("\n\t\t\tDefault is «%g».", thresh);

	printf("\n\n\t--%-10s\twlan0 or eth0.\n\t\t\tDefault is «%s».", "iface", iface.c_str());

	printf("\n\n\t--%-10s\tThe path to the graph. Current working directory is «%s».", "graph", temp);
	printf("\n\t\t\tDefault is «%s».", graph);

	printf("\n\n\t--%-10s\tThe path to the meta file. Current working directory is «%s».", "meta", temp);
	printf("\n\t\t\tDefault is «%s».", meta);

	printf("\n\n\t--%-10s\tThe program will connect and exchange data only with Raspberry.", "only-pi");
	printf("\n\t\t\tDefault is false.");

	printf("\n\n\t--%-10s\tDisable all NCS operations. It sends empty packets to Unity.", "disable_ncs");
	printf("\n\t\t\tDefault is false.");

	printf("\n\n\t--%-10s\tSet verbose output. 0=No, 1=critical, 2=error, 3=warning, 4=info, 5=debug, 6=trace.", "verbose");
	printf("\n\t\t\tDefault is 3.\n\n");
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

	// bbox b;
	// b.box.x = 0.482650; b.box.y = 0.527183; b.box.w = 0.175509; b.box.h = 0.401476;

	// auto m = cv::imread("/home/developer/Desktop/phs_w_bboxes/im_122.jpg", 1);

	// cv::Rect roi (20, 60, 1000, 800);
	// auto m_cropped = m(roi);
	// cv::Mat m_cropped_2 = m_cropped.clone();

	// SPDLOG_ERROR("{}", m.step);
	// SPDLOG_ERROR("{}", m_cropped.step);
	// m_cropped.step = 1000*3;
	// SPDLOG_ERROR("{}", m_cropped.step);
	// SPDLOG_ERROR("{}", m_cropped_2.step);
	// rgb_pixel color;
	// color.g=250;
	// Coordinator::drawBbox((rgb_pixel*) m_cropped.data, b.box, color, 1000, 800);
	// Coordinator::drawBbox((rgb_pixel*) m_cropped_2.data, b.box, color, 1000, 800);

    // cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );// Create a window for display.
    // cv::imshow( "Display window", m );     
    // cv::waitKey(0);  
    // cv::imshow( "Display window", m_cropped );
    // cv::waitKey(0);  
    // cv::imshow( "Display window", m_cropped_2 );
    // cv::waitKey(0);  

	setvbuf(stdout, NULL, _IONBF, 0);

	spdlog::set_pattern("%^[%5l][%S.%f::%s:%#:%!]%$: %v ");



	if(argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printHelp();
		exit(0);
	}
	auto log_level = spdlog::level::warn;
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
			if(!strcmp(argv[i], "--only-pi")) {
				only_pi  = true;
			}
			if(!strcmp(argv[i], "--disable-ncs")) {
				disable_ncs  = true;
			}
			if(!strcmp(argv[i], "--port")) {
				port  = atoi(argv[i+1]);
			}
			if(!strcmp(argv[i], "--thresh")) {
				thresh  = atof(argv[i+1]);
			}
			if(!strcmp(argv[i], "--verbose")) {
				int l = 6 - atoi(argv[i+1]);
				if(l < 0 || l > 6) log_level = spdlog::level::off;
				else {
					log_level = (spdlog::level::level_enum) l;
				}
			}
		}
	}
	spdlog::set_level(log_level);


    printf("Pimovity:\n\t%6s = %s\n\t%6s = %u\n\t%6s = %s\n\t%6s = %s\n\t%6s = %g\n",
	"iface", iface.c_str(), "port", port, "graph", graph, "meta", meta, "thresh", thresh);


	unsigned int nb;
	Coordinator coo(iface.c_str(), port);

    coo.init(graph, meta, thresh, only_pi, disable_ncs);

    coo.run(port);

	exit(0);

	char *buf;



	file2bytes2("/home/developer/dog_or.jpg", &buf, &coo.rpacket_buffer_size);

	coo.rpacket = (RecvPacket *) calloc(sizeof(RecvPacket) + coo.rpacket_buffer_size, 1);
	memcpy(coo.rpacket->image, buf, coo.rpacket_buffer_size);

	if(coo.ncs->initDevice()) exit(1);

	coo.ncs->setSizes(768, 576);
	// coo.ncs->setSizes(416, 416);

	coo.rpacket->l = coo.rpacket_buffer_size;
	coo.ncs->nn.bboxes = (bbox*) calloc(5, sizeof(bbox));
	coo.elaborate();

    coo.run(port);

}