#include "Raft.h"
#include <fstream>
#include <cerrno>
#include <chrono>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void Raft::loadPersistentState() {
    size_t logSize;
    std::ifstream stateFile(stateFilename);
    if (stateFile.is_open()) {
        stateFile >> state.currentTerm;
        stateFile >> state.votedFor;

        stateFile >> logSize;

        stateFile.close();
    }
    std::ifstream logFile(logFilename);
    if (logFile.is_open()) {
        for (size_t i = 0; i < logSize; ++i) {
            LogEntry entry;
            logFile >> entry.term;
            logFile >> entry.index;

            logFile >> entry.command.longURL;
            logFile >> entry.command.shortURL;

            applyCommand(entry.command);

            state.log.push_back(entry);
        }
        logFile.close();
    }
}


void Raft::applyCommand(const Command& command) {
    storage.insertURL(command.longURL, command.shortURL);
}


bool Raft::compareLogEntries(size_t prevLogIndex, size_t prevLogTerm) {
    if (prevLogIndex >= state.log.size()) {
        return false;
    }
    return state.log[prevLogIndex].term == prevLogTerm;
}


void Raft::appendLogs(size_t prevLogIndex, const std::vector<struct LogEntry> &newEntries) {
    state.log.insert(state.log.begin() + prevLogIndex + 1, newEntries.begin(), newEntries.end());
}


void Raft::commitLogsToFile(size_t prevCommitIndex, size_t commitIndex) {
    std::ofstream logFile(logFilename, std::ios::out | std::ios::app);

    if (logFile.is_open()) {
        for (size_t i = prevCommitIndex + 1; i <= commitIndex; i++) {
            const struct LogEntry& entry = state.log[i];
            logFile << entry.term << " ";
            logFile << entry.index << " ";

            logFile << entry.command.longURL << " ";
            logFile << entry.command.shortURL << "\n";
        }

        logFile.close();
    }
}


void Raft::commitStateToFile() {
    std::ofstream stateFile(stateFilename);
    if (stateFile.is_open()) {
        // Write the state information
        stateFile << state.currentTerm << " ";
        stateFile << state.votedFor << "\n";

        stateFile.close();
    }
}


