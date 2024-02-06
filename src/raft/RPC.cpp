#include "RPC.h"
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>


void AppendEntries::serialize(char*& buffer) const {
    // Serialize each member individually
    size_t termNet = htonl(term);
    memcpy(buffer, &termNet, sizeof(size_t));
    buffer += sizeof(size_t);

    int leaderIdNet = htonl(leaderId);
    memcpy(buffer, &leaderIdNet, sizeof(int));
    buffer += sizeof(int);

    size_t prevLogIndexNet = htonl(prevLogIndex);
    memcpy(buffer, &prevLogIndexNet, sizeof(size_t));
    buffer += sizeof(size_t);

    size_t prevLogTermNet = htonl(prevLogTerm);
    memcpy(buffer, &prevLogTermNet, sizeof(size_t));
    buffer += sizeof(size_t);

    size_t commitIndexNet = htonl(commitIndex);
    memcpy(buffer, &commitIndexNet, sizeof(size_t));
    buffer += sizeof(size_t);

    size_t numEntries = entries.size();
    size_t numEntriesNet = htonl(numEntries);
    memcpy(buffer, &numEntriesNet, sizeof(size_t));
    buffer += sizeof(size_t);

    // Serialize each LogEntry in the vector
    for (const auto& logEntry : entries) {
        size_t termNetEntry = htonl(logEntry.term);
        memcpy(buffer, &termNetEntry, sizeof(size_t));
        buffer += sizeof(size_t);

        size_t indexNetEntry = htonl(logEntry.index);
        memcpy(buffer, &indexNetEntry, sizeof(size_t));
        buffer += sizeof(size_t);

        // Serialize the Command struct
        int modeNet = htonl(static_cast<int>(logEntry.command.mode));
        memcpy(buffer, &modeNet, sizeof(int));
        buffer += sizeof(int);

        // Serialize the key string
        size_t keyLength = logEntry.command.key.length();
        size_t keyLengthNet = htonl(keyLength);
        memcpy(buffer, &keyLengthNet, sizeof(size_t));
        buffer += sizeof(size_t);
        memcpy(buffer, logEntry.command.key.c_str(), keyLength);
        buffer += keyLength;

        // Serialize the value string
        size_t valueLength = logEntry.command.value.length();
        size_t valueLengthNet = htonl(valueLength);
        memcpy(buffer, &valueLengthNet, sizeof(size_t));
        buffer += sizeof(size_t);
        memcpy(buffer, logEntry.command.value.c_str(), valueLength);
        buffer += valueLength;
    }
}


void AppendEntries::deserialize(const char*& buffer) {
    // Deserialize each member individually
    size_t termNet;
    memcpy(&termNet, buffer, sizeof(size_t));
    term = ntohl(termNet);
    buffer += sizeof(size_t);

    int leaderIdNet;
    memcpy(&leaderIdNet, buffer, sizeof(int));
    leaderId = ntohl(leaderIdNet);
    buffer += sizeof(int);

    size_t prevLogIndexNet;
    memcpy(&prevLogIndexNet, buffer, sizeof(size_t));
    prevLogIndex = ntohl(prevLogIndexNet);
    buffer += sizeof(size_t);

    size_t prevLogTermNet;
    memcpy(&prevLogTermNet, buffer, sizeof(size_t));
    prevLogTerm = ntohl(prevLogTermNet);
    buffer += sizeof(size_t);

    size_t commitIndexNet;
    memcpy(&commitIndexNet, buffer, sizeof(size_t));
    commitIndex = ntohl(commitIndexNet);
    buffer += sizeof(size_t);

    size_t numEntriesNet;
    memcpy(&numEntriesNet, buffer, sizeof(size_t));
    size_t numEntries = ntohl(numEntriesNet);
    buffer += sizeof(size_t);

    // Deserialize each LogEntry in the vector
    entries.resize(numEntries);
    for (auto& logEntry : entries) {
        size_t termNetEntry;
        memcpy(&termNetEntry, buffer, sizeof(size_t));
        logEntry.term = ntohl(termNetEntry);
        buffer += sizeof(size_t);

        size_t indexNetEntry;
        memcpy(&indexNetEntry, buffer, sizeof(size_t));
        logEntry.index = ntohl(indexNetEntry);
        buffer += sizeof(size_t);

        // Deserialize the Command struct
        int modeNet;
        memcpy(&modeNet, buffer, sizeof(int));
        logEntry.command.mode = static_cast<Mode>(ntohl(modeNet));
        buffer += sizeof(int);

        // Deserialize the key string
        size_t keyLengthNet;
        memcpy(&keyLengthNet, buffer, sizeof(size_t));
        size_t keyLength = ntohl(keyLengthNet);
        buffer += sizeof(size_t);
        logEntry.command.key.assign(buffer, keyLength);
        buffer += keyLength;

        // Deserialize the value string
        size_t valueLengthNet;
        memcpy(&valueLengthNet, buffer, sizeof(size_t));
        size_t valueLength = ntohl(valueLengthNet);
        buffer += sizeof(size_t);
        logEntry.command.value.assign(buffer, valueLength);
        buffer += valueLength;
    }
}

