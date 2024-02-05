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