#include <iostream>
#include "Node.h"
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

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


std::string Node::GetLocalIpAddress() {
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

void Node::broadcast() {
    char buffer[256];
    sockaddr_in senderAddress;
    socklen_t senderAddressSize = sizeof(senderAddress);
    const char* broadcastMessage = "Node Discovery";
    sendto(broadcastSocket, broadcastMessage, strlen(broadcastMessage), 0, (struct sockaddr*)&broadcastAddress, sizeof(broadcastAddress));
    recvfrom(broadcastListenSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&senderAddress, &senderAddressSize);
    std::string nodeAddress = inet_ntoa(senderAddress.sin_addr);
    if (nodeAddress != localIpAddress && nodes.find(nodeAddress) == nodes.end()) {
        int localIpAddressInt = convertIPToInteger(localIpAddress);
        int nodeAddressInt = convertIPToInteger(nodeAddress);
        if (nodeAddressInt > localIpAddressInt && pendingNodeAddresses.find(nodeAddress) == pendingNodeAddresses.end()) {
            pendingNodeAddresses.insert(nodeAddress);
            std::cout << "Received from: " << nodeAddress << " Message: " << buffer << std::endl;
        }
    }
}

void Node::acceptConnection() {
    sockaddr_in senderAddress;
    socklen_t addrlen = sizeof(senderAddress);
    int nodeSocket = accept(listeningSocket, (struct sockaddr*)&senderAddress, &addrlen);
    if (nodeSocket != -1) {
        std::string nodeAddress = inet_ntoa(senderAddress.sin_addr);
        if (nodes.find(nodeAddress) == nodes.end()) {
            std::cout << " -  accepted new node: " << nodeAddress << std::endl;
            int flags = fcntl(nodeSocket, F_GETFL, 0);
            fcntl(nodeSocket, F_SETFL, flags | O_NONBLOCK);
            nodes.emplace(nodeAddress, nodeSocket);
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

   std::cout << "Connected to " << host << std::endl;

   nodes.emplace(host, nodeSocket);

   freeaddrinfo(res);   

   return 0;
}

Node::Node() {
    localIpAddress = GetLocalIpAddress();
    std::cout << "Local address: " << localIpAddress << std::endl;
    if (localIpAddress.empty()) {
        std::cerr << "Failed to retrieve local IP address." << std::endl;
        // throw exception here
        return;
    }
    const int BROADCAST_PORT = 4242;

    // Create a socket for broadcasting
    broadcastSocket = socket(AF_INET, SOCK_DGRAM, 0);

    // Enable broadcast option
    int broadcastEnable = 1;
    setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    // Set up the broadcast address
    broadcastAddress.sin_family = AF_INET;
    broadcastAddress.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcastAddress.sin_port = htons(BROADCAST_PORT);

    // Create a socket for listening
    broadcastListenSocket = socket(AF_INET, SOCK_DGRAM, 0);

    // Set up the listening address
    listenAddress.sin_family = AF_INET;
    listenAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    listenAddress.sin_port = htons(BROADCAST_PORT);

    // Bind the socket to the listening address
    bind(broadcastListenSocket, (struct sockaddr*)&listenAddress, sizeof(listenAddress));
    // Non-blocking
    int flags = fcntl(broadcastListenSocket, F_GETFL, 0);
    fcntl(broadcastListenSocket, F_SETFL, flags | O_NONBLOCK);

    // use 4343 for commmunication between nodes
    setupListeningSocket("4343");
}

void Node::start() {
    std::cout << "Starting the node...\n";
    while (true) {
        broadcast();
        sleep(1);
        acceptConnection();
        if (!pendingNodeAddresses.empty()) {
            const std::string& node_address = *pendingNodeAddresses.begin();
            // use 4343 for commmunication between nodes
            if (connectToNode(node_address, "4343") == 0) {
                pendingNodeAddresses.erase(node_address);
            }
        }
    }
    s.run();
}