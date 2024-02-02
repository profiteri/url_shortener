#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>


enum class NodeType {
    Follower = 0,
    Leader = 1
};


std::unordered_map<std::string, std::string> shortToLong;
std::unordered_map<std::string, std::string> longToShort;


std::string toBase62(size_t value) {
    const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string result;

    do {
        result.insert(result.begin(), characters[value % 62]);
        value /= 62;
    } while (value != 0);

    return result;
}


std::string generateShortURL(const std::string& longURL) {
    // Check if the URL is present
    auto it = longToShort.find(longURL);
    if (it != longToShort.end()) {
        return it->second;
    }
    // Use a hash function to generate a hash value
    std::hash<std::string> hasher;
    size_t hashValue = hasher(longURL);

    // Convert the hash value to a string
    std::string shortHash = toBase62(hashValue);

    // Append counter to handle duplicate hash values
    shortHash += std::to_string(longToShort.size());

    longToShort[longURL] = shortHash;
    shortToLong[shortHash] = longURL;

    std::cout << longURL << " -> " << shortHash << std::endl;

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

    auto short1 = generateShortURL("longURLtest1");

    auto short2 = generateShortURL("longURLtest2");

    getLongURL(short1);
    getLongURL(short2);
    getLongURL("unknownShortURL");

    return 0;
}