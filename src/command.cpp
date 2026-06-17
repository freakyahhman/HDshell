#include "command.h"
#include "builtins.h"
#include "globals.h"
#include "jobs.h"
#include <array>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
void closeAllPipes(const std::vector<std::array<int, 2>>& pipes) {
    for (const auto& pipeFds : pipes) {
        if (pipeFds[0] >= 0) {
            close(pipeFds[0]);
        }
        if (pipeFds[1] >= 0) {
            close(pipeFds[1]);
        }
    }
}

void executeSimpleCommandInChild(SimpleCommand* simpleCmd) {
    if (simpleCmd == nullptr || simpleCmd->name.empty()) {
        _exit(1);
    }

    if (Executor::setupRedirection(*simpleCmd) != 0) {
        _exit(1);
    }

    if (Builtins::isBuiltin(simpleCmd->name)) {
        Builtins builtins;
        int exitCode = builtins.executeBuiltin(*simpleCmd);
        _exit(exitCode == -1 ? 1 : exitCode);
    }

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(simpleCmd->name.c_str()));
    for (const auto& arg : simpleCmd->args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    execvp(simpleCmd->name.c_str(), argv.data());
    perror("execvp");
    _exit(127);
}

std::string buildSimpleCommandString(const SimpleCommand& cmd) {
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

std::string buildPipelineCommandString(const PipeCommand& pipeCmd) {
    std::string command;

    for (const auto& subcommand : pipeCmd.subcommands) {
        const SimpleCommand* simpleCmd = dynamic_cast<const SimpleCommand*>(subcommand.get());
        if (simpleCmd == nullptr) {
            continue;
        }

        if (!command.empty()) {
            command += " | ";
        }
        command += buildSimpleCommandString(*simpleCmd);
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

    return -1;
}

int waitForPipelineChildren(const std::vector<pid_t>& childPids) {
    int lastStatus = 0;

    for (size_t i = 0; i < childPids.size(); ++i) {
        int status;
        if (waitpid(childPids[i], &status, 0) < 0) {
            perror("waitpid");
            return -1;
        }

        if (i + 1 == childPids.size()) {
            lastStatus = status;
        }
    }

    return statusToExitCode(lastStatus);
}

int executePipeline(const std::vector<std::unique_ptr<Command>>& subcommands,
                    pid_t processGroupId,
                    bool claimTerminal,
                    const std::string& commandString) {
    std::vector<std::array<int, 2>> pipes(subcommands.size() - 1, std::array<int, 2>{-1, -1});
    for (auto& pipeFds : pipes) {
        if (pipe(pipeFds.data()) < 0) {
            perror("pipe");
            closeAllPipes(pipes);
            return -1;
        }
    }

    std::vector<pid_t> childPids;
    childPids.reserve(subcommands.size());

    for (size_t i = 0; i < subcommands.size(); ++i) {
        SimpleCommand* simpleCmd = dynamic_cast<SimpleCommand*>(subcommands[i].get());
        if (simpleCmd == nullptr) {
            std::cerr << "Error: pipeline only supports simple commands" << std::endl;
            closeAllPipes(pipes);
            return -1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            closeAllPipes(pipes);
            for (pid_t childPid : childPids) {
                waitpid(childPid, nullptr, 0);
            }
            return -1;
        }

        if (pid == 0) {
            Executor::prepareChildProcess(processGroupId);

            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }

            if (i + 1 < subcommands.size() && dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }

            closeAllPipes(pipes);
            executeSimpleCommandInChild(simpleCmd);
        }

        if (processGroupId == 0) {
            processGroupId = pid;
        }
        setpgid(pid, processGroupId);
        childPids.push_back(pid);
    }

    closeAllPipes(pipes);

    if (claimTerminal) {
        return Executor::waitForForegroundProcessGroup(processGroupId, commandString, childPids);
    }

    return waitForPipelineChildren(childPids);
}
}


int SimpleCommand::execute() {
    if (name.empty()) {
        return 0; // Neu khong co lenh nao duoc nhap, tra ve 0
    }

    if (Builtins::isBuiltin(name)) {
        int savedStdin = -1;
        int savedStdout = -1;

        if (!input_file.empty()) {
            savedStdin = dup(STDIN_FILENO);
            if (savedStdin < 0) {
                perror("dup");
                return -1;
            }
        }

        if (!output_file.empty()) {
            savedStdout = dup(STDOUT_FILENO);
            if (savedStdout < 0) {
                perror("dup");
                if (savedStdin != -1) {
                    close(savedStdin);
                }
                return -1;
            }
        }

        if (Executor::setupRedirection(*this) != 0) {
            if (savedStdin != -1) {
                dup2(savedStdin, STDIN_FILENO);
                close(savedStdin);
            }
            if (savedStdout != -1) {
                dup2(savedStdout, STDOUT_FILENO);
                close(savedStdout);
            }
            return -1;
        }

        Builtins builtins;
        exit_code = builtins.executeBuiltin(*this);

        if (savedStdin != -1) {
            if (dup2(savedStdin, STDIN_FILENO) < 0) {
                perror("dup2");
            }
            close(savedStdin);
        }

        if (savedStdout != -1) {
            if (dup2(savedStdout, STDOUT_FILENO) < 0) {
                perror("dup2");
            }
            close(savedStdout);
        }

        return exit_code; // Tra ve exit code cua lenh builtin
    }

    return executor->handleFork(this); // Neu khong phai builtin, thuc thi lenh binh thuong
}


int PipeCommand::execute() {
    if (subcommands.empty()) {
        std::cerr << "Error: empty pipeline" << std::endl;
        return -1;
    }

    if (subcommands.size() == 1) {
        exit_code = subcommands[0]->execute();
        return exit_code;
    }

    if (run_in_background) {
        std::string commandString = buildPipelineCommandString(*this);
        pid_t runnerPid = fork();
        if (runnerPid < 0) {
            perror("fork");
            exit_code = -1;
            return exit_code;
        }

        if (runnerPid == 0) {
            Executor::prepareChildProcess(0);
            int pipelineExitCode = executePipeline(subcommands, getpid(), false, commandString);
            _exit(pipelineExitCode == -1 ? 1 : pipelineExitCode);
        }

        setpgid(runnerPid, runnerPid);
        int jobId = Jobs::add(runnerPid, commandString);
        std::cout << "[" << jobId << "] " << runnerPid << std::endl;
        exit_code = 0;
        return exit_code;
    }

    exit_code = executePipeline(subcommands, 0, true, buildPipelineCommandString(*this));
    return exit_code;
}
