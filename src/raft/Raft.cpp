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

void Raft::State::loadPersistentState() {
    size_t logSize;
    std::ifstream stateFile("/space/state.txt");
    if (stateFile.is_open()) {
        stateFile >> currentTerm;
        stateFile >> votedFor;

        stateFile >> logSize;

        stateFile.close();
    }
    std::ifstream logFile("/space/log.txt");
    if (logFile.is_open()) {
        for (size_t i = 0; i < logSize; ++i) {
            LogEntry entry;
            logFile >> entry.term;
            logFile >> entry.index;

            logFile >> entry.command.key;
            logFile >> entry.command.value;

            log.push_back(entry);
        }
        logFile.close();
    }
}

void Raft::State::dumpStateToFile(const std::vector<struct LogEntry>& newEntries) {
    std::ofstream stateFile("/space/state.txt");

    if (stateFile.is_open()) {
        // Write the state information
        stateFile << currentTerm << " ";
        stateFile << votedFor << "\n";

        // Write the log size
        stateFile << (log.size() + newEntries.size()) << "\n";

        // Close the file
        stateFile.close();
    }

    std::ofstream logFile("/space/log.txt", std::ios::out | std::ios::app);

    if (logFile.is_open()) {
        // Write each log entry
        for (const auto& entry : newEntries) {
            logFile << entry.term << " ";
            logFile << entry.index << " ";

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


void Raft::handleRPC(char* buffer) {
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

void Raft::listenToRPCs(long timeout) {

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
        std::cout << "elapsed: " << elapsed << '\n';
        if (elapsed >= timeout) {
            std::cout << "Timeout expired\n";
            if (state.type == NodeType::Follower) state.type = NodeType::Candidate;
            else if (state.type == NodeType::Candidate) state.type = NodeType::Follower;
            return;            
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
            if (state.type == NodeType::Follower) state.type = NodeType::Candidate;
            else if (state.type == NodeType::Candidate) state.type = NodeType::Follower;
            return;
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

                    std::cout << "Received msg from " << it->first << " " << std::string{buffer.begin(), buffer.end()} << '\n';
                    sendRPC("Nice to meet you!", it->first);

                    //renew timeout
                    if (state.type == NodeType::Follower) start = std::chrono::steady_clock::now();

                    // TODO: proper handling
                    ++it;
                }

            }   
        }   
    }
}

void Raft::run() {

    while(true) {

        if (state.type == NodeType::Follower) {

            std::cout << "Listen to heartbits\n";            

            //setup heartbeats timeout (sec), after which node runs election
            long timeout = 10;
            listenToRPCs(timeout);

        }

        else if (state.type == NodeType::Candidate) {

            std::cout << "Start election\n";

            //setup heartbeats timeout, after which node loses election
            runElection();
            long timeout = 10;
            listenToRPCs(timeout);            

        }

        else if (state.type == NodeType::Leader) {
            //TODO
        }

    }

}


Raft::Raft() {
    // do something on initilization
}