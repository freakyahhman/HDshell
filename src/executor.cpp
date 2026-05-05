#include "executor.h"
#include "jobs.h"
#include <cstdio>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::string buildCommandString(const SimpleCommand& cmd) {
    std::string command = cmd.name;

    for (const auto& arg : cmd.args) {
        command += " " + arg;
    }

    if (!cmd.input_file.empty()) {
        command += " < " + cmd.input_file;
    }

    if (!cmd.output_file.empty()) {
        command += " > " + cmd.output_file;
    }

    return command;
}
}

void Executor::executeCommand(std::unique_ptr<Command> cmd) {
    if (cmd) {
        int exit_code = cmd->execute();
        cmd->exit_code = exit_code;
    }
}

int Executor::handleFork(Command* cmd) {
    SimpleCommand* simpleCmd = dynamic_cast<SimpleCommand*>(cmd);
    if (simpleCmd == nullptr) {
        std::cerr << "Error: Unsupported command type" << std::endl;
        return -1;
    }

    int pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        return -1;
    }
    else if (pid == 0) {
        if (simpleCmd->run_in_background) {
            setpgid(0, 0);
        }

        // Process con
        // Thiet lap redirection neu can thiet
        if (setupRedirection(*simpleCmd) != 0) {
            _exit(1);
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
    if (!simpleCmd->run_in_background) {
        waitpid(pid, &status, 0); // Cho process con ket thuc
        if (WIFEXITED(status)) {
            cmd->exit_code = WEXITSTATUS(status); // Lay exit code cua process con
        }
        else {
            cmd->exit_code = -1; // Neu process con bi loi, tra ve -1
        }

        return cmd->exit_code;
    }

    setpgid(pid, pid);
    int jobId = Jobs::add(pid, buildCommandString(*simpleCmd));
    std::cout << "[" << jobId << "] " << pid << std::endl;
    cmd->exit_code = 0;
    return cmd->exit_code;
}

int Executor::setupRedirection(const SimpleCommand& cmd) {
    if (!cmd.input_file.empty()) {
        int inputFd = open(cmd.input_file.c_str(), O_RDONLY);
        if (inputFd < 0) {
            perror(cmd.input_file.c_str());
            return -1;
        }

        if (dup2(inputFd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(inputFd);
            return -1;
        }
        close(inputFd);
    }

    if (!cmd.output_file.empty()) {
        int outputFd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outputFd < 0) {
            perror(cmd.output_file.c_str());
            return -1;
        }

        if (dup2(outputFd, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(outputFd);
            return -1;
        }
        close(outputFd);
    }

    return 0;
}
