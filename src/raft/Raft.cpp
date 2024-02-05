#include "Raft.h"
#include <fstream>


Raft::Raft() {
    std::ifstream stateFile("/space/state.txt");
    if (stateFile.is_open()) {
        stateFile >> state.currentTerm;
        stateFile >> state.votedFor;

        size_t logSize;
        stateFile >> logSize;

        for (size_t i = 0; i < logSize; ++i) {
            LogEntry entry;
            stateFile >> entry.term;
            stateFile >> entry.index;

            int mode;
            stateFile >> mode;
            entry.command.mode = static_cast<Mode>(mode);

            stateFile >> entry.command.key;
            stateFile >> entry.command.value;

            state.log.push_back(entry);
        }

        stateFile.close();
    }
}