#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>
#include <fstream>


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


int main(int argc, char* argv[]) {
    if (argc) {
        std::cout << "Started " << argv[0] << std::endl;
    }

    std::ifstream shortToLongFile("/space/shortToLong.txt");
    std::ifstream longToShortFile("/space/longToShort.txt");

    if (shortToLongFile.is_open() && longToShortFile.is_open()) {
        std::string key;
        std::string value;

        while (shortToLongFile >> key >> value) {
            shortToLong[key] = value;
        }

        while (longToShortFile >> key >> value) {
            longToShort[key] = value;
        }
    } else if (shortToLongFile.is_open() || longToShortFile.is_open()) {
        std::cerr << "Only one hashtable file is found" << std::endl;
        return 1;
    }

    auto short1 = generateShortURL("longURLtest1");

    auto short2 = generateShortURL("longURLtest2");

    getLongURL(short1);
    getLongURL(short2);
    getLongURL("unknownShortURL");

    return 0;
}