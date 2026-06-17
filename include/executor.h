#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "command.h"
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>

class Executor {
public:
    // Thuc thi mot Command
    static void executeCommand(std::unique_ptr<Command> cmd);
    static int handleFork(Command* cmd);
    static int setupRedirection(const SimpleCommand& cmd);
    static void initializeShellJobControl();
    static void prepareChildProcess(pid_t processGroupId);
    static int waitForForegroundProcessGroup(pid_t processGroupId,
                                             const std::string& command,
                                             const std::vector<pid_t>& childPids,
                                             int existingJobId = -1);
};

#endif
