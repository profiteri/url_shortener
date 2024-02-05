#pragma once

#include <vector>
#include <string>

class Raft {

public:

    enum Mode {
        shorten = 0,
        extend = 1
    };

    struct Command {
        enum Mode mode; 
        std::string key;
        std::string value;
    };

    struct LogEntry {
        size_t term;
        size_t index;
        struct Command command;
    };

    struct State {
        size_t currentTerm = 0;
        int votedFor = -1;
        std::vector<struct LogEntry> log;
    };

    struct RequestVote {
        int candidateId;
        size_t term;
        size_t lastLogIndex;
        size_t lastLogTerm;
    };

    struct AppendEntries {
        size_t term;
        int leaderId;
        size_t prevLogIndex;
        size_t prevLogTerm;
        // have to use vector since it's variable length
        std::vector<struct LogEntry> entries;
        size_t commitIndex;
    };

    Raft();

private:

    struct State state;
    void loadPersistentState();
    void dumpStateToFile(const std::vector<struct LogEntry>& newEntries);
};