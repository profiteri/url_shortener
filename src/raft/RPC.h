#pragma once

#include <vector>
#include <string>


enum RPCType : unsigned char {
    appendEntries,
    appendEntriesResponse,
    requestVote,
    requestVoteResponse
};


struct Command {
    std::string longURL;
    std::string shortURL;
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
    size_t prevLogIndex = -1;
    size_t prevLogTerm = -1;
    size_t commitIndex = -1;
    // have to use vector since it's variable length
    std::vector<struct LogEntry> entries;

    bool sendRPC(int socket);

    void serialize(char*& buffer) const;

    void deserialize(const char*& buffer);

    size_t getDataSize();
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

    size_t getDataSize();
};


struct AppendEntriesResponse {

public:

    size_t term;
    bool success;

    bool sendRPC(int socket);

    void serialize(char*& buffer) const;

    void deserialize(const char*& buffer);

    size_t getDataSize();
};


struct RequestVoteResponse {

public:

    size_t term;
    bool voteGranted;

    bool sendRPC(int socket);

    void serialize(char*& buffer) const;

    void deserialize(const char*& buffer);

    size_t getDataSize();
};