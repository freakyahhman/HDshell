#include "executor.h"

void Executor::executeCommand(std::unique_ptr<Command> cmd) {
    if (cmd) {
        int exit_code = cmd->execute();
        cmd->exit_code = exit_code;
    }
}

int Executor::handleFork(Command* cmd) {
    // Ham nay se duoc su dung de tao ra mot process con de thuc thi lenh
    // (chua hoan thien)
    return 0; // Tra ve exit code cua process con
}

void Executor::setupRedirection(const std::vector<std::string>& tokens) {
    // Ham nay se duoc su dung de thiet lap redirection cho lenh
    // (chua hoan thien)
}