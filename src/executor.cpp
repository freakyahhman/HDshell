#include "executor.h"
#include "jobs.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace {
int shellTerminal = STDIN_FILENO;
bool shellInteractive = false;
pid_t shellProcessGroup = -1;
termios shellTerminalModes {};

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

int statusToExitCode(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    if (WIFSTOPPED(status)) {
        return 128 + WSTOPSIG(status);
    }

    return -1;
}

void restoreShellTerminal() {
    if (!shellInteractive) {
        return;
    }

    if (tcsetpgrp(shellTerminal, shellProcessGroup) < 0) {
        perror("tcsetpgrp");
    }
    if (tcsetattr(shellTerminal, TCSADRAIN, &shellTerminalModes) < 0) {
        perror("tcsetattr");
    }
}

void erasePid(std::vector<pid_t>& pids, pid_t pid) {
    pids.erase(std::remove(pids.begin(), pids.end(), pid), pids.end());
}
}

void Executor::initializeShellJobControl() {
    if (!isatty(shellTerminal)) {
        return;
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    shellProcessGroup = getpid();
    if (setpgid(shellProcessGroup, shellProcessGroup) < 0 && errno != EACCES && errno != EPERM) {
        perror("setpgid");
        return;
    }

    shellProcessGroup = getpgrp();
    if (tcsetpgrp(shellTerminal, shellProcessGroup) < 0) {
        return;
    }

    if (tcgetattr(shellTerminal, &shellTerminalModes) < 0) {
        perror("tcgetattr");
        return;
    }

    shellInteractive = true;
}

void Executor::prepareChildProcess(pid_t processGroupId) {
    pid_t targetProcessGroup = processGroupId == 0 ? getpid() : processGroupId;
    setpgid(0, targetProcessGroup);

    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
}

int Executor::waitForForegroundProcessGroup(pid_t processGroupId,
                                            const std::string& command,
                                            const std::vector<pid_t>& childPids,
                                            int existingJobId) {
    if (processGroupId <= 0) {
        return -1;
    }

    if (shellInteractive && tcsetpgrp(shellTerminal, processGroupId) < 0) {
        perror("tcsetpgrp");
    }

    std::vector<pid_t> remainingPids = childPids.empty() ? std::vector<pid_t>{processGroupId} : childPids;
    int lastStatus = 0;
    bool hasStatus = false;
    bool stopped = false;

    while (!remainingPids.empty()) {
        int status = 0;
        pid_t waitedPid = waitpid(-processGroupId, &status, WUNTRACED);

        if (waitedPid < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == ECHILD) {
                break;
            }

            perror("waitpid");
            restoreShellTerminal();
            return -1;
        }

        hasStatus = true;
        lastStatus = status;

        if (WIFSTOPPED(status)) {
            stopped = true;
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            erasePid(remainingPids, waitedPid);
        }
    }

    restoreShellTerminal();

    if (stopped) {
        if (existingJobId >= 0) {
            Jobs::setStatusByPid(processGroupId, Jobs::Status::Stopped);
            std::cout << std::endl
                      << "[" << existingJobId << "] Stopped\t"
                      << command << std::endl;
        }
        else {
            int jobId = Jobs::add(processGroupId, command, Jobs::Status::Stopped, remainingPids);
            std::cout << std::endl
                      << "[" << jobId << "] Stopped\t"
                      << command << std::endl;
        }
    }
    else if (existingJobId >= 0) {
        Jobs::removeByJobId(existingJobId, false);
    }

    return hasStatus ? statusToExitCode(lastStatus) : 0;
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

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        return -1;
    }
    else if (pid == 0) {
        Executor::prepareChildProcess(0);

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

    setpgid(pid, pid);
    std::string command = buildCommandString(*simpleCmd);

    if (!simpleCmd->run_in_background) {
        cmd->exit_code = Executor::waitForForegroundProcessGroup(pid, command, std::vector<pid_t>{pid});
        return cmd->exit_code;
    }

    int jobId = Jobs::add(pid, command);
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
