#!/bin/bash

source params.sh

cmake --build build --target all

docker build -t cbdp_node --target node .

az login --use-device-code

# Create a "resource group" that bundles your setup
az group create --name cbdp-resourcegroup --location westeurope
# Create a container registry for your coordinator/worker containers
# You need a unique name for your registry, specify it in params.sh
az acr create --name "$REGISTRY_NAME" --resource-group cbdp-resourcegroup --sku standard --admin-enabled true
# See your login details. These are PRIVATE, don't show around, but you will need them to start containers - so copy paste them somewhere.
az acr credential show --name "$REGISTRY_NAME" --resource-group cbdp-resourcegroup
# Log into the azure docker registry. Use the credentials from the last command
docker login "$REGISTRY_NAME.azurecr.io"

# Push your local containers to the registry
docker tag cbdp_node "$REGISTRY_NAME.azurecr.io"/cbdp_node
docker push "$REGISTRY_NAME.azurecr.io"/cbdp_node

# You should now see your images in azure:
az acr repository list --name "$REGISTRY_NAME" --resource-group cbdp-resourcegroup

# Create a network between the containers
az network vnet create -g cbdp-resourcegroup --name cbdpVnet
az network vnet subnet create -g cbdp-resourcegroup --vnet-name cbdpVnet -n cbdpSubnet --address-prefixes 10.0.0.0/24

for ((i = 0; i < 2; i++))
do
    az container create -g cbdp-resourcegroup --vnet cbdpVnet --subnet cbdpSubnet --restart-policy Never --name node-$i --image "$REGISTRY_NAME.azurecr.io"/cbdp_node
done