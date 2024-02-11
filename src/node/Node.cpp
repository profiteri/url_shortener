#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if.h>
#include <sstream>

#include "Node.h"

int convertIPToInteger(const std::string& ipAddress) {
    std::stringstream ss;
    for (char c : ipAddress) {
        if (std::isdigit(c)) {
            ss << c;
        }
    }
    int result;
    ss >> result;
    return result;
}


std::string Node::getLocalIpAddress() {
    char hostName[255];
    if (gethostname(hostName, sizeof(hostName)) != 0) {
        std::cerr << "Error getting hostname." << std::endl;
        return "";
    }
    struct addrinfo* result = nullptr;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostName, nullptr, &hints, &result) != 0) {
        std::cerr << "Error getting address info." << std::endl;
        return "";
    }
    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(result->ai_family, &((struct sockaddr_in*)result->ai_addr)->sin_addr, ipAddress, sizeof(ipAddress));
    freeaddrinfo(result);
    return ipAddress;
}

void Node::acceptConnection() {
    std::cout << "Try accept new connection\n";
    sockaddr_in senderAddress;
    socklen_t addrlen = sizeof(senderAddress);
    int nodeSocket = accept(listeningSocket, (struct sockaddr*)&senderAddress, &addrlen);
    if (nodeSocket != -1) {
        std::string nodeAddress = inet_ntoa(senderAddress.sin_addr);
        if (readSockets.find(nodeAddress) == readSockets.end()) {
            std::cout << " -  accepted new node: " << nodeAddress << std::endl;
            int flags = fcntl(nodeSocket, F_GETFL, 0);
            fcntl(nodeSocket, F_SETFL, flags | O_NONBLOCK);
            readSockets.emplace(nodeAddress, nodeSocket);
        } else {
            close(nodeSocket);
        }
    }
}

int Node::setupListeningSocket(const std::string& port) {
   addrinfo hints, *res;
   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

   getaddrinfo(NULL, port.c_str(), &hints, &res);
   
   listeningSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   if (listeningSocket == -1) {
      std::cerr << "Couldn't create coordinator socket" << std::endl;
      freeaddrinfo(res);
      return 1;
   }
   // set socket to non-blocking mode for non-blocking accept()
   int flags = fcntl(listeningSocket, F_GETFL, 0);
   fcntl(listeningSocket, F_SETFL, flags | O_NONBLOCK);

   if (bind(listeningSocket, res->ai_addr, res->ai_addrlen) == -1) {
      std::cerr << "Couldn't bind coordinator socket" << std::endl;
      close(listeningSocket);
      freeaddrinfo(res);
      return 1;
   }
   freeaddrinfo(res);
   if (listen(listeningSocket, 20) == -1) {
      std::cerr << "Listen error" << std::endl;
      close(listeningSocket);
      return 1;
   }

   return 0;
}

int Node::connectToNode(const std::string& host, const std::string& port) {
   addrinfo hints, *res;
   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

   if (auto status = getaddrinfo(host.c_str(), port.c_str(), &hints, &res)) {
        std::cerr << "Couldn't getaddrinfo for " << host  << ": " << std::string(gai_strerror(status)) << std::endl;
        return -1;
        // throw std::runtime_error(std::string(gai_strerror(status)));
   }
   
   int nodeSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   if (connect(nodeSocket, res->ai_addr, res->ai_addrlen) == -1) {
        std::cerr << "Couldn't connect to " << host << " yet" << std::endl;
        close(nodeSocket);
        return -1;
   }

   writeSockets.emplace(host, nodeSocket);

   freeaddrinfo(res);   

   return 0;
}

Node::Node() {

    localIpAddress = getLocalIpAddress();
    std::cout << "Local address: " << localIpAddress << std::endl;
    if (localIpAddress.empty()) {
        std::cerr << "Failed to retrieve local IP address." << std::endl;
        // throw exception here
        return;
    }

    id = convertIPToInteger(localIpAddress);

    numNodes = nodeAddresses.size();
    numNodesForConsesus = 1 + (numNodes / 2);

    nodeAddresses.erase(localIpAddress);

    // use 4343 for commmunication between nodes
    if (setupListeningSocket("4343")) throw std::runtime_error("");

}

Node::~Node() {
    close(listeningSocket);
    for (const auto& pair : writeSockets) {
        close(pair.second);
    }
    for (const auto& pair : readSockets) {
        close(pair.second);
    }    
}