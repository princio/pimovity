/*
 * Gender_Age_Lbp
 *
 * Contributing Authors: Tome Vang <tome.vang@intel.com>, Neal Smith <neal.p.smith@intel.com>, Heather McCabe <heather.m.mccabe@intel.com>
 *
 *
 *
 */
#include "holooj.hpp"
#include "ncs.hpp"
#include "ny2.hpp"

#include <time.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <time.h>
#include <mvnc.h>


#define EXPIT(X)  (1 / (1 + exp(-(X))))

uint32_t numNCSConnected = 0;
ncStatus_t retCode;
struct ncDeviceHandle_t* dev_handle;
struct ncGraphHandle_t* graph_handle = NULL;
struct ncFifoHandle_t* fifo_in = NULL;
struct ncFifoHandle_t* fifo_out = NULL;
NCSerror ncs_errno;

int file2bytes(const char *filename, char **buf, unsigned int *n_bytes) {
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

int NCS::parse_meta_file() {
    SPDLOG_DEBUG("Starting «parse_meta_file»");
    char *buf;
    char *labels;
    char *anchors_text;
    char *in_size;
    char *out_size;
    char *pch;
    
    if(file2bytes(meta_path.c_str(), &buf, NULL)) return -1;

    pch = strtok( buf, "\"\"" );
    while (pch != NULL)
    {
        if(!strcmp(pch, "labels")) {
            pch = strtok (NULL, "[");
            labels = strtok (NULL, "]");
        } else
        if(!strcmp(pch, "anchors")) {
            pch = strtok (NULL, "[");
            anchors_text = strtok (NULL, "]");
        } else
        if(!strcmp(pch, "classes")) {
            pch = strtok (NULL, ",\"}");
            sscanf(pch, "%*[^0123456789]%d", &this->nn.nclasses);
        } else
        if(!strcmp(pch, "inp_size")) {
            pch = strtok (NULL, "[");
            in_size = strtok (NULL, "]");
            sscanf(in_size, "%d%*[^0123456789]%d%*[^0123456789]%d", &this->nn.in_w, &this->nn.in_h, &this->nn.in_c);
        } else
        if(!strcmp(pch, "out_size")) {
            pch = strtok (NULL, "[");
            out_size = strtok (NULL, "]");
            sscanf(out_size, "%d%*[^0123456789]%d%*[^0123456789]%d", &this->nn.out_w, &this->nn.out_h, &this->nn.out_z);
        } else
        if(!strcmp(pch, "num")) {
            pch = strtok (NULL, ",}\"");
            sscanf(pch, "%*[^0123456789]%d", &this->nn.nbbox);
        } else
        if(!strcmp(pch, "coords")) {
            pch = strtok (NULL, ",}\"");
            sscanf(pch, "%*[^0123456789]%d", &this->nn.ncoords);
        }
        pch = strtok (NULL, "\"");
    }

    this->nn.classes_buffer = (char*) calloc(strlen(labels) + 80, 1);
    this->nn.classes = (char**) calloc(this->nn.nclasses, sizeof(char*));
    pch = strtok(labels, "\"");
    int i = 0;
    int l = 0;
    while (pch != NULL)
    {
        if(strstr(pch, ",") == NULL) {
            strcpy(this->nn.classes_buffer + l, pch);
            this->nn.classes[i++] = this->nn.classes_buffer + l;
            l += strlen(pch);
            this->nn.classes_buffer[l] = '\0';
            ++l;
        }
        pch = strtok(NULL, "\"");
    }

    this->nn.nanchors = 1;
    pch = strpbrk (anchors_text, ",");
    while (pch != NULL)
    {
        ++this->nn.nanchors;
        pch = strpbrk (pch+1, ",");
    }

    this->nn.anchors = (float *) calloc(this->nn.nanchors, sizeof(float));
    pch = strtok(anchors_text, ",]");
    i = -1;
    while (pch != NULL)
    {
        sscanf(pch, "%f", &this->nn.anchors[++i]);
        pch = strtok(NULL, ",]");
    }
    printf("\n\n");


    printf("\nYOLOv2:\n");

    printf("\n\tclasses[%d]:\n\t\t", this->nn.nclasses);
    for(i=0; i < this->nn.nclasses; i++) printf("%s%s", this->nn.classes[i], (i > 0 && i % 10 == 0) ? "\n\t\t" : ", ");

    printf("\n\n\tanchors[%d]:\n\t\t", this->nn.nanchors);
    for(i=0; i < this->nn.nanchors; i++) printf("%f%s", this->nn.anchors[i], (i > 0 && i %  3 == 0) ? "\n\t\t" : ", ");

    printf("\n\n\t input: [ %5d, %5d, %5d ]\n\toutput: [ %5d, %5d, %5d ]\n\t nbbox: %d\n\t ncoords: %d\n",
            this->nn.in_w, this->nn.in_h, this->nn.in_c, this->nn.out_w, this->nn.out_h, this->nn.out_z, this->nn.nbbox, this->nn.ncoords);

    this->nn.nbbox_total = this->nn.out_w * this->nn.out_h * this->nn.nbbox;
    this->nn.input_size_byte = 4 * this->nn.in_w * this->nn.in_h * this->nn.in_c;
    this->nn.output_size_byte = 4 * this->nn.out_w * this->nn.out_h * this->nn.nbbox * (this->nn.ncoords + 1 + this->nn.nclasses);
    printf("\n\t input size: %d\n\toutput_size: %d\n\nnbbox total: %d\n\n",
            this->nn.input_size_byte, this->nn.output_size_byte, this->nn.nbbox_total);

    free(buf);

    printf("NN:\n");
    printf("\t%20s:\t%s\n", "name", this->nn.name);
    printf("\t%20s:\t%f\n", "thresh", this->nn.thresh);
    printf("\t%20s:\t[ %d, %d, %d]\n", "input", this->nn.in_w, this->nn.in_h, this->nn.in_c);
    printf("\t%20s:\t[ %d, %d ]\n", "image", this->nn.im_cols, this->nn.im_rows);
    printf("\t%20s:\t[ %d, %d, %d ]\n", "output", this->nn.out_w, this->nn.out_h, this->nn.out_z);
    printf("\t%20s:\t%d\n", "nbbox", this->nn.nbbox);
    printf("\t%20s:\t%d\n", "nbbox_total", this->nn.nbbox_total);
    printf("\t%20s:\t%d\n", "ncoords", this->nn.ncoords);
    printf("\t%20s:\t%d\n", "nclasses", this->nn.nclasses);
    printf("\t%20s:\t%d\n", "nanchors", this->nn.nanchors);
    printf("\t%20s:\t%d\n", "input_size_byte", this->nn.input_size_byte);
    printf("\t%20s:\t%d\n", "output_size_byte", this->nn.output_size_byte);

    SPDLOG_DEBUG("Done «parse_meta_file»");
    return 0;
}


int NCS::load_nn(){
    SPDLOG_DEBUG("Starting «load_nn»");
    int used_memory, total_memory;
    unsigned int graph_len = 0, data_length;
    char *buf;

    if(file2bytes(graph_path.c_str(), &buf, &graph_len)) return -1;

    retCode = ncGraphCreate(this->nn.name, &graph_handle);
    REPORTSPD(retCode, "NCS loading NN: «[{}] graph create error».", retCode);
    SPDLOG_DEBUG("graph created.");

    ncDeviceGetOption(dev_handle, NC_RO_DEVICE_CURRENT_MEMORY_USED, &used_memory, &data_length);
    ncDeviceGetOption(dev_handle, NC_RO_DEVICE_MEMORY_SIZE, &total_memory, &data_length);

    SPDLOG_DEBUG("used/total memory = {}/{}", used_memory, total_memory);

    retCode = ncGraphAllocateWithFifosEx(dev_handle, graph_handle, buf, graph_len,
                                        &fifo_in, NC_FIFO_HOST_WO, 2, NC_FIFO_FP32,
                                        &fifo_out, NC_FIFO_HOST_RO, 2, NC_FIFO_FP32);
    REPORTSPD(retCode, "NCS loading NN: «[{}] graph allocate error».", retCode);
    SPDLOG_DEBUG("graph allocated with fifos.");

    free(buf);

    SPDLOG_DEBUG("Done «load_nn»");
    return 0;
}

int NCS::init() {
    if(this->isInit) {
        SPDLOG_INFO("NCS already initialized.");
        return 0;
    }
    SPDLOG_DEBUG("Start.");

    if(this->parse_meta_file())
        return -1;

    switch(this->nn.type) {
        case NCSNN_YOLOv2:
            this->nn.type = NCSNN_YOLOv2;
            strcpy(this->nn.name, "yolov2");
        break;
    }

    this->nn.input = (float*) calloc(this->nn.input_size_byte, 1);
    this->nn.output = (float*) calloc(this->nn.output_size_byte, 1);

    retCode = ncDeviceCreate(0, &dev_handle);
    SPDLOG_DEBUG("Creating device...");
    REPORTSPD(retCode, "«device creating error» [{}].", retCode);
    SPDLOG_DEBUG("Device created.");

    SPDLOG_DEBUG("Opening device...");
    retCode = ncDeviceOpen(dev_handle);
    REPORTSPD(retCode, "«device opening error» [{}].", retCode);
    SPDLOG_DEBUG("Device opened.");

    this->nn.dets = (detection*) calloc(this->nn.nbbox_total, sizeof(detection));

    for(int i = this->nn.nbbox_total-1; i >= 0; --i) {
        this->nn.dets[i].prob = (float*) calloc(this->nn.nclasses, sizeof(float));
    }

    if(this->load_nn()) return -1;

    this->ny2 = new NY2(&this->nn);

    isInit = true;
    SPDLOG_DEBUG("End.");
    return 0;

}

NCS::NCS(const char *graph, const char *meta, NCSNNType nntype) {
    this->graph_path = graph;
    this->meta_path = meta;
    this->nn.type = nntype;
}


int NCS::inference_byte(unsigned char *image, int nbboxes_max) {
    SPDLOG_DEBUG("Start.");

	int i = 0;
    int letterbox = this->nn.in_c * this->nn.in_w * ((this->nn.in_h - this->nn.im_rows) >> 1);
    int l = this->nn.im_rows * this->nn.im_cols * this->nn.in_c;
	float *y = &this->nn.input[letterbox];
	while(i <= l-3) {
		y[i] = image[i + 2] / 255.; ++i;    // X[i] = imbuffer[i+2] / 255.; ++i;
		y[i] = image[i] / 255.;     ++i;    // X[i] = imbuffer[i] / 255.;   ++i;
		y[i] = image[i - 2] / 255.; ++i;    // X[i] = imbuffer[i-2] / 255.; ++i;
	}

#ifdef OPENCV
    IplImage *iplim = cvCreateImage(cvSize(this->nn.in_w, this->nn.in_h), IPL_DEPTH_32F, 3);
    memcpy(iplim->imageData, this->nn.input, this->nn.input_size_byte*4);
    cvShowImage("bibo2", iplim);
    cvUpdateWindow("bibo2");
    cvReleaseImage(&iplim);
    // cvWaitKey(0);
#endif

    int nbbox = this->inference(nbboxes_max);
    
    if(nbbox < 0) return -1;

    SPDLOG_DEBUG("End.");
    return nbbox;
}


int NCS::inference(int nbboxes_max) {
    SPDLOG_DEBUG("Start.");

    unsigned int length_bytes;
    unsigned int returned_opt_size;
    unsigned int in_size_bytes = this->nn.input_size_byte;
    unsigned int out_size_bytes = this->nn.output_size_byte;

    retCode = ncGraphQueueInferenceWithFifoElem(graph_handle, fifo_in, fifo_out, this->nn.input, &in_size_bytes, 0);
    REPORTSPD(retCode, "«NCS inference error.» [{}]", retCode);

    returned_opt_size = 4;
    retCode = ncFifoGetOption(fifo_out, NC_RO_FIFO_ELEMENT_DATA_SIZE, &length_bytes, &returned_opt_size);
    REPORTSPD(retCode, "«NCS get option error.» [{}]", retCode);

    REPORTSPD(length_bytes != out_size_bytes, "«Too few bytes read from fifo output ({} != {})»", length_bytes, out_size_bytes);

    retCode = ncFifoReadElem(fifo_out, this->nn.output, &length_bytes, NULL);
    REPORTSPD(retCode, "«NCS fifo read error.» [{}]", retCode);

    int nbbox = -1;
    switch(this->nn.type) {
        case NCSNN_YOLOv2:
            nbbox = this->ny2->inference(nbboxes_max);
            if(nbbox < 0) return -1;
        break;
    }
    
    SPDLOG_DEBUG("End.");
    return nbbox;
}

int NCS::destroy_movidius() {
    REPORTSPD(retCode = ncFifoDestroy(&fifo_in), "«Fifo In destroy error» [{}].", retCode);
    REPORTSPD(retCode = ncFifoDestroy(&fifo_out), "«Fifo Out destroy error» [{}].", retCode);
    REPORTSPD(retCode = ncGraphDestroy(&graph_handle), "Graph destroy error» [{}].", retCode);
    REPORTSPD(retCode = ncDeviceClose(dev_handle), "«Device closing error» [{}].", retCode);
    REPORTSPD(retCode = ncDeviceDestroy(&dev_handle), "«Device destroying error» [{}].", retCode);
    return 0;
}

NCS::~NCS() {
    for(int i = this->nn.nbbox_total-1; i >= 0; --i){
        free(this->nn.dets[i].prob);
    }
    free(this->nn.anchors);
    free(this->nn.classes);
    free(this->nn.classes_buffer);
    free(this->nn.output);
    free(this->nn.input);
    free(this->nn.dets);
}