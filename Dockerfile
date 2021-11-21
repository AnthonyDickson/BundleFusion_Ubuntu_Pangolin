FROM nvidia/cuda:10.1-devel-ubuntu18.04

RUN apt update &&  \
    apt install -y --no-install-recommends  \
    wget unzip make cmake gcc ninja-build libeigen3-dev libncurses5-dev libncursesw5-dev libfreeimage-dev \
    # libs for FFMPEG functionality in OpenCV
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libgtk2.0-dev pkg-config && \
    rm -rf /var/lib/apt/lists/*

# Download and install OpenCV
RUN wget -O opencv.zip https://github.com/opencv/opencv/archive/master.zip && \
    unzip opencv.zip && \
    mkdir -p opencv-build && cd opencv-build && \
    cmake  ../opencv-master && \
    cmake --build . -- -j 8 && \
    make install && \
    rm ../opencv.zip && \
    rm -rf opencv-master

WORKDIR /app

COPY CMakeLists.txt zParametersBundlingDefault.txt zParametersDefault.txt ./
ADD cmake cmake
ADD include include
ADD src src
ADD example example

RUN mkdir build && \
    cd build &&  \
    cmake -DVISUALIZATION=OFF .. &&  \
    make -j8

ENTRYPOINT ["./build/bundle_fusion_example", "./zParametersDefault.txt", "./zParametersBundlingDefault.txt"]