#include "Server.h"

size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    if (data == nullptr) {
        return 0;
    }
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string forwardToLeader(const std::string& leaderIP, const std::string& longURL) {
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, leaderIP.c_str());

        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        std::string endpointString = _cut_request_path + "/" + _cut_full_url_key + "=" + longURL;
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, endpointString.c_str());

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        std::string response_data;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            std::cout << "Response: " << response_data << std::endl;
            return response_data;
        } else {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            return "";
        }

        // Cleanup
        curl_easy_cleanup(curl);
    } else {
        std::cerr << "Failed to initialize cURL" << std::endl;
    }
    return "";
}

std::shared_ptr<http_response> Server::cut_url_resource::render(const http_request& req) {

    auto arg = req.get_arg_flat(_cut_full_url_key);
    std::string longURL = std::string(arg);
    std::cout << "Received cut request: " << longURL << std::endl;
    if (raft.nodeType == Raft::NodeType::Leader) {
        std::pair<bool, std::string> res = raft.storage.cutLongUrl(longURL);
        std::string shortURL = res.second;
        if (res.first) {
            raft.createRequest(longURL, shortURL);
        }
        return std::shared_ptr<http_response>(new string_response(shortURL));
    } else {
        // forward
        std::string resp = forwardToLeader(raft.currentLeader, longURL);
        return std::make_shared<string_response>(resp);
    }
}

std::shared_ptr<http_response> Server::expand_url_resource::render(const http_request& req) {

    auto arg = req.get_arg_flat(_expand_full_url_key);
    std::string shortURL = std::string(arg);
    std::cout << "Received expand request: " << shortURL << std::endl;
    std::string resp = raft.storage.expandShortUrl(shortURL);
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