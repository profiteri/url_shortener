#!/bin/bash

docker rm node

if [ "$1" == "build" ]; then

    echo "Bulding..."
    docker build -t cbdp_node --target node .

fi

docker run --rm -it --network host --mount source=cbdp-volume,target=/space --name=node cbdp_node bash