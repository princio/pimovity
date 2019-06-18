


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
    
	setvbuf(stdout, NULL, _IONBF, 0);


    char *graph = "./yolov2/original/yolov2-tiny-original.graph";
    char *meta = "./yolov2/original/yolov2-tiny-original.meta";
    char *iface = "wlan0";
    unsigned int port = 56789;
	float thresh = 0.5;


	int r;
	srand(time(0)); 
	r = rand() % 1000;
	port = r + 50000;

	port = 56789;

	if(argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printf("HoloOj for Raspberry.\n\t--graph\t\t\tthe path to the graph file.\n\t--meta\t\t\tthe path to the meta file.\n\t--iface\t\t\tthe network interface to use.\n\t--help, -h\t\tthis help.\n");
		exit(0);
	}

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
		}
	}


    printf("HoloOj for Raspberry:\n\t%6s = %s\n\t%6s = %u\n\t%6s = %s\n\t%6s = %s\t%6s = %s\n",
	"iface", iface, "port", port, "graph", graph, "meta", meta, "thresh", meta);


	Coordinator coo(iface, port);


    coo.run(graph, meta, thresh);

}