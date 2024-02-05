#pragma once

#include "http_server/Server.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_set>
#include <unordered_map>

class Node {
    Server s;

public:
    void start();
    Node();

private:
    std::string localIpAddress;
    sockaddr_in broadcastAddress;
    sockaddr_in listenAddress;
    int broadcastSocket;
    int broadcastListenSocket;
    std::unordered_map<std::string, int> nodes;
    std::unordered_set<std::string> pendingNodeAddresses;
    std::string GetLocalIpAddress();
    void broadcast();
    int listeningSocket;
    int setupListeningSocket(const std::string& port);
    int connectToNode(const std::string& host, const std::string& port);
    void acceptConnection();
};