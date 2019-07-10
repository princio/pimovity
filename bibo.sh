#!/bin/bash

for i in {1..60}
do
    rungeant4 ./DICOM run$i.mac
done