bool AppendEntries::sendRPC(int socket) {
    // Calculate the size needed for serialization
    size_t dataSize = sizeof(size_t) + sizeof(int) + sizeof(size_t) + sizeof(size_t) +
                      sizeof(size_t) + sizeof(size_t);  // Base size for AppendEntries
    // Add size for each LogEntry
    for (const auto& logEntry : entries) {
        dataSize += sizeof(size_t) + sizeof(size_t) + sizeof(Mode) +
                    sizeof(size_t) + sizeof(size_t) +
                    logEntry.command.key.size() + 1 +
                    logEntry.command.value.size() + 1;
    }
    // Create a buffer to hold the serialized data
    char buffer[dataSize];
    // Serialize the data into the buffer
    buffer[0] = RPCType::appendEntries;
    char* bufferPtr = &buffer[1];
    serialize(bufferPtr);
    // Send the buffer over the socket
    ssize_t sentBytes = send(socket, buffer, dataSize, 0);
    return sentBytes == static_cast<ssize_t>(dataSize);
}

void RequestVote::serialize(char*& buffer) const {
    // Serialize each member individually
    int candidateIdNet = htonl(candidateId);
    memcpy(buffer, &candidateIdNet, sizeof(int));
    buffer += sizeof(int);

    size_t termNet = htonl(term);
    memcpy(buffer, &termNet, sizeof(size_t));
    buffer += sizeof(size_t);

    size_t lastLogIndexNet = htonl(lastLogIndex);
    memcpy(buffer, &lastLogIndexNet, sizeof(size_t));
    buffer += sizeof(size_t);

    size_t lastLogTermNet = htonl(lastLogTerm);
    memcpy(buffer, &lastLogTermNet, sizeof(size_t));
    buffer += sizeof(size_t);
}


void RequestVote::deserialize(const char*& buffer) {
    // Deserialize each member individually
    int candidateIdNet;
    memcpy(&candidateIdNet, buffer, sizeof(int));
    candidateId = ntohl(candidateIdNet);
    buffer += sizeof(int);

    size_t termNet;
    memcpy(&termNet, buffer, sizeof(size_t));
    term = ntohl(termNet);
    buffer += sizeof(size_t);

    size_t lastLogIndexNet;
    memcpy(&lastLogIndexNet, buffer, sizeof(size_t));
    lastLogIndex = ntohl(lastLogIndexNet);
    buffer += sizeof(size_t);

    size_t lastLogTermNet;
    memcpy(&lastLogTermNet, buffer, sizeof(size_t));
    lastLogTerm = ntohl(lastLogTermNet);
    buffer += sizeof(size_t);
}

