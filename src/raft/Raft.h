#pragma once

#include <vector>
#include <string>
#include "node/Node.h"
#include "RPC.h"

class Raft {

public:

    struct State {
        size_t currentTerm = 0;
        int votedFor = -1;
        std::vector<struct LogEntry> log;

        State() {
            loadPersistentState();
        }

        void loadPersistentState();

        void dumpStateToFile(const std::vector<struct LogEntry>& newEntries); 
    };

    Raft();

    Node node;

private:

    struct State state;
    int receiveRPC(int socket, char* buffer);
    void listenToRPCs();

};