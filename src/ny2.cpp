

#include "ny2.hpp"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef OPENCV
#include <cv.h>
#include <highgui.h>
#endif

#define EXPIT(X)  (1 / (1 + exp(-(X))))


nnet *nn;

float overlap(float x1, float w1, float x2, float w2)
{
    float l1 = x1 - w1/2;
    float l2 = x2 - w2/2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1/2;
    float r2 = x2 + w2/2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

float box_intersection(Box a, Box b)
{
    float w = overlap(a.x, a.w, b.x, b.w);
    float h = overlap(a.y, a.h, b.y, b.h);
    if(w < 0 || h < 0) return 0;
    float area = w*h;
    return area;
}

float box_union(Box a, Box b)
{
    float i = box_intersection(a, b);
    float u = a.w*a.h + b.w*b.h - i;
    return u;
}

float box_iou(Box a, Box b)
{
    return box_intersection(a, b)/box_union(a, b);
}

int nms_comparator(const void *pa, const void *pb)
{
    detection a = *(detection *)pa;
    detection b = *(detection *)pb;
    float diff = 0;
    if(b.sort_class >= 0){
        diff = a.prob[b.sort_class] - b.prob[b.sort_class];
    } else {
        diff = a.objectness - b.objectness;
    }
    if(diff < 0) return 1;
    else if(diff > 0) return -1;
    return 0;
}

void NY2::correct_region_boxes()
{
    int i;
    int netw = this->nn->in_w;
    int neth = this->nn->in_h;

    int new_w= 0;
    int new_h= 0;

    int w = this->nn->im_or_cols;
    int h = this->nn->im_or_rows;
    if (((float)netw/w) < ((float)neth/h)) {
        new_w = netw;
        new_h = (h * netw)/w;
    } else {
        new_h = neth;
        new_w = (w * neth)/h;
    }
    float factor = (float) (nn->in_h - nn->im_resized_rows) / nn->in_h / 2;
    for (i = 0; i < this->nn->nbbox_total; ++i){
        Box *b = &this->nn->dets[i].bbox;

        // b->y = b->y - factor;
        // b->y /= 2*factor;
        // b->h = b->h;

        b->x =  (b->x - (netw - new_w)/2./netw) / ((float)new_w/netw); 
        b->y =  (b->y - (neth - new_h)/2./neth) / ((float)new_h/neth); 
        b->w *= (float)netw/new_w;
        b->h *= (float)neth/new_h;
        // this->nn->dets[i].bbox = b;
    }
}

/**
 * @brief NMS sort
 * @param total    The total number of bounding boxes
 * @param classes  number of classes
 */
void NY2::do_nms_sort()
{
    float thresh = .45; // like in darknet:detector#575
    int i, j, k;
    int total = this->nn->nbbox_total;
    k = (total)-1;
    for(i = 0; i <= k; ++i){
        if(this->nn->dets[i].objectness == 0){
            detection swap = this->nn->dets[i];
            this->nn->dets[i] = this->nn->dets[k];
            this->nn->dets[k] = swap;
            --k;
            --i;
        }
    }
    total = k+1;

    for(k = 0; k < this->nn->nclasses; ++k){
        for(i = 0; i < total; ++i){
            this->nn->dets[i].sort_class = k;
        }
        qsort(this->nn->dets, total, sizeof(detection), nms_comparator);
        for(i = 0; i < total; ++i){
            if(this->nn->dets[i].prob[k] == 0) continue;
            Box a = this->nn->dets[i].bbox;
            for(j = i+1; j < total; ++j){
                Box b = this->nn->dets[j].bbox;
                if (box_iou(a, b) > thresh){
                    this->nn->dets[j].prob[k] = 0;
                }
            }
        }
    }
}

Box NY2::get_region_box(float *x, int n, int index, int i, int j)
{
    Box b;
    b.x = (j + EXPIT(x[index])) / this->nn->out_w;
    b.y = (i + EXPIT(x[index+1])) / this->nn->out_h;
    b.w = exp(x[index + 2]) * this->nn->anchors[2*n]   / this->nn->out_w;
    b.h = exp(x[index + 3]) * this->nn->anchors[2*n+1] / this->nn->out_h;
    return b;
}

int NY2::get_bboxes(int nbboxes_max)
{
    SPDLOG_TRACE("Start.");
    int i,j,n;
    int wh = this->nn->out_w * this->nn->out_h;
    int b = 0;
    for (i = 0; i < wh; ++i){
        for(n = 0; n < this->nn->nbbox; ++n){
            int obj_index  = b + this->nn->ncoords;  //entry_index(l, 0, n*yolo_w*yolo_h + i, yolo_coords);
            int box_index  = b;                //entry_index(l, 0, n*yolo_w*yolo_h + i, 0);
            int det_index = i + n*wh;           // det have the same size of output but reordered with struct
                                                // det have size 13x13x5=845, total number of bboxes.
                                                // for each one there are 2 different pointers to: bbox(5) and classes(80)
            float scale = EXPIT(this->nn->output[obj_index]);

            for(j = 0; j < this->nn->nclasses; ++j){
                this->nn->dets[det_index].prob[j] = 0;
            }

            this->nn->dets[det_index].bbox = get_region_box(this->nn->output, n, box_index, i / this->nn->out_w, i % this->nn->out_w);
            
            if(scale > this->nn->thresh) {
                this->nn->dets[det_index].objectness = scale;

                /** SOFTMAX **/
                float *pclasses = this->nn->output + b + 5;
                float sum = 0;
                float largest = -FLT_MAX;
                int k;
                int n0_classes = this->nn->nclasses - 1;
                for(k = n0_classes; k >= 0; --k){
                    if(pclasses[k] > largest) largest = pclasses[k];
                }
                for(k = n0_classes; k >= 0; --k){
                    float e = exp(pclasses[k] - largest);
                    sum += e;
                    pclasses[k] = e;
                }
                for(k = n0_classes; k >= 0; --k){
                    float prob = scale * pclasses[k] / sum;
                    this->nn->dets[det_index].prob[k] = (prob > this->nn->thresh) ? prob : 0;
                }
                /** SOFTMAX **/
            }
            b += this->nn->ncoords + 1 + this->nn->nclasses;
        }
    }

    this->correct_region_boxes();

    this->do_nms_sort();

    int nbbox = 0;
    for(int n = 0; n < this->nn->nbbox_total; n++) {
        for(int k = 0; k < this->nn->nclasses; k++) {
            if (this->nn->dets[n].prob[k] > .5) {
                detection d = this->nn->dets[n];

                bbox *_bbox = &this->nn->bboxes[nbbox];

                memcpy(&(_bbox->box), &(d.bbox), 16);  //BBOX
                _bbox->objectness = d.objectness;  //OBJ
                _bbox->prob = d.prob[k];           //Classness
                _bbox->cindex = k;                 //class-index

                if(++nbbox == nbboxes_max) return nbbox;
            }
        }
    }
    SPDLOG_TRACE("End.");
    return nbbox;
}

NY2::NY2(nnet *nn) { this->nn = nn; }

int NY2::inference(int nbboxes_max) {
    SPDLOG_TRACE("Start.");

#ifdef NOMOVIDIUS
    int i = -1;
    char s[20] = {0};
    FILE*f = fopen("out.txt", "r");
    while (fgets(s, 20, f) != NULL) {
       this->nn->output[++i] = atof(s);
    }
    fclose(f);
#endif

    int nb = this->get_bboxes(nbboxes_max);
    SPDLOG_TRACE("End.");
    return nb;
}
