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
    sockaddr_in senderAddress;
    socklen_t addrlen = sizeof(senderAddress);
    int nodeSocket = accept(listeningSocket, (struct sockaddr*)&senderAddress, &addrlen);
    if (nodeSocket != -1) {
        std::string nodeAddress = inet_ntoa(senderAddress.sin_addr);
        if (nodes.find(nodeAddress) == nodes.end()) {
            std::cout << " -  accepted new node: " << nodeAddress << std::endl;
            int flags = fcntl(nodeSocket, F_GETFL, 0);
            fcntl(nodeSocket, F_SETFL, flags/* | O_NONBLOCK*/);
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

   nodes.emplace(host, nodeSocket);

   freeaddrinfo(res);   

   return 0;
}

void Node::send_msg(const std::string& data, const std::string& to) {

    std::cout << "Try send to: " << to << '\n';
    if (send(nodes[to], data.c_str(), strlen(data.c_str()), 0) > 0) {
        std::cout << "Sent successful\n";
    } else {
        std::cout << "Error sending msg\n";
    }

}

std::string Node::recv_msg(const std::string& from) {

    std::vector<char> buffer(4096);
    std::string data;   
    int bytesReceived = recv(nodes[from], &buffer[0], buffer.size(), 0);
    
    if (bytesReceived == -1) { 
        std::cout << "Didn't receive shit :(\n";
        return "";
    } else {
        data.append(buffer.cbegin(), buffer.cend());
        std::cout << "Received msg: " << data << '\n';
        return data;
    }

}

Node::Node() {

    localIpAddress = getLocalIpAddress();
    std::cout << "Local address: " << localIpAddress << std::endl;
    if (localIpAddress.empty()) {
        std::cerr << "Failed to retrieve local IP address." << std::endl;
        // throw exception here
        return;
    }

    //remove higher adresses
    int localIpAddressInt = convertIPToInteger(localIpAddress);
    for (auto it = pendingNodeAddresses.begin(); it != pendingNodeAddresses.end();) {
        
        int nodeIpAddressInt = convertIPToInteger(*it);
        if (nodeIpAddressInt >= localIpAddressInt) it = pendingNodeAddresses.erase(it);
        else ++it;

    }
    
    // use 4343 for commmunication between nodes
    setupListeningSocket("4343");

}

void Node::start() {

    std::cout << "Starting discovery phase\n";
    for (int i = 0; i < 10; ++i) {
        
        std::cout << "Loop No. : " << i << "\n";
        sleep(1);
        
        std::cout << "Try accept connection...\n";
        acceptConnection();

        if (!pendingNodeAddresses.empty()) {
            const std::string& node_address = *pendingNodeAddresses.begin();
            std::cout << "Try connect to: " << node_address << "\n";
            if (connectToNode(node_address, "4343") == 0) {
                pendingNodeAddresses.erase(node_address);
            }
        }
    }

    std::cout << "Discovery done\n";
    for (const auto& el : nodes) {
        std::cout << el.first << ": " << el.second << "\n";
        send_msg("hello from node " + localIpAddress, el.first);

        std::string msg;
        do {
            msg = recv_msg(el.first);
        } while (msg == "");        
    }

    std::cout << "Starting http server\n";
    s.run();
    
}