#include <iostream>
#include "Node.h"

void Node::start() {
    std::cout << "Starting the node...\n";
    s.run();
}