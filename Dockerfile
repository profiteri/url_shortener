FROM ubuntu:22.04 as base

RUN apt-get update && apt-get install -y cmake g++ libasan6 libcurl4-openssl-dev
WORKDIR cbdp

FROM base as node

COPY build/node .
CMD exec ./node
