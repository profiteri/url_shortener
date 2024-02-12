#include "Server.h"
#include "Log.h"

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void forwardToLeader(const std::string& leaderIP, const std::string& longURL, std::string& responseData) {

    std::cout << "Forwarding request to leader" << std::endl;

   if (leaderIP.empty()) {
        std::cerr << "Leader IP is not provided." << std::endl;
        responseData = "";
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize cURL" << std::endl;
        responseData = "";
        return;
    }

    std::string endpointString = leaderIP + _cut_request_path + "?" + _cut_full_url_key + "=" + longURL;
    std::cout << "Endpoint: " << endpointString << "\n";
    
    curl_easy_setopt(curl, CURLOPT_URL, endpointString.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        responseData = "";
        return;
    }
    
    std::cout << "curl successful, response data: " << responseData << std::endl;

    curl_easy_cleanup(curl);
    return;
}


std::shared_ptr<http_response> Server::cut_url_resource::render(const http_request& req) {

    Log::stat("Received CUT request");

    auto arg = req.get_arg_flat(_cut_full_url_key);
    std::string longURL = std::string(arg);
    std::cout << "Received cut request: " << longURL << std::endl;
    if (raft->nodeType.load() == Raft::NodeType::Leader) {
        std::pair<bool, std::string> res = raft->storage.cutLongUrl(longURL);
        std::string shortURL = res.second;
        if (res.first) {
            Log::stat("URL generated, sending a write request");
            if (raft->makeWriteRequest(longURL, res.second)) {
                std::cout << "  -   server side: makeWriteRequest returned true\n";
                Log::stat("Finished CUT request succesfull with write request");
                return std::shared_ptr<http_response>(new string_response(_expand_request_path + "?" + _expand_full_url_key + "=" + shortURL));
            }
            else {
                std::cout << "  -   server side: makeWriteRequest returned false\n";
                Log::stat("Finished CUT request unsuccesfull with write request");
                return std::shared_ptr<http_response>(new string_response(""));
            }
        }
        Log::stat("Finished CUT request succesfull without write request");
        return std::shared_ptr<http_response>(new string_response(_expand_request_path + "?" + _expand_full_url_key + "=" + shortURL));
    } else {
        // forward
        std::string responseData;
        forwardToLeader(raft->currentLeader, longURL, responseData);
        Log::stat("Finished CUT request with forwarding");
        return std::shared_ptr<http_response>(new string_response(responseData));
    }
}

std::shared_ptr<http_response> Server::expand_url_resource::render(const http_request& req) {

    Log::stat("Received EXPAND request");

    auto arg = req.get_arg_flat(_expand_full_url_key);
    std::string shortURL = std::string(arg);
    std::cout << "Received expand request: " << shortURL << std::endl;
    std::string resp = raft->storage.expandShortUrl(shortURL);

    Log::stat("Finished EXPAND request");
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