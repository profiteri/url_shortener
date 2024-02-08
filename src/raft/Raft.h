#pragma once

#include <vector>
#include <string>
#include <optional>
#include <cstring>
#include <cerrno>
#include "node/Node.h"
#include "RPC.h"
#include "storage/Storage.h"
#include "requests.pb.h"

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
        int currentTerm = 0;
        int votedFor = -1;
        std::vector<struct LogEntry> log;
    };

    struct WriteRequest {
        Raft& raft;
        std::unordered_set<std::string> pendingNodes;
        struct Command command;

        WriteRequest(Raft& raft, std::string longURL, std::string shortURL) : raft(raft) {
            for (const auto& pair : raft.nextIndices) {
                pendingNodes.insert(pair.first);
            }
            command.longURL = longURL;
            command.shortURL = shortURL;
            raft.updateNextIndices();
            raft.state.log.emplace_back(raft.state.currentTerm, raft.state.log.size(), command);
        }
    };

    Raft(Node& node, Storage& storage);
    void run();

    Node& node;
    Storage& storage;
    std::string currentLeader;
    NodeType nodeType = Follower;

    std::optional<struct WriteRequest> pendingWrite = std::nullopt;

    std::unordered_map<std::string, int> nextIndices;

private:

    struct State state;
    int prevCommitIndex = -1;
    int receivedVotes = 1;

    const std::string stateFilename = "/space/state.txt";
    const std::string logFilename = "/space/log.txt";

    
    int receiveRPC(int socket, char* buffer);

    void handleFollowerRPC(const std::string& msg, const std::string& from);
    void handleCandidateRPC(const std::string& msg, const std::string& from);
    void handleLeaderRPC(const std::string& msg, const std::string& from);

    void handleAppendEntries(ProtoAppendEntries, const std::string& from);
    void handleRequestVote(ProtoRequestVote, const std::string& from);

    void sendRPC(char* data, const std::string& to);
    void updateNextIndices();
    void runElection();
    event listenToRPCs(long timeout);

    void loadPersistentState();
    void applyCommand(const Command& command);

    bool compareLogEntries(int prevLogIndex, int prevLogTerm);

    void appendLogs(int prevLogIndex, const ProtoAppendEntries& newEntries);

    void commitLogsToFile(int prevCommitIndex, int commitIndex);
    void commitStateToFile();

    void commitToStorage(int prevCommitIndex, int commitIndex);

};