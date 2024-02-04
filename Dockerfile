FROM ubuntu:22.04 as base

WORKDIR cbdp
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    cmake \
    libtool \
    automake \
    autoconf \
    nasm \
    pkgconf \
    libasan6 \
    libcurl4-openssl-dev \
    libmicrohttpd-dev \
    wget

RUN wget https://github.com/etr/libhttpserver/archive/refs/tags/0.19.0.tar.gz && \
    tar -xf 0.19.0.tar.gz && \
    cd libhttpserver-0.19.0 && \
    ./bootstrap && \
    mkdir build && \
    cd build && \
    ../configure && \
    make && \
    make install

FROM base as node

COPY src src
COPY CMakeLists.txt .

RUN mkdir build && cd build && cmake .. && make
ENTRYPOINT ./build/node
