#ifndef __NCS_HPP__
#define __NCS_HPP__

#include "ny2.hpp"
#include <string>


typedef enum {
	NCSNN_YOLOv2
} NCSNNType;

typedef enum { 
    NCSDevCreateError,
    NCSDevOpenError,
	NCSGraphCreateError,
	NCSGraphAllocateError,
	NCSInferenceError,
	NCSGetOptError,
	NCSFifoReadError,
	NCSDestroyError,
    NCSReadGraphFileError,
    NCSTooFewBytesError,
    NCSParseError
} NCSerror;


typedef struct box{
    float x, y, w, h;
} box;

typedef struct bbox{
    box box;
    float objectness;
    float prob;
    int cindex;
} bbox;

typedef struct detection{
    box bbox;
    int classes;
    float *prob;
    float objectness;
    int sort_class;
} detection;

typedef struct nnet {
	char name[10];
    NCSNNType type;
    float thresh;
    int in_w;
    int in_h;
    int in_c;
    int im_cols;
    int im_rows;
    int out_w;
    int out_h;
    int out_z;
    int nbbox;
    int nbbox_total;
    int ncoords;
    int nclasses;
    int nanchors;
    int input_size_byte;
    int output_size_byte;
    char *classes_buffer;
    char **classes;
    float *anchors;
    float *output;
    float *input;
    detection *dets;
    bbox *bboxes;
} nnet;

class NCS {
    private:
        NY2 *ny2;
        std::string graph_path = "";
        std::string meta_path = "";
        int parse_meta_file();
        int load_nn();
        box get_region_box(float *x, int n, int index, int i, int j);
        void do_nms_sort();
        int nms_comparator(const void *pa, const void *pb);
        void correct_region_boxes();
    public:
        nnet nn;
        detection *dets;
        bbox *bboxes;
        NCS(const char*, const char*, NCSNNType);
        ~NCS();
        int init();
        int inference_byte(unsigned char *image, int nbboxes_max);
        int inference(int nbboxes_max);
        int destroy_movidius();
};

#endif //__NCS_HPP__