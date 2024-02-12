FROM bravehead/url-shortener:latest as base

FROM base as node

COPY src src
COPY proto proto
COPY CMakeLists.txt .

RUN mkdir build && cd build && cmake .. && make

ENTRYPOINT ./build/node