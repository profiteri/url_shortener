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

    void send_msg(const std::string& data, const std::string& to);
    //void broadcast_msg(const std::string& data);

    std::string recv_msg(const std::string& from);
    //std::string recv_msg_blocking(const std::string& from);

    enum NodeType : unsigned char {
        Follower = 0,
        Leader = 1
    };

    std::unordered_map<std::string, int> nodes;

private:

    std::string localIpAddress;
    sockaddr_in broadcastAddress;
    sockaddr_in listenAddress;
    int broadcastSocket;
    int broadcastListenSocket;

    std::unordered_set<std::string> pendingNodeAddresses = {"10.0.1.4", "10.0.1.5"};//, "10.0.1.5", "10.0.1.6", "10.0.1.7", "10.0.1.8"};

    std::string getLocalIpAddress();
    void broadcast();
    int listeningSocket;
    int setupListeningSocket(const std::string& port);
    int connectToNode(const std::string& host, const std::string& port);
    void acceptConnection();

};