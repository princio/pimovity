#ifndef __NY2_HPP__
#define __NY2_HPP__

#include "holooj.hpp"

class NY2 {
    void correct_region_boxes();
    void do_nms_sort();
    Box get_region_box(float *x, int n, int index, int i, int j);
    int get_bboxes(int nbboxes_max);
    nnet *nn;
    public:
        NY2(nnet *nn);
        int inference_byte(unsigned char *image, int nbboxes_max);
        int inference(int nbboxes_max);
};

#endif