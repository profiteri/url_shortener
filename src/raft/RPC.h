#pragma once

#include <vector>
#include <string>

struct Command {
    std::string longURL;
    std::string shortURL;
};


struct LogEntry {
    int term;
    int index;
    struct Command command;
};