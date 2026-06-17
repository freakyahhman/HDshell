#include "builtins.h"
#include "config.h"
#include "executor.h"
#include "history.h"
#include "jobs.h"
#include "path_utils.h"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <iostream>
#include <unistd.h>
#include <filesystem>
#include <sstream>
#include <cctype>
#include <signal.h>

extern char **environ;

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

const SimpleCommand* requireSimpleCommand(const Command& cmd) {
    const SimpleCommand* simpleCmd = dynamic_cast<const SimpleCommand*>(&cmd);
    if (simpleCmd == nullptr) {
        std::cerr << "Error: Unsupported command type" << std::endl;
    }

    return simpleCmd;
}

bool isEnvNameStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isEnvNameChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool isValidEnvName(const std::string& name) {
    if (name.empty() || !isEnvNameStart(name[0])) {
        return false;
    }

    for (char c : name) {
        if (!isEnvNameChar(c)) {
            return false;
        }
    }

    return true;
}

bool splitEnvAssignment(const std::string& assignment, std::string& name, std::string& value) {
    size_t equalPos = assignment.find('=');
    if (equalPos == std::string::npos) {
        return false;
    }

    name = assignment.substr(0, equalPos);
    value = assignment.substr(equalPos + 1);
    return true;
}

void printEnvironment() {
    for (char** env = environ; env != nullptr && *env != nullptr; ++env) {
        std::cout << *env << std::endl;
    }
}

bool resolveJobTarget(const std::string& target, const char* commandName, Jobs::Job& job) {
    if (target.empty()) {
        if (!Jobs::getMostRecentJob(job)) {
            std::cerr << commandName << ": no current job" << std::endl;
            return false;
        }

        return true;
    }

    if (target[0] == '%') {
        int jobId;
        if (!parsePositiveInt(target.substr(1), jobId)) {
            std::cerr << commandName << ": invalid job id: " << target << std::endl;
            return false;
        }

        if (!Jobs::getJobByJobId(jobId, job)) {
            std::cerr << commandName << ": job not found: " << target << std::endl;
            return false;
        }

        return true;
    }

    int parsedPid;
    if (!parsePositiveInt(target, parsedPid)) {
        std::cerr << commandName << ": invalid pid: " << target << std::endl;
        return false;
    }

    if (!Jobs::getJobByPid(static_cast<pid_t>(parsedPid), job)) {
        std::cerr << commandName << ": job not found: " << target << std::endl;
        return false;
    }

    return true;
}

