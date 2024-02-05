#pragma once

#include <vector>
#include <string>


enum RPCType : unsigned char {
    appendEntries = 1,
    requestVote = 2
};

enum Mode : unsigned char {
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


struct RequestVote {
    int candidateId;
    size_t term;
    size_t lastLogIndex;
    size_t lastLogTerm;
};

struct AppendEntries {

public:

    size_t term;
    int leaderId;
    size_t prevLogIndex;
    size_t prevLogTerm;
    size_t commitIndex;
    // have to use vector since it's variable length
    std::vector<struct LogEntry> entries;

    void serialize(char*& buffer) const;

    void deserialize(const char*& buffer);
};