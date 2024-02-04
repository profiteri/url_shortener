#include "Server.h"

std::shared_ptr<http_response> Server::cut_url_resource::render(const http_request& req) {

    //TODO: logic
    std::cout << "Received cut request\n";
    auto arg = req.get_arg_flat(_cut_full_url_key);
    std::string resp = "url to cut: " + std::string(arg);
    return std::shared_ptr<http_response>(new string_response(resp));

}

std::shared_ptr<http_response> Server::expand_url_resource::render(const http_request& req) {

    //TODO: logic
    std::cout << "Received expand request\n";
    auto arg = req.get_arg_flat(_expand_full_url_key);
    std::string resp = "url to expand: " + std::string(arg);
    return std::shared_ptr<http_response>(new string_response(resp));
    
}

void Server::run() {

    _server = std::unique_ptr<webserver>(new webserver(create_webserver(_port)));

    cut_url_resource r1;
    _server->register_resource(_cut_request_path, &r1);

    expand_url_resource r2;
    _server->register_resource(_expand_request_path, &r2);

    _server->start(true);

}