#pragma once

#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>
#include <fstream>
#include <mutex>

class Storage {

    mutable std::mutex mutex;

    std::unordered_map<std::string, std::string> shortToLong;
    std::unordered_map<std::string, std::string> longToShort;    

    std::string toBase62(size_t hashValue);

public:

    std::string getLongUrl(const std::string& shortUrl);
    std::string generateShortUrl(const std::string& longUrl);
    void insertURL(const std::string& longURL, const std::string& shortURL);

};