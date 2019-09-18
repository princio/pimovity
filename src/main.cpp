


#include "holooj.hpp"
#include "holocoo.hpp"

#include <stdio.h>
#include <string.h>
#include <turbojpeg.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>



#ifdef DEBUG
#define CHECK(i) if(i < 0 || i > wh) printf("Overflow!");
#else
#define CHECK(i) void(0);
#endif


const char   *graph  =  "../data/yolov2/original/yolov2-tiny-original.graph";
const char    *meta  =  "../data/yolov2/original/yolov2-tiny-original.meta";
std::string   iface  =  "wlan0";
unsigned int   port  =  8001;
float        thresh  =  0.7;
bool        only_pi  =  false;
bool    disable_ncs  =  false;


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
 			
bool file_exists (const char * name) {
    if (FILE *file = fopen(name, "r")) {
	fclose(file);
        return true;
    } else {
	printf("%s:\t%s", name, strerror(errno));
        return false;
    }   
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

	printf("\n\n\t--%-10s\tChange to convert received photo to rgb color space instead that default bgr.", "rgb");
	printf("\n\t\t\tDefault is false.");

	printf("\n\n\t--%-10s\tSave received and decoded photo into ./phs folder as jpeg.", "save-photo");
	printf("\n\t\t\tDefault is false.");

	printf("\n\n\t--%-10s\tSet verbose output. 0=No, 1=critical, 2=error, 3=warning, 4=info, 5=debug, 6=trace.", "verbose");
	printf("\n\t\t\tDefault is 3.\n\n");
}

int main (int argc, char** argv) {

	setvbuf(stdout, NULL, _IONBF, 0);

	spdlog::set_pattern("*** %^[%f::%5l::%s:%#:%!]%$: %v  ***");

	char graph[100];
	char meta[100];

	strcpy(graph, "../data/yolov2/original/yolov2-tiny-original.graph");
	strcpy(meta, "../data/yolov2/original/yolov2-tiny-original.meta");

    std::string iface = "wlan0";
    unsigned int port = 8000;
	float thresh = 0.5;
	bool rgb = false;
	bool savePhoto = false;


	if(argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printHelp();
		exit(0);
	}
	auto log_level = spdlog::level::info;
	if(argc >= 3) {
		for(int i = 1; i < argc; i++) {
			if(!strcmp(argv[i], "--ale")) {
				char *ale = argv[i+1];
				
				strcpy(graph, "../data/yolov2/ale/yolov2-tiny-ale-"); 
				strcat(graph, ale);
				strcat(graph, ".graph");
				strcpy(meta,  "../data/yolov2/ale/yolov2-tiny-ale-");
				strcat(meta,  ale);
				strcat(meta,  ".meta");
				
				if(!file_exists(graph)){
					SPDLOG_WARN("No graph file found ({}): exit.", graph);
					exit(1);
				}
				 if(!file_exists(graph))  {
					SPDLOG_WARN("No meta file found ({}): exit.", meta);
					exit(1);
				}
				
			} else
			if(!strcmp(argv[i], "--iface")) {
				iface = argv[i+1];
			} else
			if(!strcmp(argv[i], "--graph")) {
				strcpy(graph, argv[i+1]);
			} else
			if(!strcmp(argv[i], "--meta")) {
				strcpy(meta, argv[i+1]);
			} else
			if(!strcmp(argv[i], "--port")) {
				port  = atoi(argv[i+1]);
			} else
			if(!strcmp(argv[i], "--thresh")) {
				thresh  = atof(argv[i+1]);
			} else
			if(!strcmp(argv[i], "--verbose")) {
				int l = 6 - atoi(argv[i+1]);
				if(l < 0 || l > 6) log_level = spdlog::level::off;
				else {
					log_level = (spdlog::level::level_enum) l;
				}
			} else
			if(!strcmp(argv[i], "--rgb")) {
				rgb = true;
			} else
			if(!strcmp(argv[i], "--save-photo")) {
				savePhoto = true;
			}
		}
	}
	spdlog::set_level(log_level);


    printf("HoloOj for Raspberry:");
	printf("\n%18s:   %s", ".graph file path", graph);
	printf("\n%18s:   %s", ".meta file path", meta);
	printf("\n%18s:   %g", "thresh", thresh);
	printf("\n%18s:   %s", "iface", iface.c_str());
	printf("\n%18s:   %d\n\n", "port", port);


	unsigned int nb;
	HoloCoo coo(iface.c_str(), port);

    coo.init(graph, meta, thresh);
    
    coo.rgb = rgb;
    coo.savePhoto = savePhoto;

    coo.run(port);

	exit(0);

	char *buf;

    coo.init(graph, meta, thresh);


	file2bytes2("/home/developer/dog_or.jpg", &buf, &coo.rpacket_buffer_size);

	coo.rpacket = (RecvPacket *) calloc(sizeof(RecvPacket) + coo.rpacket_buffer_size, 1);
	memcpy(coo.rpacket->image, buf, coo.rpacket_buffer_size);

	if(coo.ncs->initDevice()) exit(1);

	coo.ncs->setSizes(768, 576);
	// coo.ncs->setSizes(416, 416);

	coo.rpacket->l = coo.rpacket_buffer_size;
	coo.ncs->nn.bboxes = (bbox*) calloc(5, sizeof(bbox));

    coo.run(port);

}
