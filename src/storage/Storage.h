#pragma once

#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>
#include <fstream>
#include <mutex>

class Storage {

    mutable std::mutex mutex;

    const std::string longToShortFileName = "/space/shortToLong.txt";
    const std::string shortToLongFileName = "/space/longToShort.txt";

    std::ifstream shortToLongFile;
    std::ifstream longToShortFile;

    std::unordered_map<std::string, std::string> shortToLong;
    std::unordered_map<std::string, std::string> longToShort;    

    std::string toBase62(size_t hashValue);

public:

    Storage();
    ~Storage();

    std::string getLongUrl(const std::string& shortUrl);
    std::string generateShortUrl(const std::string& longUrl);

};