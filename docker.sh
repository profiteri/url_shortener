#!/bin/bash

docker rm node

if [ "$1" == "build" ]; then

    echo "Bulding..."

    cmake --build build --target all

    docker build -t cbdp_node --target node .

fi

docker run --mount source=cbdp-volume,target=/space --name=node cbdp_node

