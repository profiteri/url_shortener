#include "http_server/Server.h"
#include "node/Node.h"
#include "storage/Storage.h"
#include "raft/Raft.h"
#include "Log.h"

#include <thread>

int main() {

    Log::init();

    std::cout << "Starting the node...\n";

    Node node;
    std::cout << "Node created\n";

    Storage storage;
    std::cout << "Storage created\n";

    Raft raft(node, storage);
    std::cout << "Raft created\n";

    std::thread server_thread([&raft](){
        Server server(&raft);
        server.run();
    });
    server_thread.detach();
    raft.run();
    return 0;
}