int printFormattedTime(const Command& cmd, const char* commandName, const char* defaultFormat) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (simpleCmd->args.size() > 1) {
        std::cerr << "Usage: " << commandName << " [+format]" << std::endl;
        return -1;
    }

    std::string format = simpleCmd->args.empty() ? defaultFormat : simpleCmd->args[0];
    if (!format.empty() && format[0] == '+') {
        format.erase(0, 1);
    }

    std::time_t now = std::time(nullptr);
    std::tm localTime {};
    if (localtime_r(&now, &localTime) == nullptr) {
        std::cerr << commandName << ": failed to read local time" << std::endl;
        return -1;
    }

    char buffer[256];
    if (std::strftime(buffer, sizeof(buffer), format.c_str(), &localTime) == 0) {
        std::cerr << commandName << ": formatted output is too long" << std::endl;
        return -1;
    }

    std::cout << buffer << std::endl;
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
    {"fg", builtin_fg},
    {"bg", builtin_bg},
    {"env", builtin_env},
    {"printenv", builtin_printenv},
    {"export", builtin_export},
    {"setenv", builtin_setenv},
    {"unset", builtin_unset},
    {"unsetenv", builtin_unset},
    {"date", builtin_date},
    {"time", builtin_time},
    {"history", builtin_history},
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
    std::cout << "jobs -c: Remove finished jobs from the job table." << std::endl;
    std::cout << "jobs -d <pid|%job_id>: Remove one finished job from the job table." << std::endl;
    std::cout << "kill: Send a signal to a PID or background job." << std::endl;
    std::cout << "killall: Send a signal to active background jobs by command name." << std::endl;
    std::cout << "stop: Stop a PID or background job." << std::endl;
    std::cout << "resume: Resume a PID or background job." << std::endl;
    std::cout << "fg: Move a background/stopped job to the foreground." << std::endl;
    std::cout << "bg: Continue a stopped job in the background." << std::endl;
    std::cout << "env/printenv/export/setenv/unset: Manage environment variables." << std::endl;
    std::cout << "date/time: Show current date or time. Optional format: +%Y-%m-%d." << std::endl;
    std::cout << "history: Show command history. Use Up/Down arrows, history set <1-1000>, or history clear." << std::endl;
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
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    Jobs::reap(false);

    if (simpleCmd->args.empty()) {
        Jobs::print();
        return 0;
    }

    const std::string& action = simpleCmd->args[0];

    if (action == "-c" || action == "--clear") {
        int removed = Jobs::removeFinished();
        std::cout << "Removed " << removed << " finished job(s)" << std::endl;
        return 0;
    }

    if (action == "-d" || action == "--delete" || action == "rm") {
        if (simpleCmd->args.size() != 2) {
            std::cerr << "Usage: jobs [-c|--clear] | jobs [-d|--delete|rm] <pid|%job_id>" << std::endl;
            return -1;
        }

        Jobs::Job job;
        if (!resolveJobTarget(simpleCmd->args[1], "jobs", job)) {
            return -1;
        }

        if (Jobs::isActive(job.status)) {
            std::cerr << "jobs: refusing to remove active job: %" << job.id << std::endl;
            return -1;
        }

        Jobs::removeByJobId(job.id, false);
        return 0;
    }

    std::cerr << "Usage: jobs [-c|--clear] | jobs [-d|--delete|rm] <pid|%job_id>" << std::endl;
    return -1;
}

int Builtins::builtin_env(const Command& cmd) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (!simpleCmd->args.empty()) {
        std::cerr << "Usage: env" << std::endl;
        return -1;
    }

    printEnvironment();
    return 0;
}

int Builtins::builtin_printenv(const Command& cmd) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (simpleCmd->args.empty()) {
        printEnvironment();
        return 0;
    }

    int exitCode = 0;
    for (const std::string& name : simpleCmd->args) {
        const char* value = getenv(name.c_str());
        if (value == nullptr) {
            exitCode = -1;
            continue;
        }

        std::cout << value << std::endl;
    }

    return exitCode;
}

int Builtins::builtin_export(const Command& cmd) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (simpleCmd->args.empty()) {
        printEnvironment();
        return 0;
    }

    int exitCode = 0;

    for (size_t i = 0; i < simpleCmd->args.size(); ++i) {
        std::string name;
        std::string value;

        if (!splitEnvAssignment(simpleCmd->args[i], name, value)) {
            if (simpleCmd->args.size() == 2 && i == 0) {
                name = simpleCmd->args[0];
                value = simpleCmd->args[1];
                i = 1;
            }
            else {
                name = simpleCmd->args[i];
                const char* existingValue = getenv(name.c_str());
                value = existingValue == nullptr ? "" : existingValue;
            }
        }

        if (!isValidEnvName(name)) {
            std::cerr << "export: invalid variable name: " << name << std::endl;
            exitCode = -1;
            continue;
        }

        if (::setenv(name.c_str(), value.c_str(), 1) != 0) {
            perror("export");
            exitCode = -1;
        }
    }

    return exitCode;
}

int Builtins::builtin_setenv(const Command& cmd) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (simpleCmd->args.size() < 2) {
        std::cerr << "Usage: setenv <name> <value>" << std::endl;
        return -1;
    }

    const std::string& name = simpleCmd->args[0];
    std::string value = joinArgs(simpleCmd->args, 1);

    if (!isValidEnvName(name)) {
        std::cerr << "setenv: invalid variable name: " << name << std::endl;
        return -1;
    }

    if (::setenv(name.c_str(), value.c_str(), 1) != 0) {
        perror("setenv");
        return -1;
    }

    return 0;
}

