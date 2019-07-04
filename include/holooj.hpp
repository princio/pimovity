#ifndef __HOLOOJ_H__
#define __HOLOOJ_H__

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <sstream>
#include <boost/format.hpp>
#include <iostream>
#include <spdlog/spdlog.h>

typedef unsigned char byte;
typedef const int error;

typedef struct rgb_pixel {
    byte r;
    byte g;
    byte b;
} rgb_pixel;

typedef struct Box{
    float x, y, w, h;
} Box;

typedef struct bbox{
    Box box;
    float objectness;
    float prob;
    int cindex;
} bbox;

typedef enum {
	NCSNN_YOLOv2
} NCSNNType;

typedef struct detection{
    Box bbox;
    //int classes;
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
    int im_or_cols;
    int im_or_rows;
    int im_or_size;
    int im_resized_cols;
    int im_resized_rows;
    int im_resized_size;
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
    float *input_letterbox;
    detection *dets;
    bbox *bboxes;
} nnet;

#define RE( expr, msg, ... )\
    if((expr) < 0) {\
        printf("\n[%s::%d] error: "msg" [%d=%s].\n\n",\
                __FILE__, __LINE__, code, errno, strerror(errno), ##__VA_ARGS__);\
        return -1;\
    }


#define REPORTSPD_ERRNO( expr )\
    if((expr)) {\
        SPDLOG_ERROR("{} [{}]", strerror(errno),  errno);\
        return -1;\
    }

#define REPORT_ERRNO( expr, code, msg, ... )\
    if((expr) < 0) {\
        printf("\n[%s::%d] error "#code"=%d: " msg " [%d=%s].\n\n",\
                __FILE__, __LINE__, code, errno, strerror(errno), ##__VA_ARGS__);\
        return -1;\
    }


#define REPORTSPD( expr, msg, ... )\
    if((expr)) {\
        SPDLOG_ERROR(msg, ##__VA_ARGS__);\
        return -1;\
    }

#define REPORT( expr, code, msg, ... )\
    if((expr) < 0) {\
        printf("\n[%s::%d] error " #code "=%d: " msg ".\n\n", __FILE__, __LINE__, code, ##__VA_ARGS__);\
        return -1;\
    }

#define RIFE2( expr, mask, code, msg, ... )\
    if((expr)) {\
        if(!strcmp(#mask, "SO")) {\
            printf("\n[%s::%d]\t"#mask" "#code"\t(errno#%d=%s)"msg".\n\n", __FILE__, __LINE__, errno, strerror(errno), ##__VA_ARGS__);\
            socket_errno = (mask##Error) mask##code;\
        } else {\
            printf("\n[%s::%d]\t"#mask" "#code"\t"msg".\n\n", __FILE__, __LINE__, ##__VA_ARGS__);\
            socket_errno = (mask##Error) mask##code;\
        }\
	return -1;\
    }



#endif