#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>
#include <fstream>
#include <set>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <arpa/inet.h>
#include <unordered_set>
#include <unistd.h>


enum class NodeType {
    Follower = 0,
    Leader = 1
};


std::unordered_map<std::string, std::string> shortToLong;
std::unordered_map<std::string, std::string> longToShort;


std::string toBase62(size_t hashValue) {
    const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string result;

    do {
        result.insert(result.begin(), characters[hashValue % 62]);
        hashValue /= 62;
    } while (hashValue != 0);

    return result;
}


std::string generateShortURL(const std::string& longURL) {
    // Check if the URL is present
    auto it = longToShort.find(longURL);
    if (it != longToShort.end()) {
        std::cout << longURL << " -> " << it->second << " (existing entry)" << std::endl;
        return it->second;
    }
    // Use a hash function to generate a hash value
    std::hash<std::string> hasher;
    size_t hashValue = hasher(longURL);

    // Convert the hash value to a string of digits and letters
    std::string shortHash = toBase62(hashValue);

    // Append counter to handle duplicate hash values
    shortHash += std::to_string(longToShort.size());

    longToShort[longURL] = shortHash;
    std::ofstream longToShortFile("/space/longToShort.txt", std::ios::out | std::ios::app);
    longToShortFile << longURL << ' ' << shortHash << std::endl;
    shortToLong[shortHash] = longURL;
    std::ofstream shortToLongFile("/space/shortToLong.txt", std::ios::out | std::ios::app);
    shortToLongFile << shortHash << ' ' << longURL << std::endl;

    std::cout << longURL << " -> " << shortHash << " (new entry)" << std::endl;

    return shortHash;
}


std::string getLongURL(const std::string& shortURL) {
     // Check if the URL is present
    auto it = shortToLong.find(shortURL);
    if (it != shortToLong.end()) {
        std::cout << shortURL << " -> " << it->second << std::endl;
        return it->second;
    }
    std::cout << shortURL << " is not mapped yet" << std::endl;
    return "";
}


// Always 127.0.0.1 when executed in docker locally
std::string GetLocalIpAddress() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        std::cerr << "Error creating socket for local IP address retrieval." << std::endl;
        return "";
    }

    sockaddr_in loopback;
    memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    loopback.sin_port = htons(9);  // Discard port

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&loopback), sizeof(loopback)) == -1) {
        close(sock);
        std::cerr << "Error connecting to local address." << std::endl;
        return "";
    }

    sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);
    if (getsockname(sock, reinterpret_cast<struct sockaddr*>(&localAddr), &addrLen) == -1) {
        close(sock);
        std::cerr << "Error getting local address." << std::endl;
        return "";
    }

    close(sock);

    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &localAddr.sin_addr, ipAddress, sizeof(ipAddress));
    return ipAddress;
}


int main(int argc, char* argv[]) {
    if (argc) {
        std::cout << "Started " << argv[0] << std::endl;
    }

    std::string localIpAddress = GetLocalIpAddress();

    std::cout << "Local address: " << localIpAddress << std::endl;

    if (localIpAddress.empty()) {
        std::cerr << "Failed to retrieve local IP address." << std::endl;
        return 1;
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

    const int BROADCAST_DURATION_SECONDS = 10;

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

    // std::ifstream shortToLongFile("/space/shortToLong.txt");
    // std::ifstream longToShortFile("/space/longToShort.txt");

    // if (shortToLongFile.is_open() && longToShortFile.is_open()) {
    //     std::string key;
    //     std::string value;

    //     while (shortToLongFile >> key >> value) {
    //         shortToLong[key] = value;
    //     }

    //     while (longToShortFile >> key >> value) {
    //         longToShort[key] = value;
    //     }
    // } else if (shortToLongFile.is_open() || longToShortFile.is_open()) {
    //     std::cerr << "Only one hashtable file is found" << std::endl;
    //     return 1;
    // }

    // auto short1 = generateShortURL("longURLtest1");

    // auto short2 = generateShortURL("longURLtest2");

    // getLongURL(short1);
    // getLongURL(short2);
    // getLongURL("unknownShortURL");

    return 0;
}