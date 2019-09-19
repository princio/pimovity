#! /bin/bash

# required .cfg .weights .names

flow --model yolov2.cfg --load yolov2.weights  --savepb

export PYTHONPATH="${PYTHONPATH}:/opt/movidius/caffe/python"

mvNCCompile yolov2.pb -s 12 -in input -on output -o yolov2.graph
