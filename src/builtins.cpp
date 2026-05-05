#include "builtins.h"
#include "config.h"
#include "jobs.h"
#include "path_utils.h"
#include <fstream>
#include <string>
#include <iostream>
#include <unistd.h>
#include <filesystem>
#include <sstream>
#include <cctype>
#include <signal.h>

namespace {
bool parsePositiveInt(const std::string& value, int& result) {
    if (value.empty()) {
        return false;
    }

    result = 0;
    for (char c : value) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
        result = result * 10 + (c - '0');
    }

    return result > 0;
}

std::string joinArgs(const std::vector<std::string>& args, size_t start) {
    std::string result;
    for (size_t i = start; i < args.size(); ++i) {
        if (!result.empty()) {
            result += " ";
        }
        result += args[i];
    }

    return result;
}

bool resolvePidTarget(const std::string& target, const char* commandName, pid_t& signalPid, pid_t& jobPid) {
    if (!target.empty() && target[0] == '%') {
        int jobId;
        if (!parsePositiveInt(target.substr(1), jobId)) {
            std::cerr << commandName << ": invalid job id: " << target << std::endl;
            return false;
        }

        jobPid = Jobs::getPidByJobId(jobId);
        if (jobPid < 0) {
            std::cerr << commandName << ": job not found: " << target << std::endl;
            return false;
        }

        signalPid = -jobPid;
        return true;
    }

    int parsedPid;
    if (!parsePositiveInt(target, parsedPid)) {
        std::cerr << commandName << ": invalid pid: " << target << std::endl;
        return false;
    }

    signalPid = static_cast<pid_t>(parsedPid);
    jobPid = signalPid;
    return true;
}

int signalOneProcess(const Command& cmd, const char* commandName, int signalNumber, Jobs::Status newStatus) {
    const SimpleCommand* simpleCmd = dynamic_cast<const SimpleCommand*>(&cmd);
    if (simpleCmd == nullptr) {
        std::cerr << "Error: Unsupported command type" << std::endl;
        return -1;
    }

    if (simpleCmd->args.size() != 1) {
        std::cerr << "Usage: " << commandName << " <pid|%job_id>" << std::endl;
        return -1;
    }

    Jobs::reap(false);

    pid_t signalPid;
    pid_t jobPid;
    if (!resolvePidTarget(simpleCmd->args[0], commandName, signalPid, jobPid)) {
        return -1;
    }

    if (::kill(signalPid, signalNumber) != 0) {
        perror(commandName);
        return -1;
    }

    Jobs::setStatusByPid(jobPid, newStatus);
    return 0;
}
}

Builtins::Builtins() = default;


const std::map<std::string, std::function<int(const Command&)>> Builtins::builtinCommands = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"clear", builtin_clear},
    {"help", builtin_help},
    {"father", builtin_father},
    {"ps", builtin_ps},
    {"jobs", builtin_jobs},
    {"kill", builtin_kill},
    {"killall", builtin_killall},
    {"stop", builtin_stop},
    {"resume", builtin_resume},
    {"change", builtin_change}
};


bool Builtins::isBuiltin(const std::string& cmd) {
    return builtinCommands.find(cmd) != builtinCommands.end();
}

int Builtins::executeBuiltin(const Command& cmd) const {
    const SimpleCommand* simpleCmd = dynamic_cast<const SimpleCommand*>(&cmd);
    if (simpleCmd == nullptr) {
        std::cerr << "Error: Unsupported command type" << std::endl;
        return -1;
    }

    auto it = builtinCommands.find(simpleCmd->name);
    if (it != builtinCommands.end()) {
        return it->second(cmd);
    }

    std::cerr << "Error: Command not found: " << simpleCmd->name << std::endl;
    return -1;
}


