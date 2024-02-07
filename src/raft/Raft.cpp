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

void Raft::dumpStateToFile(const std::vector<struct LogEntry>& newEntries) {
    std::ofstream stateFile(stateFilename);

    if (stateFile.is_open()) {
        // Write the state information
        stateFile << state.currentTerm << " ";
        stateFile << state.votedFor << "\n";

        // Write the log size
        stateFile << (state.log.size() + newEntries.size()) << "\n";

        // Close the file
        stateFile.close();
    }

    std::ofstream logFile(logFilename, std::ios::out | std::ios::app);

    if (logFile.is_open()) {
        // Write each log entry
        for (const auto& entry : newEntries) {
            logFile << entry.term << " ";
            logFile << entry.index << " ";

            logFile << entry.command.longURL << " ";
            logFile << entry.command.shortURL << "\n";
        }

        // Close the file
        logFile.close();
    }
}


void Raft::applyCommand(const Command& command) {
    storage.insertURL(command.longURL, command.shortURL);
}


bool Raft::compareLogEntries(const LogEntry& first, const LogEntry& second) {
    return first.term == second.term &&
        first.index == second.index &&
        first.command.longURL == second.command.longURL &&
        first.command.shortURL == second.command.shortURL;
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


void Raft::handleRPC(const std::string& buffer) {
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

void Raft::sendRPC(const std::string& data, const std::string& to) {

    if (node.writeSockets.count(to) == 0) {
        std::cout << "Try to connected to: " << to << '\n';
        if (node.connectToNode(to) == -1) {
            std::cerr << "Failed to send. Couldn't connect to: " << to << '\n';
            return;
        }
    }

    std::cout << "Try send to: " << to << '\n';
    if (send(node.writeSockets[to], data.c_str(), strlen(data.c_str()), 0) > 0) {
        std::cout << "Sent successful\n";
    } else {
        std::cout << "Error sending msg\n";
    }

}

void Raft::runElection() {
    //TODO
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

        auto elapsed  = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Elapsed: " << elapsed << '\n';
        if (elapsed >= timeout) {
            std::cout << "Timeout expired\n";
            return {EventType::timeout, std::nullopt};
        }

        struct timeval tv;
        tv.tv_sec = timeout - elapsed;
        tv.tv_usec = 0;

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

void Raft::run() {

    while(true) {
        
        switch (state.type) {

            case NodeType::Follower: {

                std::cout << "Listen to heartbits\n";            

                //setup heartbeats timeout (sec), after which node runs election
                long timeout = 10;
                event e = listenToRPCs(timeout);

                switch (e.first) {

                    case EventType::timeout: {
                        state.type = NodeType::Candidate;
                        break;
                    }

                    case EventType::message: {
                        auto p = e.second.value();
                        std::cout << "Got msg: " << p.first << '\n';
                        handleRPC(p.first);
                        break;
                    }
                }
                break;
            }

            case NodeType::Candidate: {

                std::cout << "Start election\n";

                runElection();

                long timeout = 10;
                event e = listenToRPCs(timeout);

                switch (e.first) {

                    case EventType::timeout: {
                        state.type = NodeType::Follower;
                        break;
                    }

                    case EventType::message: {
                        auto p = e.second.value();
                        std::cout << "Got msg: " << p.first << '\n';
                        //handleRPC(p.first);
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