#pragma once

#include <vector>
#include <string>
#include <optional>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <random>

#include "node/Node.h"
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

    struct Command {
        std::string longURL;
        std::string shortURL;
    };


    struct LogEntry {
        int term;
        int index;
        struct Command command;
    };   

    struct State {
        int currentTerm = 0;
        int votedFor = -1;
        std::vector<struct LogEntry> log;
    };

    Raft(Node& node, Storage& storage);
    void run();
    bool makeWriteRequest(const std::string& longUrl, const std::string& shortUrl);

    Node& node;
    Storage& storage;
    std::string currentLeader = "";
    std::atomic<NodeType> nodeType{NodeType::Follower};
    std::unordered_map<std::string, int> nextIndices;

private:

    struct State state;
    int prevCommitIndex = -1;

    const long listenTimeoutLowest = 400;
    const long listenTimeoutHighest = 500;

    const long heartbeatTimeout = 100;

    const std::string stateFilename = "/space/state.txt";
    const std::string logFilename = "/space/log.txt";

    int receivedVotes;

    mutable std::mutex mutex;

    int receiveRPC(int socket, char* buffer);

    void handleFollowerRPC(const std::string& msg, const std::string& from);
    void handleCandidateRPC(const std::string& msg, const std::string& from);
    void handleLeaderRPC(const std::string& msg, const std::string& from);

    void handleAppendEntries(ProtoAppendEntries, const std::string& from);
    void handleRequestVote(ProtoRequestVote, const std::string& from);

    void sendRPC(char* data, const std::string& to);
    void updateNextIndices();
    void runElection();
    std::vector<event> listenToRPCs(long timeout);

    inline ProtoAppendEntries constructAppendRPC(const std::string& receiverNode, std::optional<LogEntry> potentialNewEntryOpt);

    void loadPersistentState();
    void applyCommand(const Command& command);

    bool compareLogEntries(int prevLogIndex, int prevLogTerm);

    void appendLogs(int prevLogIndex, const ProtoAppendEntries& newEntries);

    void commitLogsToFile(int prevCommitIndex, int commitIndex);
    void commitStateToFile();

    void commitToStorage(int prevCommitIndex, int commitIndex);

    void printState();

};