#pragma once

#include <vector>
#include <string>
#include "node/Node.h"
#include "RPC.h"
#include "storage/Storage.h"

class Raft {

public:

    enum NodeType : unsigned char {
        Follower = 0,
        Candidate = 1,
        Leader = 2
    };

    struct State {
        NodeType type = NodeType::Follower;
        size_t currentTerm = 0;
        int votedFor = -1;
        std::vector<struct LogEntry> log;
    };

    Raft();
    void run();

private:

    struct State state;
    Node node{};
    Storage storage;

    int receiveRPC(int socket, char* buffer);
    void handleRPC(char* buffer);
    void sendRPC(const std::string& data, const std::string& to);
    void runElection();
    void listenToRPCs(long timeout);
    void applyCommand(const Command& command);
    bool compareLogEntries(const LogEntry& first, const LogEntry& second);
    void loadPersistentState();

    void dumpStateToFile(const std::vector<struct LogEntry>& newEntries); 

};