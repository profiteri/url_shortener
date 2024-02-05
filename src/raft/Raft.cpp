#include "Raft.h"
#include <fstream>

void Raft::loadPersistentState() {
    size_t logSize;
    std::ifstream stateFile("/space/state.txt");
    if (stateFile.is_open()) {
        stateFile >> state.currentTerm;
        stateFile >> state.votedFor;

        stateFile >> logSize;

        stateFile.close();
    }
    std::ifstream logFile("/space/log.txt");
    if (logFile.is_open()) {
        for (size_t i = 0; i < logSize; ++i) {
            LogEntry entry;
            logFile >> entry.term;
            logFile >> entry.index;

            int mode;
            logFile >> mode;
            entry.command.mode = static_cast<Mode>(mode);

            logFile >> entry.command.key;
            logFile >> entry.command.value;

            state.log.push_back(entry);
        }
        logFile.close();
    }
}

void Raft::dumpStateToFile(const std::vector<struct LogEntry>& newEntries) {
    std::ofstream stateFile("/space/state.txt");

    if (stateFile.is_open()) {
        // Write the state information
        stateFile << state.currentTerm << " ";
        stateFile << state.votedFor << "\n";

        // Write the log size
        stateFile << (state.log.size() + newEntries.size()) << "\n";

        // Close the file
        stateFile.close();
    }

    std::ofstream logFile("/space/log.txt", std::ios::out | std::ios::app);

    if (logFile.is_open()) {
        // Write each log entry
        for (const auto& entry : newEntries) {
            logFile << entry.term << " ";
            logFile << entry.index << " ";
            logFile << static_cast<int>(entry.command.mode) << " ";
            logFile << entry.command.key << " ";
            logFile << entry.command.value << "\n";
        }

        // Close the file
        logFile.close();
    }
}


int Raft::receiveRPC(int socket, char* buffer) {
    int bytesReceived = recv(socket, buffer, sizeof(buffer), 0);
    
    if (bytesReceived <= 0) { 
        std::cout << "Didn't receive shit :(\n";
        return -1;
    } else {
        std::cout << "Received msg of size " << bytesReceived << '\n';
        return bytesReceived;
    }

}


void Raft::listenToRPCs() {
    for (const auto& pair : node.nodes) {
        char buffer[4096];
        if (receiveRPC(pair.second, buffer) == -1) {
            continue;
        }
        char type = buffer[0];
        const char* bufferPtr = &buffer[1];
        if (type == RPCType::appendEntries) {
            AppendEntries appendEntries;
            appendEntries.deserialize(bufferPtr);
        } else if (type == RPCType::requestVote) {
            RequestVote requestVote;
            requestVote.deserialize(bufferPtr);
        }
    }
}


Raft::Raft() {
    loadPersistentState();
}