bool RequestVote::sendRPC(int socket) {
    // Calculate the size needed for serialization
    size_t dataSize = sizeof(int) + sizeof(size_t) + sizeof(size_t) + sizeof(size_t);

    // Create a buffer to hold the serialized data
    char buffer[dataSize];
    // Serialize the data into the buffer
    buffer[0] = RPCType::requestVote;
    char* bufferPtr = &buffer[1];
    serialize(bufferPtr);
    // Send the buffer over the socket
    ssize_t sentBytes = send(socket, buffer, dataSize, 0);
    return sentBytes == static_cast<ssize_t>(dataSize);
}


void AppendEntriesResponse::serialize(char*& buffer) const {
    // Serialize each member individually
    size_t termNet = htonl(term);
    memcpy(buffer, &termNet, sizeof(size_t));
    buffer += sizeof(size_t);

    uint8_t successAsUint8 = static_cast<uint8_t>(success);
    uint32_t successNet = htonl(successAsUint8);
    memcpy(buffer, &successNet, sizeof(uint32_t));
    buffer += sizeof(uint32_t);
}


void AppendEntriesResponse::deserialize(const char*& buffer) {
    // Deserialize each member individually
    size_t termNet;
    memcpy(&termNet, buffer, sizeof(size_t));
    term = ntohl(termNet);
    buffer += sizeof(size_t);

    uint32_t successNet;
    memcpy(&successNet, buffer, sizeof(uint32_t));
    uint8_t successAsUint8 = ntohl(successNet);
    success = static_cast<bool>(successAsUint8);
    buffer += sizeof(uint32_t);
}

bool AppendEntriesResponse::sendRPC(int socket) {
    // Calculate the size needed for serialization
    size_t dataSize = sizeof(size_t) + sizeof(uint32_t);

    // Create a buffer to hold the serialized data
    char buffer[dataSize];
    // Serialize the data into the buffer
    buffer[0] = RPCType::appendEntriesResponse;
    char* bufferPtr = &buffer[1];
    serialize(bufferPtr);
    // Send the buffer over the socket
    ssize_t sentBytes = send(socket, buffer, dataSize, 0);
    return sentBytes == static_cast<ssize_t>(dataSize);
}


void RequestVoteResponse::serialize(char*& buffer) const {
    // Serialize each member individually
    size_t termNet = htonl(term);
    memcpy(buffer, &termNet, sizeof(size_t));
    buffer += sizeof(size_t);

    uint8_t voteGrantedAsUint8 = static_cast<uint8_t>(voteGranted);
    uint32_t voteGrantedNet = htonl(voteGrantedAsUint8);
    memcpy(buffer, &voteGrantedNet, sizeof(uint32_t));
    buffer += sizeof(uint32_t);
}


void RequestVoteResponse::deserialize(const char*& buffer) {
    // Deserialize each member individually
    size_t termNet;
    memcpy(&termNet, buffer, sizeof(size_t));
    term = ntohl(termNet);
    buffer += sizeof(size_t);

    uint32_t voteGrantedNet;
    memcpy(&voteGrantedNet, buffer, sizeof(uint32_t));
    uint8_t voteGrantedAsUint8 = ntohl(voteGrantedNet);
    voteGranted = static_cast<bool>(voteGrantedAsUint8);
    buffer += sizeof(uint32_t);
}

bool RequestVoteResponse::sendRPC(int socket) {
    // Calculate the size needed for serialization
    size_t dataSize = sizeof(size_t) + sizeof(uint32_t);

    // Create a buffer to hold the serialized data
    char buffer[dataSize];
    // Serialize the data into the buffer
    buffer[0] = RPCType::requestVoteResponse;
    char* bufferPtr = &buffer[1];
    serialize(bufferPtr);
    // Send the buffer over the socket
    ssize_t sentBytes = send(socket, buffer, dataSize, 0);
    return sentBytes == static_cast<ssize_t>(dataSize);
}