#!/bin/bash

for ((i = 0; i < 2; i++))
do
    docker stop node_$i
    docker rm node_$i
done

docker network create cbdp_net

if [ "$1" == "build" ]; then

    echo "Bulding..."

    cmake --build build --target all

    docker build -t cbdp_node --target node .

fi

for ((i = 0; i < 2; i++))
do
    docker run -d --network=cbdp_net --mount source=cbdp-volume,target=/space --name=node_$i cbdp_node
done