int Builtins::builtin_cd(const Command& cmd) {
    (void)cmd;
    SimpleCommand* simpleCmd = const_cast<SimpleCommand*>(dynamic_cast<const SimpleCommand*>(&cmd));
    if (simpleCmd == nullptr) {
        std::cerr << "Error: Unsupported command type" << std::endl;
        return -1;
    }

    std::string targetDir;

    if (simpleCmd->args.empty() || simpleCmd->args[0] == "~") {
        char* home = getenv("HOME");
        if (home == nullptr) {
            std::cerr << "Error: HOME environment variable not set" << std::endl;
            return -1;
        }
        targetDir = home;
    } else if (simpleCmd->args[0] == "-") {
        const char* oldpwd = getenv("OLDPWD");
        if (oldpwd == nullptr) {
            std::cerr << "Error: OLDPWD environment variable not set" << std::endl;
            return -1;
        }
        targetDir = oldpwd;
        std::cout << targetDir << std::endl;
    } else {
        targetDir = simpleCmd->args[0];
    }

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    if (chdir(targetDir.c_str()) != 0) {
        std::cerr << "Error: Failed to change directory to " << targetDir << std::endl;
        return -1;
    }

    setenv("OLDPWD", cwd, 1);
    char new_pwd[1024];
    getcwd(new_pwd, sizeof(new_pwd));
    setenv("PWD", new_pwd, 1);

    return 0;
}

int Builtins::builtin_exit(const Command& cmd) {
    (void)cmd;
    exit(0);
    return 0; // This line will never be reached, but it's here to satisfy the return type
}

int Builtins::builtin_clear(const Command& cmd) {
    (void)cmd;
    std::cout << "\033[H\033[2J\033[3J" << std::flush;
    
    return 0;
}

int Builtins::builtin_help(const Command& cmd) {
    (void)cmd;
    std::cout << "===================== BUILTIN COMMANDS =====================" << std::endl;
    std::cout << "cd: Change the current directory." << std::endl;
    std::cout << "exit: Exit the shell." << std::endl;
    std::cout << "clear: Clear the terminal screen." << std::endl;
    std::cout << "help: Display this help message." << std::endl;
    std::cout << "ps: Show all processes and their information include: their own ids, their parents' ids, their states." << std::endl;
    std::cout << "jobs: Show background jobs started by this shell." << std::endl;
    std::cout << "kill: Send a signal to a PID or background job." << std::endl;
    std::cout << "killall: Send a signal to active background jobs by command name." << std::endl;
    std::cout << "stop: Stop a PID or background job." << std::endl;
    std::cout << "resume: Resume a PID or background job." << std::endl;
    std::cout << "change: Change shell name or prompt color." << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}


int Builtins::builtin_father(const Command& cmd) {
    (void)cmd;
    std::string filename = path_utils::resolveDataFilePath("father.txt");
    std::ifstream infile = std::ifstream(filename);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return -1; 
    }

    while (!infile.eof()) {
        std::string line;
        std::getline(infile, line);
        std::cout << line << std::endl;
    }

    return 0;
}

int Builtins::builtin_jobs(const Command& cmd) {
    (void)cmd;

    Jobs::reap(false);
    Jobs::print();

    return 0;
}

int Builtins::builtin_ps(const Command& cmd) {
    (void)cmd;

    std::cout << "PID\tPPID\tSTATE\tCOMMAND\n";


    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        std::string name = entry.path().filename().string();

        bool isPid = true;

        for (char c : name) {
            if (!std::isdigit(c)) {
                isPid = false;
                break;
            }
        }

        if (!isPid)
            continue;

        std::ifstream statFile("/proc/" + name + "/stat");

        if (!statFile.is_open())
            continue;
        
        
        int pid;
        std::string comm;
        char state;
        int ppid;

        statFile >> pid >> comm >> state >> ppid;


        std::cout << pid << "\t"  << ppid << "\t" << state << "\t" << comm << "\n";

    }

    return 0;
}


int Builtins::builtin_kill(const Command& cmd) {
    const SimpleCommand* simpleCmd = dynamic_cast<const SimpleCommand*>(&cmd);
    if (simpleCmd == nullptr) {
        std::cerr << "Error: Unsupported command type" << std::endl;
        return -1;
    }

    if (simpleCmd->args.empty()) {
        std::cerr << "Usage: kill [-signal] <pid|%job_id> ..." << std::endl;
        return -1;
    }

    int signalNumber = SIGTERM;
    size_t targetIndex = 0;

    if (!simpleCmd->args[0].empty() && simpleCmd->args[0][0] == '-') {
        if (!parsePositiveInt(simpleCmd->args[0].substr(1), signalNumber)) {
            std::cerr << "kill: invalid signal: " << simpleCmd->args[0] << std::endl;
            return -1;
        }

        targetIndex = 1;
    }

    if (targetIndex >= simpleCmd->args.size()) {
        std::cerr << "Usage: kill [-signal] <pid|%job_id> ..." << std::endl;
        return -1;
    }

    int exitCode = 0;
    Jobs::reap(false);

    for (size_t i = targetIndex; i < simpleCmd->args.size(); ++i) {
        const std::string& target = simpleCmd->args[i];
        pid_t signalPid;
        pid_t jobPid;

        if (!resolvePidTarget(target, "kill", signalPid, jobPid)) {
            exitCode = -1;
            continue;
        }

        if (::kill(signalPid, signalNumber) != 0) {
            perror("kill");
            exitCode = -1;
        }
        else if (signalNumber == SIGSTOP) {
            Jobs::setStatusByPid(jobPid, Jobs::Status::Stopped);
        }
        else if (signalNumber == SIGCONT) {
            Jobs::setStatusByPid(jobPid, Jobs::Status::Running);
        }
    }

    return exitCode;
}

