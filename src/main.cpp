#include "http_server/Server.h"
#include "node/Node.h"
#include "storage/Storage.h"
#include "raft/Raft.h"

int main() {

    Raft r;
    r.run();
    return 0;

}