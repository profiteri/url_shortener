#pragma once

#include <httpserver.hpp>
#include <memory>
#include <iostream>
#include "raft/Raft.h"
#include <curl/curl.h>

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

        Raft& raft;

        cut_url_resource(Raft& raft) : raft(raft) {}
    };

    struct expand_url_resource : public http_resource {
        std::shared_ptr<http_response> render(const http_request&);
        Raft& raft;

        expand_url_resource(Raft& raft) : raft(raft) {}
    }; 

public:

    Raft& r;

    Server(Raft& raft) : r(raft) {
        curl_global_init(CURL_GLOBAL_ALL);
    }

    ~Server() {
        curl_global_cleanup();
    }
    void run();

};