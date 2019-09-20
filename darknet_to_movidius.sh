#! /bin/bash

echo "Execution must be done from PIMOVITY folder!!!"

# required .cfg .weights .names
NAME="$1"
__PATH="./data/yolov2/ale/$NAME/"
FILE="${__PATH}$NAME.zip"

echo "File is: «$FILE»."

if [ ! -f $FILE ]; then
    echo "Zip file not exists! Exit."
    exit 1
fi

cd $__PATH

RET=`unzip $NAME.zip`
if [[ $RET -gt 0 ]]; then
    echo "Unzip filed."
    exit 1
fi


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

cd built_graph

export PYTHONPATH="${PYTHONPATH}:/opt/movidius/caffe/python"

mvNCCompile $NAME.pb -s 12 -in input -on output -o $NAME.graph

mv $NAME.graph ../../yolov2-tiny-$NAME.graph
mv $NAME.meta ../../yolov2-tiny-$NAME.meta
