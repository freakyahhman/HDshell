#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "command.h"
#include <memory>

class Executor {
public:
    // Thuc thi mot Command
    static void executeCommand(std::unique_ptr<Command> cmd);
private:
    static int handleFork(Command* cmd);
    static void setupRedirection(const std::vector<std::string>& tokens);
};

#endif