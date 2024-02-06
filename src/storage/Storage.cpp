#include "Storage.h"

std::string Storage::toBase62(size_t hashValue) {
    const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string result;

    do {
        result.insert(result.begin(), characters[hashValue % 62]);
        hashValue /= 62;
    } while (hashValue != 0);

    return result;
}

Storage::Storage() {
    shortToLongFile.open(shortToLongFileName);
    longToShortFile.open(longToShortFileName);
}

Storage::~Storage() {
    shortToLongFile.close();
    longToShortFile.close();
}

std::string Storage::getLongUrl(const std::string& shortURL) {

    const std::lock_guard lock(mutex);

     // Check if the URL is present
    auto it = shortToLong.find(shortURL);
    if (it != shortToLong.end()) {
        std::cout << shortURL << " -> " << it->second << std::endl;
        return it->second;
    }
    std::cout << shortURL << " is not mapped yet" << std::endl;
    return "";
}

std::string Storage::generateShortUrl(const std::string& longURL) {

    const std::lock_guard lock(mutex);

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