void Raft::commitToStorage(size_t prevCommitIndex, size_t commitIndex) {
    for (size_t i = prevCommitIndex + 1; i <= commitIndex; i++) {
        const struct LogEntry& entry = state.log[i];
        applyCommand(entry.command);
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

void Raft::handleFollowerRPC(const std::string& msg, const std::string& from) {

    unsigned char type = msg[0];
    const char* bufferPtr = &msg[1];

    if (type == RPCType::appendEntries) {

        AppendEntries rpc;
        rpc.deserialize(bufferPtr);

        handleAppendEntries(rpc, from);

    } else if (type == RPCType::requestVote) {

        RequestVote rpc;
        rpc.deserialize(bufferPtr);

        //not vote
        if (rpc.term < state.currentTerm) {

            RequestVoteResponse response = {
                .term = state.currentTerm,
                .voteGranted = false
            };
            char msg[response.getDataSize()];
            msg[0] = RPCType::requestVoteResponse;
            char* ptr = &msg[1];
            response.serialize(ptr);
            sendRPC(msg, from);
            return;            

        }

        handleRequestVote(rpc, from);

    } else std::cerr << "Unrecognized type" << "\n";
}

void Raft::handleCandidateRPC(const std::string& msg, const std::string& from) {
    unsigned char type = msg[0];
    const char* bufferPtr = &msg[1];
    switch (type) {
        case RPCType::requestVoteResponse: {
            RequestVoteResponse requestVoteResponse;
            requestVoteResponse.deserialize(bufferPtr);
            if (requestVoteResponse.voteGranted) {
                receivedVotes++;
                if (receivedVotes > node.numNodes / 2) {
                    nodeType = NodeType::Leader;
                }
            } else if (requestVoteResponse.term > state.currentTerm) {
                state.currentTerm = requestVoteResponse.term;
                nodeType = NodeType::Follower;
            }
            break;
        }
    
        case RPCType::appendEntries: {
            AppendEntries appendEntries;
            appendEntries.deserialize(bufferPtr);
            nodeType = NodeType::Follower;
            handleAppendEntries(appendEntries, from);
            break;
        }

        case RPCType::requestVote: {
            RequestVote requestVote;
            requestVote.deserialize(bufferPtr);

            //not vote
            if (requestVote.term <= state.currentTerm) {
                RequestVoteResponse response = {
                    .term = state.currentTerm,
                    .voteGranted = false
                };
                char msg[response.getDataSize()];
                msg[0] = RPCType::requestVoteResponse;
                char* ptr = &msg[1];
                response.serialize(ptr);
                sendRPC(msg, from);
                return;
            }
            nodeType = NodeType::Follower;
            handleRequestVote(requestVote, from);
        }
    }
}

void Raft::handleLeaderRPC(const std::string& buffer) {
    unsigned char type = buffer[0];
    const char* bufferPtr = &buffer[1];
    if (type == RPCType::appendEntries) {
        AppendEntries appendEntries;
        appendEntries.deserialize(bufferPtr);
        if (appendEntries.entries.empty()) {
            // heartbeat
            // update time of last heartbeat`
        }
        // ...
    } else if (type == RPCType::appendEntriesResponse) {
        AppendEntriesResponse appendEntriesResponse;
        appendEntriesResponse.deserialize(bufferPtr);
        // ...
    } else if (type == RPCType::requestVote) {
        RequestVote requestVote;
        requestVote.deserialize(bufferPtr);
        // ...
    } else if (type == RPCType::requestVoteResponse) {
        RequestVoteResponse requestVoteResponse;
        requestVoteResponse.deserialize(bufferPtr);
        // ...
    }
}

void Raft::handleAppendEntries(AppendEntries rpc, const std::string &from) {

    if (rpc.term < state.currentTerm) return;

    else if (rpc.term > state.currentTerm) {
        state.currentTerm = rpc.term;
        state.votedFor = -1;
    }

    if (!compareLogEntries(rpc.prevLogIndex, rpc.prevLogTerm)) {

        //send fail
        AppendEntriesResponse resp;
        resp.term = state.currentTerm;
        resp.success = false;
        
        char msg[resp.getDataSize()];
        msg[0] = RPCType::appendEntriesResponse;
        char* ptr = &msg[1];
        resp.serialize(ptr);
        sendRPC(msg, from);
        return;

    }

    //delete & append
    appendLogs(rpc.prevLogIndex, rpc.entries);

    //advance state machine
    commitLogsToFile(prevCommitIndex, rpc.commitIndex);
    commitStateToFile();
    commitToStorage(prevCommitIndex, rpc.commitIndex);

    //send success
    AppendEntriesResponse resp;
    resp.term = state.currentTerm;
    resp.success = true;

    char msg[resp.getDataSize()];
    msg[0] = RPCType::appendEntriesResponse;
    char* ptr = &msg[1];
    resp.serialize(ptr);
    sendRPC(msg, from);
    return;

}

void Raft::handleRequestVote(RequestVote rpc, const std::string &from) {

    if (rpc.term > state.currentTerm) {
        state.currentTerm = rpc.term;
        state.votedFor = -1;
    }

    RequestVoteResponse resp; 
    resp.term = state.currentTerm;   

    resp.voteGranted = (
        (state.votedFor == -1 || state.votedFor == rpc.candidateId)
        &&
        (rpc.lastLogIndex >= state.log.back().index && rpc.lastLogTerm >= state.log.back().term)
    ); 

    char msg[resp.getDataSize()];
    msg[0] = RPCType::requestVoteResponse;
    char* ptr = &msg[1];
    resp.serialize(ptr);
    sendRPC(msg, from);

}

void Raft::sendRPC(char* data, const std::string& to) {

    if (node.writeSockets.count(to) == 0) {
        std::cout << "Try to connected to: " << to << '\n';
        if (node.connectToNode(to) == -1) {
            std::cerr << "Failed to send. Couldn't connect to: " << to << '\n';
            return;
        }
    }

    std::cout << "Try send to: " << to << '\n';
    if (send(node.writeSockets[to], data, strlen(data), 0) > 0) {
        std::cout << "Sent successful\n";
    } else {
        std::cout << "Error sending msg\n";
    }

}

void Raft::runElection() {
    receivedVotes = 1;
    state.currentTerm++;
    state.votedFor = node.id;
    RequestVote requestVote = {
        .candidateId = node.id,
        .term = state.currentTerm,
        .lastLogIndex = state.log.back().index,
        .lastLogTerm = state.log.back().term
    };
    for (const std::string& nodeAddress : node.nodeAddresses) {
        char msg[requestVote.getDataSize()];
        msg[0] = RPCType::appendEntriesResponse;
        char* ptr = &msg[1];
        requestVote.serialize(ptr);
        sendRPC(msg, nodeAddress);        
    }
}

// <msg, sender IP>
event Raft::listenToRPCs(long timeout) {

    int master_socket = node.listeningSocket;

    //set of socket descriptors  
    fd_set readfds;
             
    auto start = std::chrono::steady_clock::now();
    while(true) {

        //clear the socket set  
        FD_ZERO(&readfds);
     
        //add my socket to set  
        FD_SET(master_socket, &readfds);   
        int max_sd = master_socket;   
             
        //add nodes sockets to set 
        for (const auto& pair : node.readSockets) {   
            //add to read list  
            FD_SET(pair.second, &readfds);   
                 
            //highest file descriptor number, need it for the select function  
            if (pair.second > max_sd) max_sd = pair.second;   
        }

        auto elapsed  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Elapsed: " << elapsed << '\n';
        if (elapsed >= timeout) {
            std::cout << "Timeout expired\n";
            return {EventType::timeout, std::nullopt};
        }

        struct timeval tv;
        tv.tv_sec = (timeout - elapsed) / 1000;
        tv.tv_usec = ((timeout - elapsed) % 1000) * 1000;

        std::cout << "Seconds left: " << tv.tv_sec << '\n';

        //wait for an activity on one of the sockets
        std::cout << "Waiting for activity...\n";
        int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);   
       
        if (activity == -1) { 
            std::cerr << "Select error: " << errno << " " << std::strerror(errno) << '\n';
        }
        else if (activity == 0) {
            std::cout << "Timeout expired\n";
            return {EventType::timeout, std::nullopt};
        }
             
        //If something happened on my socket,  
        //then its an incoming connection
        if(FD_ISSET(master_socket, &readfds)) {
            node.acceptConnection();
            continue;
        }   
             
        //else its some IO operation on some other socket 
        for (auto it = node.readSockets.begin(); it != node.readSockets.end(); ) {
                 
            if (FD_ISSET(it->second, &readfds)) {
                
                std::vector<char> buffer(4096);
                int bytesReceived = recv(it->second, &buffer[0], buffer.size(), 0);

                if (bytesReceived == 0) { //someone disconnected
                    std::cout << "Node disconnected: " << it->first << " on socket: " << it->second << '\n';
                    close(it->second);
                    it = node.readSockets.erase(it);
                } 
                else if (bytesReceived < 0){ // some error  
                    std::cerr << "Some error on socket: " << std::strerror(errno) << '\n';
                    ++it;
                }
                else { //message received
                    std::pair p = {std::string{buffer.begin(), buffer.end()}, it->first};
                    return {EventType::message, {p}};
                    ++it;
                }
            }   
        }
    }
}

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> distr(50, 250); // milliseconds

void Raft::run() {

    while(true) {
        
        switch (nodeType) {

            case NodeType::Follower: {

                std::cout << "Listen to heartbits\n";            

                //setup heartbeats timeout (sec), after which node runs election
                long timeout = static_cast<long>(distr(gen));
                event e = listenToRPCs(timeout);

                switch (e.first) {

                    case EventType::timeout: {
                        nodeType = NodeType::Candidate;
                        break;
                    }

                    case EventType::message: {
                        auto p = e.second.value();
                        std::cout << "Got msg: " << p.first << '\n';
                        handleFollowerRPC(p.first, p.second);
                        break;
                    }
                }
                break;
            }

            case NodeType::Candidate: {

                std::cout << "Start election\n";

                runElection();

                long timeout = static_cast<long>(distr(gen));
                event e = listenToRPCs(timeout);

                switch (e.first) {

                    case EventType::timeout: {
                        continue;
                    }

                    case EventType::message: {
                        auto p = e.second.value();
                        std::cout << "Got msg: " << p.first << '\n';
                        handleCandidateRPC(p.first, p.second);
                        break;
                    }
                }
                break;
            }

            case NodeType::Leader: {
                //TODO
            }

        }
    }
}


Raft::Raft() {
    loadPersistentState();
}