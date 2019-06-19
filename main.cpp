


#include "holooj.h"
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



int main (int argc, char** argv) {
	spdlog::set_pattern("*** %^[%S.%f::%5l::%@:%!]%$: %v  ***");

	setvbuf(stdout, NULL, _IONBF, 0);

    const char *graph = "./yolov2/original/yolov2-tiny-original.graph";
    const char *meta = "./yolov2/original/yolov2-tiny-original.meta";
    std::string iface = "wlan0";
    unsigned int port = 8008;
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