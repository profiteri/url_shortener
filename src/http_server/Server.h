#pragma once

#include <httpserver.hpp>
#include <memory>
#include <iostream>

using namespace httpserver;

static const std::string _cut_request_path = "/cut";
static const std::string _cut_full_url_key = "full-url";
static const std::string _expand_request_path = "/expand";
static const std::string _expand_full_url_key = "short-url";

class Server {

    const int _port = 80; 
    std::unique_ptr<webserver> _server;

    struct cut_url_resource : public http_resource {
        std::shared_ptr<http_response> render(const http_request&);
    };

    struct expand_url_resource : public http_resource {
        std::shared_ptr<http_response> render(const http_request&);
    };    

public:

    Server() = default;
    void run();

};