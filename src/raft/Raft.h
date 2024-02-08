#pragma once

#include <vector>
#include <string>
#include <optional>
#include <cstring>
#include <cerrno>
#include "node/Node.h"
#include "RPC.h"
#include "storage/Storage.h"

enum EventType { message, timeout };

using event = std::pair< 
    //                        msg,        sender IP
    EventType, std::optional<std::pair<std::string, std::string>>
    >;

class Raft {

public:

    enum NodeType : unsigned char {
        Follower = 0,
        Candidate = 1,
        Leader = 2
    };

    struct State {
        size_t currentTerm = 0;
        int votedFor = -1;
        std::vector<struct LogEntry> log;
    };

    Raft(Node& node, Storage& storage);
    void run();

    Node& node;
    Storage& storage;
    std::string currentLeader;
    NodeType nodeType = Follower;

    std::optional<struct Command> pendingWrite = std::nullopt;

private:

    struct State state;
    size_t prevCommitIndex;
    size_t receivedVotes = 1;

    const std::string stateFilename = "/space/state.txt";
    const std::string logFilename = "/space/log.txt";

    
    int receiveRPC(int socket, char* buffer);

    void handleFollowerRPC(const std::string& msg, const std::string& from);
    void handleCandidateRPC(const std::string& msg, const std::string& from);
    void handleLeaderRPC(const std::string& buffer);

    void handleAppendEntries(AppendEntries, const std::string& from);
    void handleRequestVote(RequestVote, const std::string& from);

    void sendRPC(char* data, const std::string& to);
    void runElection();
    event listenToRPCs(long timeout);

    void loadPersistentState();
    void applyCommand(const Command& command);

    bool compareLogEntries(size_t prevLogIndex, size_t prevLogTerm);

    void appendLogs(size_t prevLogIndex, const std::vector<struct LogEntry>& newEntries);

    void commitLogsToFile(size_t prevCommitIndex, size_t commitIndex);
    void commitStateToFile();

    void commitToStorage(size_t prevCommitIndex, size_t commitIndex);

};