#include "executor.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

void Executor::executeCommand(std::unique_ptr<Command> cmd) {
    if (cmd) {
        int exit_code = cmd->execute();
        cmd->exit_code = exit_code;
    }
}

int Executor::handleFork(Command* cmd) {
    int pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        return -1;
    }
    else if (pid == 0) {
        // Process con
        // Thiet lap redirection neu can thiet
        SimpleCommand* simpleCmd = dynamic_cast<SimpleCommand*>(cmd);
        if (!simpleCmd->input_file.empty()) {
            freopen(simpleCmd->input_file.c_str(), "r", stdin);
        }
        if (!simpleCmd->output_file.empty()) {
            freopen(simpleCmd->output_file.c_str(), "w", stdout);
        }
        
        // Chuyen doi args sang dang char*[]
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(simpleCmd->name.c_str()));
        for (const auto& arg : simpleCmd->args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr); // Ket thuc mang args bang nullptr

        execvp(simpleCmd->name.c_str(), argv.data());
        perror("execvp");
        cmd->exit_code = 127; // Neu execvp that bai, dat exit code la 127
        _exit(127);
    }
    
    // Process cha
    int status;
    waitpid(pid, &status, 0); // Cho process con ket thuc
    if (WIFEXITED(status)) {
        cmd->exit_code = WEXITSTATUS(status); // Lay exit code cua process con
    }
    else {
        cmd->exit_code = -1; // Neu process con bi loi, tra ve -1
    }

    return cmd->exit_code;
}

void Executor::setupRedirection(const std::vector<std::string>& tokens) {
    // Ham nay se duoc su dung de thiet lap redirection cho lenh
    // (chua hoan thien)
}