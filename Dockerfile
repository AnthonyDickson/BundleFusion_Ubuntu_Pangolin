FROM nvidia/cuda:11.6.0-devel-ubuntu20.04

RUN apt update &&  \
    DEBIAN_FRONTEND="noninteractive" apt install -y --no-install-recommends  \
    wget unzip git make cmake gcc clang ninja-build gdb libeigen3-dev libncurses5-dev libncursesw5-dev libfreeimage-dev \
    # libs for FFMPEG functionality in OpenCV
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libgtk2.0-dev pkg-config && \
    rm -rf /var/lib/apt/lists/*

# Download and install OpenCV
ENV OPENCV_VERSION=3.4.16

RUN wget -O opencv.zip https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VERSION}.zip && \
    unzip opencv.zip && \
    mkdir -p opencv-build &&  \
    cd opencv-build && \
    cmake -D WITH_CUDA=ON \
    -D BUILD_EXAMPLES=OFF -D BUILD_opencv_apps=OFF -D BUILD_DOCS=OFF -D BUILD_PERF_TESTS=OFF -D BUILD_TESTS=OFF ../opencv-${OPENCV_VERSION} && \
    cmake --build . -- -j 8 && \
    make install && \
    cd .. && \
    rm opencv.zip && \
    rm -rf opencv-${OPENCV_VERSION} && \
    rm -rf opencv-build

WORKDIR /app

ENTRYPOINT ["./compile_and_run.sh"]