int Builtins::builtin_killall(const Command& cmd) {
    const SimpleCommand* simpleCmd = dynamic_cast<const SimpleCommand*>(&cmd);
    if (simpleCmd == nullptr) {
        std::cerr << "Error: Unsupported command type" << std::endl;
        return -1;
    }

    if (simpleCmd->args.empty()) {
        std::cerr << "Usage: killall [-signal] <command_name> ..." << std::endl;
        return -1;
    }

    int signalNumber = SIGTERM;
    size_t targetIndex = 0;

    if (!simpleCmd->args[0].empty() && simpleCmd->args[0][0] == '-') {
        if (!parsePositiveInt(simpleCmd->args[0].substr(1), signalNumber)) {
            std::cerr << "killall: invalid signal: " << simpleCmd->args[0] << std::endl;
            return -1;
        }

        targetIndex = 1;
    }

    if (targetIndex >= simpleCmd->args.size()) {
        std::cerr << "Usage: killall [-signal] <command_name> ..." << std::endl;
        return -1;
    }

    int exitCode = 0;
    Jobs::reap(false);

    for (size_t i = targetIndex; i < simpleCmd->args.size(); ++i) {
        const std::string& commandName = simpleCmd->args[i];
        std::vector<pid_t> pids = Jobs::findPidsByCommandName(commandName);

        if (pids.empty()) {
            std::cerr << "killall: no active background job named: "
                      << commandName << std::endl;
            exitCode = -1;
            continue;
        }

        for (pid_t pid : pids) {
            if (::kill(-pid, signalNumber) != 0) {
                perror("killall");
                exitCode = -1;
            }
            else if (signalNumber == SIGSTOP) {
                Jobs::setStatusByPid(pid, Jobs::Status::Stopped);
            }
            else if (signalNumber == SIGCONT) {
                Jobs::setStatusByPid(pid, Jobs::Status::Running);
            }
        }
    }

    return exitCode;
}

int Builtins::builtin_stop(const Command& cmd) {
    return signalOneProcess(cmd, "stop", SIGSTOP, Jobs::Status::Stopped);
}

int Builtins::builtin_resume(const Command& cmd) {
    return signalOneProcess(cmd, "resume", SIGCONT, Jobs::Status::Running);
}

int Builtins::builtin_change(const Command& cmd) {
    const SimpleCommand* simpleCmd = dynamic_cast<const SimpleCommand*>(&cmd);
    if (simpleCmd == nullptr) {
        std::cerr << "Error: Unsupported command type" << std::endl;
        return -1;
    }

    if (simpleCmd->args.size() < 2) {
        std::cerr << "Usage: change <name|color> <value>" << std::endl;
        return -1;
    }

    const std::string& key = simpleCmd->args[0];
    std::string value = joinArgs(simpleCmd->args, 1);

    if (key == "name") {
        if (!Config::set("name", value)) {
            return -1;
        }

        std::cout << "Shell name changed to " << value << std::endl;
        return 0;
    }

    if (key == "color") {
        if (!Config::isValidColor(value)) {
            std::cerr << "change: invalid color: " << value << std::endl;
            std::cerr << "Available colors: default, black, red, green, yellow, blue, magenta, cyan, white" << std::endl;
            return -1;
        }

        if (!Config::set("color", value)) {
            return -1;
        }

        std::cout << "Shell color changed to " << value << std::endl;
        return 0;
    }

    std::cerr << "Usage: change <name|color> <value>" << std::endl;
    return -1;
}
