#pragma once

#include <vector>
#include <string>


enum RPCType : unsigned char {
    appendEntries,
    appendEntriesResponse,
    requestVote,
    requestVoteResponse
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


struct AppendEntries {

public:

    size_t term;
    int leaderId;
    size_t prevLogIndex;
    size_t prevLogTerm;
    size_t commitIndex;
    // have to use vector since it's variable length
    std::vector<struct LogEntry> entries;

    bool sendRPC(int socket);

    void serialize(char*& buffer) const;

    void deserialize(const char*& buffer);
};


struct RequestVote {

public:

    int candidateId;
    size_t term;
    size_t lastLogIndex;
    size_t lastLogTerm;

    bool sendRPC(int socket);

    void serialize(char*& buffer) const;

    void deserialize(const char*& buffer);
};


struct AppendEntriesResponse {

public:

    size_t term;
    bool success;

    bool sendRPC(int socket);

    void serialize(char*& buffer) const;

    void deserialize(const char*& buffer);
};


struct RequestVoteResponse {

public:

    size_t term;
    bool voteGranted;

    bool sendRPC(int socket);

    void serialize(char*& buffer) const;

    void deserialize(const char*& buffer);
};