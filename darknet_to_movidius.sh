#! /bin/bash

echo "Execution must be done from PIMOVITY folder!!!"

if [ $# -eq 0 ]; then
    echo "Too less arguments."
    exit 1
fi
# required .cfg .weights .names
FILE="$1"
__PATH="./data/yolov2/ale/"
NAME="${FILE:0:-4}"

echo "File is: «$FILE»."

if [ ! -f $FILE ]; then
    echo "Zip file not exists! Exit."
    exit 1
fi

RET=`mkdir -p $NAME`
if [[ $RET -gt 0 ]]; then
    echo "mkdir failed."
    exit 1
fi

RET=`unzip $NAME.zip -d $__PATH/$NAME`
if [[ $RET -gt 0 ]]; then
    echo "Unzip failed."
    exit 1
fi

cd $__PATH/$NAME

if [ ! -f $NAME.weights ]; then
    echo "$NAME.weights file not exists! Exit."
    exit 1
fi
if [ ! -f $NAME.cfg ]; then
    echo "$NAME.cfg file not exists! Exit."
    exit 1
fi
if [ ! -f labels.txt ]; then
    echo "Labels.txt file not exists! Exit."
    exit 1
fi

flow --model $NAME.cfg --load $NAME.weights  --savepb

export PYTHONPATH="${PYTHONPATH}:/opt/movidius/caffe/python"

mvNCCompile $NAME.pb -s 12 -in input -on output -o $NAME.graph

mv $NAME.meta ../../yolov2-tiny-ale-$NAME.meta
mv $NAME.graph ../../yolov2-tiny-ale-$NAME.graph

cd ../..

rm -r $NAME
