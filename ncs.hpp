#ifndef __NCS_HPP__
#define __NCS_HPP__

#include "holooj.h"
#include "ny2.hpp"
#include <string>



typedef enum NCSerror { 
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


class NCS {
    private:
        NY2 *ny2;
        std::string graph_path;
        std::string meta_path;
        int parse_meta_file();
        int load_nn();
        Box get_region_box(float *x, int n, int index, int i, int j);
        void do_nms_sort();
        int nms_comparator(const void *pa, const void *pb);
        void correct_region_boxes();
    public:
        bool isInit = false;
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