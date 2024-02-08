#include "Raft.h"
#include "google/protobuf/any.pb.h"

#include <fstream>
#include <cerrno>
#include <chrono>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <random>

template <typename T>
static std::string packToAny(T msg) {
    google::protobuf::Any any;
    any.PackFrom(msg);

    std::cout << "  -   serialized: " << any.DebugString() << "\n";

    std::string data;
    any.SerializeToString(&data);
    return data;
}

void Raft::loadPersistentState() {
    int logSize;
    std::ifstream stateFile(stateFilename);
    if (stateFile.is_open()) {
        stateFile >> state.currentTerm;
        stateFile >> state.votedFor;

        stateFile >> logSize;

        stateFile.close();
    }
    std::ifstream logFile(logFilename);
    if (logFile.is_open()) {
        for (int i = 0; i < logSize; ++i) {
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


bool Raft::compareLogEntries(int prevLogIndex, int prevLogTerm) {
    if (prevLogIndex == -1) {
        return true;
    }
    if (prevLogIndex >= static_cast<int>(state.log.size())) {
        return false;
    }
    return state.log[prevLogIndex].term == prevLogTerm;
}


void Raft::appendLogs(int prevLogIndex, const ProtoAppendEntries& proto_entries) {

    std::vector<struct LogEntry> newEntries;

    for (const auto& proto_log : proto_entries.entries()) {

        LogEntry log;

        log.term = proto_log.term();
        log.index = proto_log.index();

        log.command.longURL = proto_log.command().longurl();
        log.command.shortURL = proto_log.command().shorturl();

        newEntries.push_back(log);

    }

    state.log.insert(state.log.begin() + prevLogIndex + 1, newEntries.begin(), newEntries.end());
}


void Raft::commitLogsToFile(int prevCommitIndex, int commitIndex) {
    std::ofstream logFile(logFilename, std::ios::out | std::ios::app);

    if (logFile.is_open()) {
        for (int i = prevCommitIndex + 1; i <= commitIndex; i++) {
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


void Raft::commitToStorage(int prevCommitIndex, int commitIndex) {
    for (int i = prevCommitIndex + 1; i <= commitIndex; i++) {
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

    ProtoAppendEntries ae;
    ProtoRequestVote rv;

    google::protobuf::Any any;
    any.ParseFromString(msg); 

    std::cout << "  -   deserialized: " << any.DebugString() << "\n";

    if (any.UnpackTo(&ae)) {

        handleAppendEntries(ae, from);

    } else if (any.UnpackTo(&rv)) {

        //not vote
        if (rv.term() < state.currentTerm) {

            ProtoRequestVoteResponse response;
            response.set_term(state.currentTerm);
            response.set_votegranted(false);

            sendRPC(packToAny(response).data(), from);
            return;            

        }

        handleRequestVote(rv, from);

    } else std::cerr << "Unrecognized type" << "\n";
}

void Raft::handleCandidateRPC(const std::string& msg, const std::string& from) {
    
    ProtoRequestVoteResponse rv_resp;
    ProtoAppendEntries ae;
    ProtoRequestVote rv;

    google::protobuf::Any any;
    any.ParseFromString(msg);

    std::cout << "  -   deserialized: " << any.DebugString() << "\n";

    if (any.UnpackTo(&rv_resp))
        {    
            if (rv_resp.votegranted()) {
                receivedVotes++;
                if (receivedVotes > node.numNodes / 2) {
                    nodeType = NodeType::Leader;
                }
            } else if (rv_resp.term() > state.currentTerm) {
                state.currentTerm = rv_resp.term();
                nodeType = NodeType::Follower;
                commitStateToFile();
            }
        }
    
    else if (any.UnpackTo(&ae))
        {
            nodeType = NodeType::Follower;
            handleAppendEntries(ae, from);
        }

    else if (any.UnpackTo(&rv))
        {
            //not vote
            if (rv.term() <= state.currentTerm) {

                ProtoRequestVoteResponse response;
                response.set_term(state.currentTerm);
                response.set_votegranted(false);
                
                sendRPC(packToAny(response).data(), from);
                return;
            }
            nodeType = NodeType::Follower;
            handleRequestVote(rv, from);
        }
}

void Raft::handleLeaderRPC(const std::string& msg, const std::string& from) {

    ProtoAppendEntriesResponse ae_resp;
    ProtoAppendEntries ae;
    ProtoRequestVote rv;

    google::protobuf::Any any;
    any.ParseFromString(msg);

    std::cout << "  -   deserialized: " << any.DebugString() << "\n";

    if (any.UnpackTo(&ae_resp))
        {            
            if (state.currentTerm < ae_resp.term()) {
                state.currentTerm = ae_resp.term();
                nodeType = NodeType::Follower;
                commitStateToFile();
                return;
            }
            auto& pendingNodes = pendingWrite.value().pendingNodes;
            if (ae_resp.success()) {
                if (pendingNodes.find(from) != pendingNodes.end()) {
                    pendingNodes.erase(from);
                    ++nextIndices[from];
                    if (node.numNodes - static_cast<int>(pendingNodes.size()) > node.numNodes / 2) {
                        commitLogsToFile(prevCommitIndex, prevCommitIndex + 1);
                        commitStateToFile();
                        commitToStorage(prevCommitIndex, prevCommitIndex + 1);
                        prevCommitIndex++;
                        pendingWrite = std::nullopt;
                    }
                }
            } else {
                --nextIndices[from];
            }
        }
    
    if (any.UnpackTo(&ae))
        {
            nodeType = NodeType::Follower;
            handleAppendEntries(ae, from);
        }

    if (any.UnpackTo(&rv))
        {
            //not vote
            if (rv.term() <= state.currentTerm) {
                ProtoRequestVoteResponse response;
                response.set_term(state.currentTerm);
                response.set_votegranted(false);
                sendRPC(packToAny(response).data(), from);
                return;
            }
            nodeType = NodeType::Follower;
            handleRequestVote(rv, from);
        }
}

void Raft::handleAppendEntries(ProtoAppendEntries rpc, const std::string &from) {

    if (rpc.term() < state.currentTerm) return;

    else if (rpc.term() > state.currentTerm) {
        state.currentTerm = rpc.term();
        state.votedFor = -1;
    }

    if (!compareLogEntries(rpc.prevlogindex(), rpc.prevlogterm())) {

        //send fail
        ProtoAppendEntriesResponse resp;
        resp.set_term(state.currentTerm);
        resp.set_success(false);

        sendRPC(packToAny(resp).data(), from);
        return;
    }

    //delete & append
    appendLogs(rpc.prevlogindex(), rpc);

    //advance state machine
    commitLogsToFile(prevCommitIndex, rpc.commitindex());
    commitStateToFile();
    commitToStorage(prevCommitIndex, rpc.commitindex());

    //send success
    ProtoAppendEntriesResponse resp;
    resp.set_term(state.currentTerm);
    resp.set_success(true);

    sendRPC(packToAny(resp).data(), from);
    return;
}

void Raft::handleRequestVote(ProtoRequestVote rpc, const std::string &from) {

    if (rpc.term() > state.currentTerm) {
        state.currentTerm = rpc.term();
        state.votedFor = -1;
    }

    ProtoRequestVoteResponse resp; 
    resp.set_term(state.currentTerm);   
    resp.set_votegranted(
        (state.votedFor == -1 || state.votedFor == rpc.candidateid())
        &&
        (rpc.lastlogindex() >= (!state.log.empty() ? state.log.back().index : -1) && rpc.lastlogterm() >= (!state.log.empty() ? state.log.back().term : -1))
    );

    if (resp.votegranted()) {
        state.votedFor = rpc.candidateid();
    }

    commitStateToFile();

    sendRPC(packToAny(resp).data(), from);

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
    int logSize = state.log.size();

    ProtoRequestVote requestVote;
    requestVote.set_term(state.currentTerm);
    requestVote.set_candidateid(node.id);
    requestVote.set_lastlogindex(logSize ? state.log.back().index : -1);
    requestVote.set_lastlogterm(logSize ? state.log.back().term : -1);

    for (const std::string& nodeAddress : node.nodeAddresses) {
        sendRPC(packToAny(requestVote).data(), nodeAddress);        
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
std::uniform_int_distribution<> distr(5000, 6000); // milliseconds

void Raft::run() {

    while(true) {
        
        switch (nodeType) {

            case NodeType::Follower: {

                std::cout << "*******************\n";
                std::cout << "Listen to heartbeats\n";
                std::cout << "*******************\n";

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
                        std::cout << "  -   got msg as follower" << '\n';
                        handleFollowerRPC(p.first, p.second);
                        break;
                    }
                }
                break;
            }

            case NodeType::Candidate: {

                std::cout << "**************\n";
                std::cout << "Start election\n";
                std::cout << "**************\n";

                runElection();

                long timeout = static_cast<long>(distr(gen));
                event e = listenToRPCs(timeout);

                switch (e.first) {

                    case EventType::timeout: {
                        continue;
                    }

                    case EventType::message: {
                        auto p = e.second.value();
                        std::cout << "  -   got msg as candidate" << '\n';
                        handleCandidateRPC(p.first, p.second);
                        break;
                    }
                }
                break;
            }

            case NodeType::Leader: {

                std::cout << "**** *******\n";
                std::cout << "I'm a leader\n";
                std::cout << "************\n";

                long timeout = 3000;
                event e = listenToRPCs(timeout);

                switch (e.first) {

                    case EventType::timeout: {
                        break;
                    }

                    case EventType::message: {
                        auto p = e.second.value();
                        std::cout << "Got msg: " << p.first << '\n';
                        handleLeaderRPC(p.first, p.second);
                        break;
                    }
                }

                if (nodeType == NodeType::Leader && pendingWrite.has_value()) {
                    int logSize = static_cast<int>(state.log.size());
                    for (const auto& pair : nextIndices) {
                        if (pair.second >= logSize) {
                            continue;
                        }
                        ProtoAppendEntries rpc;
                        rpc.set_term(state.currentTerm);
                        rpc.set_leaderid(node.id);
                        rpc.set_commitindex(prevCommitIndex);
                        if (logSize > 1) {
                            rpc.set_prevlogindex(state.log[logSize - 2].index);
                            rpc.set_prevlogterm(state.log[logSize - 2].term);
                        }
                        for (int i = pair.second; i < logSize; i++) {
                            LogEntry entry = state.log[i];
                            Command cmd = entry.command;

                            ProtoLogEntry* proto_entry = rpc.add_entries();

                            ProtoCommand* proto_command = proto_entry->mutable_command();
                            proto_command->set_longurl(cmd.longURL);
                            proto_command->set_shorturl(cmd.shortURL);

                            proto_entry->set_term(entry.term);
                            proto_entry->set_index(entry.index);
                        }
                        sendRPC(packToAny(rpc).data(), pair.first);
                    }
                }
            }

        }
    }
}


void Raft::updateNextIndices() {
    int lastLogIndex = state.log.size();
    for (const std::string& nodeAddress : node.nodeAddresses) {
        nextIndices[nodeAddress] = lastLogIndex;
    }
}


Raft::Raft(Node& node, Storage& storage) : node(node), storage(storage) {
    loadPersistentState();
}