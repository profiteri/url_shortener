#include "http_server/Server.h"
#include "node/Node.h"
#include "storage/Storage.h"
#include "raft/Raft.h"
#include <thread>

int main() {
    Node node;
    Storage storage;
    Raft raft(node, storage);
    Server server(raft);
    std::thread server_thread([&server](){
        server.run();
    });
    server_thread.detach();
    raft.run();
    return 0;

}