#!/bin/bash
mkdir -p build
cd build || exit
cmake -DVISUALIZATION=OFF ..
make -j8
cd ..
./build/bundle_fusion_example ./zParametersDefault.txt ./zParametersBundlingDefault.txt $1 $2
