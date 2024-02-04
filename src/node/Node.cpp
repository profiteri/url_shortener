#include <iostream>
#include "Node.h"
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <arpa/inet.h>
#include <unordered_set>
#include <unistd.h>


std::string GetLocalIpAddress() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    // Use a dummy IP address and port for the destination
    sockaddr_in dummyDest;
    memset(&dummyDest, 0, sizeof(dummyDest));
    dummyDest.sin_family = AF_INET;
    dummyDest.sin_addr.s_addr = inet_addr("8.8.8.8");
    dummyDest.sin_port = htons(5353);  // DNS port

    // Connect the socket to the dummy destination to get the local address
    connect(sock, reinterpret_cast<struct sockaddr*>(&dummyDest), sizeof(dummyDest));

    // Get the local address assigned to the socket
    sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);
    getsockname(sock, reinterpret_cast<struct sockaddr*>(&localAddr), &addrLen);

    // Close the socket
    close(sock);

    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(localAddr.sin_family, &(localAddr.sin_addr), ipAddress, INET_ADDRSTRLEN);

    return ipAddress;

    // char hostName[255];
    // if (gethostname(hostName, sizeof(hostName)) != 0) {
    //     std::cerr << "Error getting hostname." << std::endl;
    //     return "";
    // }

    // struct addrinfo* result = nullptr;
    // struct addrinfo hints;

    // memset(&hints, 0, sizeof(hints));
    // hints.ai_family = AF_INET;
    // hints.ai_socktype = SOCK_STREAM;

    // if (getaddrinfo(hostName, nullptr, &hints, &result) != 0) {
    //     std::cerr << "Error getting address info." << std::endl;
    //     return "";
    // }

    // char ipAddress[INET_ADDRSTRLEN];
    // inet_ntop(result->ai_family, &((struct sockaddr_in*)result->ai_addr)->sin_addr, ipAddress, sizeof(ipAddress));

    // freeaddrinfo(result);

    // return ipAddress;
}


void broadcast() {
    std::string localIpAddress = GetLocalIpAddress();

    std::cout << "Local address: " << localIpAddress << std::endl;

    if (localIpAddress.empty()) {
        std::cerr << "Failed to retrieve local IP address." << std::endl;
        return;
    }

    const int PORT = 4242;

    // Create a socket for broadcasting
    int broadcastSocket = socket(AF_INET, SOCK_DGRAM, 0);

    // Enable broadcast option
    int broadcastEnable = 1;
    setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    // Set up the broadcast address
    sockaddr_in broadcastAddress;
    broadcastAddress.sin_family = AF_INET;
    broadcastAddress.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcastAddress.sin_port = htons(PORT);

    // // Send a broadcast message
    // const char* broadcastMessage = "Node Discovery";
    // sendto(broadcastSocket, broadcastMessage, strlen(broadcastMessage), 0, (struct sockaddr*)&broadcastAddress, sizeof(broadcastAddress));

    // // Close the broadcast socket
    // close(broadcastSocket);

    // Create a socket for listening
    int listenSocket = socket(AF_INET, SOCK_DGRAM, 0);

    // Set up the listening address
    sockaddr_in listenAddress;
    listenAddress.sin_family = AF_INET;
    listenAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    listenAddress.sin_port = htons(PORT);

    // Bind the socket to the listening address
    bind(listenSocket, (struct sockaddr*)&listenAddress, sizeof(listenAddress));

    // Receive and print incoming messages
    char buffer[256];
    sockaddr_in senderAddress;
    socklen_t senderAddressSize = sizeof(senderAddress);

    std::unordered_set<std::string> nodeAddresses;
    
    while(true) {
        const char* broadcastMessage = "Node Discovery";
        sendto(broadcastSocket, broadcastMessage, strlen(broadcastMessage), 0, (struct sockaddr*)&broadcastAddress, sizeof(broadcastAddress));
        recvfrom(listenSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&senderAddress, &senderAddressSize);
        std::string nodeAddress = inet_ntoa(senderAddress.sin_addr);
        if (nodeAddress != localIpAddress && nodeAddresses.find(nodeAddress) == nodeAddresses.end()) {
            nodeAddresses.insert(nodeAddress);
            std::cout << "Received from: " << nodeAddress << " Message: " << buffer << std::endl;
        }
        sleep(1);
    }

    // Close the listening socket
    close(listenSocket);
    close(broadcastSocket);
}

void Node::start() {
    std::cout << "Starting the node...\n";
    broadcast();
    s.run();
}