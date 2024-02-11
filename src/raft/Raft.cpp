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

template <typename T>
inline static auto getElapsed(T start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

void Raft::loadPersistentState() {

    std::cout << "Start loading persistent state...\n";

    std::ifstream stateFile(stateFilename);
    if (stateFile.is_open()) {
        stateFile >> state.currentTerm;
        stateFile >> state.votedFor;
        stateFile.close();
    }
    std::ifstream logFile(logFilename);
    if (logFile.is_open()) {
        while (!logFile.eof()) {
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

    std::cout << "Loading persistent state done\n";

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
                    nodeType.store(NodeType::Leader);
                    updateNextIndices();
                }
            } else if (rv_resp.term() > state.currentTerm) {
                state.currentTerm = rv_resp.term();
                nodeType.store(NodeType::Follower);
                commitStateToFile();
            }
        }
    
    else if (any.UnpackTo(&ae))
        {
            nodeType.store(NodeType::Follower);
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
            nodeType.store(NodeType::Follower);
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

    if (any.UnpackTo(&ae_resp)) {            
            if (state.currentTerm < ae_resp.term()) {
                state.currentTerm = ae_resp.term();
                nodeType.store(NodeType::Follower);
                commitStateToFile();
                return;
            }
            if (ae_resp.success()) {
                nextIndices[from] = prevCommitIndex + 1;
            } else {
                --nextIndices[from];
            }
    }
    
    if (any.UnpackTo(&ae)) {
            nodeType.store(NodeType::Follower);
            handleAppendEntries(ae, from);
    }

    if (any.UnpackTo(&rv)) {
            //not vote
            if (rv.term() <= state.currentTerm) {
                ProtoRequestVoteResponse response;
                response.set_term(state.currentTerm);
                response.set_votegranted(false);
                sendRPC(packToAny(response).data(), from);
                return;
            }
            nodeType.store(NodeType::Follower);
            handleRequestVote(rv, from);
    }
}

void Raft::handleAppendEntries(ProtoAppendEntries rpc, const std::string &from) {

    if (rpc.term() < state.currentTerm) return;

    else if (rpc.term() > state.currentTerm) {
        state.currentTerm = rpc.term();
        state.votedFor = -1;
    }

    currentLeader = from;

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
        std::cout << std::strerror(errno) << "Error sending msg\n";
        close(node.writeSockets[to]);
        node.writeSockets.erase(to);
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
    requestVote.set_lastlogindex(logSize > 0 ? state.log.back().index : -1);
    requestVote.set_lastlogterm(logSize > 0 ? state.log.back().term : -1);

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

        auto elapsed  = getElapsed(start);
        std::cout << "Elapsed: " << elapsed << '\n';
        if (elapsed >= timeout) {
            std::cout << "Timeout expired\n";
            return {EventType::timeout, std::nullopt};
        }

        struct timeval tv;
        tv.tv_sec = (timeout - elapsed) / 1000;
        tv.tv_usec = ((timeout - elapsed) % 1000) * 1000;

        std::cout << "Seconds left: " << tv.tv_sec << '\n';
        std::cout << "Useconds left: " << tv.tv_usec << '\n';

        //wait for an activity on one of the sockets
        std::cout << "Waiting for activity...\n";
        int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);
        std::cout << "Select terminated\n";

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

    auto global_clock = std::chrono::steady_clock::now();

    while(true) {
        
        switch (nodeType.load()) {

            case NodeType::Follower: {

                std::cout << "*******************\n";
                std::cout << "Listen to heartbeats\n";
                std::cout << "*******************\n";

                //setup heartbeats timeout (sec), after which node runs election
                long timeout = static_cast<long>(distr(gen));
                event e = listenToRPCs(timeout);

                switch (e.first) {

                    case EventType::timeout: {
                        nodeType.store(NodeType::Candidate);
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

                const std::lock_guard lock(mutex);
                auto elapsed = getElapsed(global_clock);
                
                if (elapsed >= heartbeatTimeout) {

                    //send heartbeats
                    for (const auto& addr : node.nodeAddresses) {

                        auto rpc = constructAppendRPC(addr, std::nullopt);
                        sendRPC(packToAny(rpc).data(), addr);

                    }

                    global_clock = std::chrono::steady_clock::now();

                }

                event e = listenToRPCs(heartbeatTimeout);

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
            }

        }
    }
}

inline void Raft::printState() {

    std::cout << "!!! Current state !!!\n"; 
    std::cout << "  -> currentTerm: " << state.currentTerm << "\n";
    std::cout << "  -> votedFor: " << state.votedFor << "\n";
    std::cout << "  -> log.size(): " << state.log.size() << "\n";

}

inline ProtoAppendEntries Raft::constructAppendRPC(const std::string& receiverNode, std::optional<LogEntry> potentialNewEntryOpt) {

    printState();

    //allocate
    ProtoAppendEntries proto_rpc;

    //set rpc
    proto_rpc.set_term(state.currentTerm);
    proto_rpc.set_leaderid(node.id);

    auto prevLogIndex = nextIndices[receiverNode] - 1;
    std::cout << "  -   constructAppendRPC: prevLogIndex for node " << receiverNode << ": " << prevLogIndex << "\n";
    std::cout << "  -   constructAppendRPC: nextIndices[receiverNode]: " << nextIndices[receiverNode] << "\n";

    proto_rpc.set_prevlogindex(prevLogIndex);
    proto_rpc.set_prevlogterm(prevLogIndex == -1 ? -1 : state.log[prevLogIndex].term);
    proto_rpc.set_commitindex(prevCommitIndex);

    std::cout << "  -   constructAppendRPC: start constructing RPCs\n";

    //add all the old logs, that the node is missing
    for (int oldIndex = nextIndices[receiverNode]; oldIndex <= prevCommitIndex; ++oldIndex) {

        auto& oldEntry = state.log[oldIndex];
    
        ProtoLogEntry* proto_entry = proto_rpc.add_entries();
        ProtoCommand*  proto_command = proto_entry->mutable_command();

        //set command
        proto_command->set_longurl(oldEntry.command.longURL);
        proto_command->set_shorturl(oldEntry.command.shortURL);        
    
        //set entry
        proto_entry->set_term(oldEntry.term);
        proto_entry->set_index(oldEntry.index);

        std::cout << "  -   constructAppendRPC: adding old entry:\n";
        std::cout << proto_entry->DebugString();

    }

    if (!potentialNewEntryOpt.has_value()) {
        std::cout << "  -   constructAppendRPC: no new entries, sending:\n";
        std::cout << proto_rpc.DebugString(); 
        return proto_rpc;
    }

    auto potentialNewEntry = potentialNewEntryOpt.value();

    //add potential new log
    ProtoLogEntry* proto_entry = proto_rpc.add_entries();
    ProtoCommand*  proto_command = proto_entry->mutable_command();    

    //set command
    proto_command->set_longurl(potentialNewEntry.command.longURL);
    proto_command->set_shorturl(potentialNewEntry.command.shortURL);
    
    //set entry
    proto_entry->set_term(potentialNewEntry.term);
    proto_entry->set_index(potentialNewEntry.index);

    std::cout << "  -   constructAppendRPC: adding new entry:\n";
    std::cout << proto_entry->DebugString();

    return proto_rpc;

}

bool Raft::makeWriteRequest(const std::string &longUrl, const std::string &shortUrl) {

    std::cout << "  -   server thread: trying to aquire lock\n";
    const std::lock_guard lock(mutex);
    std::cout << "  -   server thread: lock aquired\n";

    if (nodeType.load() != NodeType::Leader) {
        std::cout << "  -   server thread: state change before call to makeWriteRequest. abort\n";
        return false;
    }

    Command newCommand = {longUrl, shortUrl};

    LogEntry potentialNewEntry = {
        .term = state.currentTerm, 
        .index = static_cast<int>(state.log.size()),
        .command = newCommand,
    };

    std::cout << "  -   server thread: start sending append RPCs\n";

    //send append requests to each node
    for (const auto& addr : node.nodeAddresses) {

        auto rpc = constructAppendRPC(addr, std::make_optional<LogEntry>(potentialNewEntry));
        sendRPC(packToAny(rpc).data(), addr);

    }

    std::cout << "  -   server thread: start waiting for responces\n";

    //this copy is needed to later updater nextIndices of responded noded, if transaction will be commited
    std::unordered_map<std::string, int> nextIndicesUpd = nextIndices;
    int receivedApproves = 1;
    auto start = std::chrono::steady_clock::now();
    while(true) {

        //wait for resposes halp of the heartbeat timeout
        auto elapsed = getElapsed(start);
        if (elapsed >= heartbeatTimeout / 2) {
            std::cout << "  -   server thread: timeout while waiting for responses\n";
            break;
        }
        
        event e = listenToRPCs((heartbeatTimeout / 2) - elapsed);

        //didn't get enought votes. transaction is cancelled. go back to normal heartbeat loop.
        if (e.first == EventType::timeout) {
            std::cout << "  -   server thread: timeout while waiting for responses\n";
            break;
        }

        else if (e.first == EventType::message) {

            auto p = e.second.value();

            google::protobuf::Any any;
            any.ParseFromString(p.first);

            //handle here only if AppendEntriesResponse
            ProtoAppendEntriesResponse ae_resp;
            if (any.UnpackTo(&ae_resp)) {

                std::cout << "  -   server thread: deserialized AppendEntriesResponse from node: " << p.second << ", " << any.DebugString() << "\n"; 
                
                //current term changed, step down
                if (state.currentTerm < ae_resp.term()) {

                    state.currentTerm = ae_resp.term();
                    nodeType.store(NodeType::Follower);
                    commitStateToFile();
                    return false;

                }

                if (ae_resp.success()) {

                    ++receivedApproves;
                    nextIndicesUpd[p.second] = state.log.size();

                }

                else --nextIndices[p.second];

            }
            else {

                handleLeaderRPC(p.first, p.second);
                if (nodeType.load() != NodeType::Leader) return false;

            }
        }
    }

    if (receivedApproves < node.numNodesForConsesus) {
        std::cout << "  -   didn't receive enought votes for consesus. abort transaction\n";
        return false;
    }

    //received enough successes for consesus. commit transaction
    std::cout << "  -   received enough successes for consesus\n";

    //add to log
    state.log.push_back(potentialNewEntry);

    //advance state machine
    commitLogsToFile(prevCommitIndex, state.log.size() - 1);
    commitToStorage(prevCommitIndex, state.log.size() - 1);
    prevCommitIndex = state.log.size() - 1;
    nextIndices = nextIndicesUpd;

    std::cout << "  -   transaction commited\n";

    return true;

}

void Raft::updateNextIndices() {
    std::cout << "  -   updating next indeces\n";
    int lastLeaderLogIndex = prevCommitIndex;
    for (const std::string& nodeAddress : node.nodeAddresses) {
        nextIndices[nodeAddress] = lastLeaderLogIndex + 1;
        std::cout << "  -   set " << nodeAddress << " to: " << lastLeaderLogIndex + 1 << "\n";
    }
}


Raft::Raft(Node& node, Storage& storage) : node(node), storage(storage) {
    loadPersistentState();
}