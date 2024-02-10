#!/bin/bash

for ((i = 0; i < 2; i++))
do
    docker stop node_$i
done

docker network create cbdp_net

if [ "$1" == "build" ]; then

    echo "Bulding..."
    docker build -t cbdp_node --target node .

fi

for ((i = 0; i < 2; i++))
do
    docker volume rm cbdp-volume-$i
    docker volume create cbdp-volume-$i
    docker run --rm -d --network=cbdp_net --mount source=cbdp-volume-$i,target=/space --name=node_$i cbdp_node
done