int Builtins::builtin_unset(const Command& cmd) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (simpleCmd->args.empty()) {
        std::cerr << "Usage: unset <name> ..." << std::endl;
        return -1;
    }

    int exitCode = 0;
    for (const std::string& name : simpleCmd->args) {
        if (!isValidEnvName(name)) {
            std::cerr << "unset: invalid variable name: " << name << std::endl;
            exitCode = -1;
            continue;
        }

        if (::unsetenv(name.c_str()) != 0) {
            perror("unset");
            exitCode = -1;
        }
    }

    return exitCode;
}

int Builtins::builtin_date(const Command& cmd) {
    return printFormattedTime(cmd, "date", "%Y-%m-%d");
}

int Builtins::builtin_time(const Command& cmd) {
    return printFormattedTime(cmd, "time", "%H:%M:%S");
}

int Builtins::builtin_history(const Command& cmd) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (simpleCmd->args.empty()) {
        History::print();
        return 0;
    }

    const std::string& action = simpleCmd->args[0];

    if (action == "clear") {
        if (simpleCmd->args.size() != 1) {
            std::cerr << "Usage: history clear" << std::endl;
            return -1;
        }

        History::clear();
        return 0;
    }

    if (action == "limit") {
        if (simpleCmd->args.size() != 1) {
            std::cerr << "Usage: history limit" << std::endl;
            return -1;
        }

        std::cout << History::getLimit() << std::endl;
        return 0;
    }

    if (action == "set") {
        if (simpleCmd->args.size() != 2) {
            std::cerr << "Usage: history set <1-" << History::maximumLimit() << ">" << std::endl;
            return -1;
        }

        int parsedLimit;
        if (!parsePositiveInt(simpleCmd->args[1], parsedLimit) ||
            !History::setLimit(static_cast<size_t>(parsedLimit))) {
            std::cerr << "history: limit must be between 1 and "
                      << History::maximumLimit() << std::endl;
            return -1;
        }

        std::cout << "History limit set to " << History::getLimit() << std::endl;
        return 0;
    }

    std::cerr << "Usage: history [limit|clear|set <1-" << History::maximumLimit() << ">]" << std::endl;
    return -1;
}

int Builtins::builtin_fg(const Command& cmd) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (simpleCmd->args.size() > 1) {
        std::cerr << "Usage: fg [pid|%job_id]" << std::endl;
        return -1;
    }

    Jobs::reap(false);

    Jobs::Job job;
    std::string target = simpleCmd->args.empty() ? "" : simpleCmd->args[0];
    if (!resolveJobTarget(target, "fg", job)) {
        return -1;
    }

    if (!Jobs::isActive(job.status)) {
        std::cerr << "fg: job is not active: %" << job.id << std::endl;
        return -1;
    }

    if (::kill(-job.pid, SIGCONT) != 0) {
        perror("fg");
        return -1;
    }

    Jobs::setStatusByPid(job.pid, Jobs::Status::Running);
    std::cout << job.command << std::endl;
    return Executor::waitForForegroundProcessGroup(job.pid, job.command, job.pids, job.id);
}

int Builtins::builtin_bg(const Command& cmd) {
    const SimpleCommand* simpleCmd = requireSimpleCommand(cmd);
    if (simpleCmd == nullptr) {
        return -1;
    }

    if (simpleCmd->args.size() > 1) {
        std::cerr << "Usage: bg [pid|%job_id]" << std::endl;
        return -1;
    }

    Jobs::reap(false);

    Jobs::Job job;
    std::string target = simpleCmd->args.empty() ? "" : simpleCmd->args[0];
    if (!resolveJobTarget(target, "bg", job)) {
        return -1;
    }

    if (!Jobs::isActive(job.status)) {
        std::cerr << "bg: job is not active: %" << job.id << std::endl;
        return -1;
    }

    if (::kill(-job.pid, SIGCONT) != 0) {
        perror("bg");
        return -1;
    }

    Jobs::setStatusByPid(job.pid, Jobs::Status::Running);
    std::cout << "[" << job.id << "] " << job.command << " &" << std::endl;
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
