#include "Server.h"

std::shared_ptr<http_response> Server::cut_url_resource::render(const http_request& req) {

    auto arg = req.get_arg_flat(_cut_full_url_key);
    std::string longURL = std::string(arg);
    std::cout << "Received cut request: " << longURL << std::endl;
    if (raft.nodeType == Raft::NodeType::Leader) {
        std::string shortURL = raft.storage.generateShortUrl(longURL);
        // leader logic
        return std::shared_ptr<http_response>(new string_response(shortURL));
    } else {
        // forward
        std::string resp = "url to cut: " + longURL;
        return std::shared_ptr<http_response>(new string_response(resp));
    }
}

std::shared_ptr<http_response> Server::expand_url_resource::render(const http_request& req) {

    auto arg = req.get_arg_flat(_expand_full_url_key);
    std::string shortURL = std::string(arg);
    std::cout << "Received expand request: " << shortURL << std::endl;
    std::string resp = raft.storage.getLongUrl(shortURL);
    return std::shared_ptr<http_response>(new string_response(resp));
    
}

void Server::run() {

    _server = std::unique_ptr<webserver>(new webserver(create_webserver(_port)));

    cut_url_resource r1(r);
    _server->register_resource(_cut_request_path, &r1);

    expand_url_resource r2(r);
    _server->register_resource(_expand_request_path, &r2);

    _server->start(true);

}