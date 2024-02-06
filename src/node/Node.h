#pragma once

#include "http_server/Server.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_set>
#include <unordered_map>

class Node {
    Server s;

public:
    Node();
    ~Node();

private:

    std::string localIpAddress;
    int listeningSocket;

    std::unordered_set<std::string> nodeAddresses = {"10.0.1.4", "10.0.1.5"};//, "10.0.1.5", "10.0.1.6", "10.0.1.7", "10.0.1.8"};
    std::unordered_map<std::string, int> readSockets;
    std::unordered_map<std::string, int> writeSockets;

    std::string getLocalIpAddress();
    void broadcast();
    int setupListeningSocket(const std::string& port);
    int connectToNode(const std::string& host, const std::string& port = "4343");
    void acceptConnection();

    friend class Raft;

};