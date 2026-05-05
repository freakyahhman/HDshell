#include "jobs.h"
#include <iostream>
#include <sys/wait.h>
#include <vector>

namespace {
std::vector<Jobs::Job> backgroundJobs;
int nextJobId = 1;

const char* statusToString(Jobs::Status status) {
    switch (status) {
        case Jobs::Status::Running:
            return "Running";
        case Jobs::Status::Done:
            return "Done";
        case Jobs::Status::Terminated:
            return "Terminated";
        case Jobs::Status::Stopped:
            return "Stopped";
    }

    return "Unknown";
}

Jobs::Job* findByPid(pid_t pid) {
    for (auto& job : backgroundJobs) {
        if (job.pid == pid) {
            return &job;
        }
    }

    return nullptr;
}

bool isActive(Jobs::Status status) {
    return status == Jobs::Status::Running || status == Jobs::Status::Stopped;
}

std::string getCommandName(const std::string& command) {
    size_t firstSpace = command.find(' ');
    if (firstSpace == std::string::npos) {
        return command;
    }

    return command.substr(0, firstSpace);
}
}

pid_t Jobs::getPidByJobId(int jobId) {
    for (auto& job : backgroundJobs) {
        if (job.id == jobId && isActive(job.status)) {
            return job.pid;
        }
    }

    return -1;
}

std::vector<pid_t> Jobs::findPidsByCommandName(const std::string& name) {
    std::vector<pid_t> pid_list;

    for (auto& job : backgroundJobs) {
        if (getCommandName(job.command) == name && isActive(job.status)) {
            pid_list.push_back(job.pid);
        }
    }


    return pid_list;
}

bool Jobs::setStatusByPid(pid_t pid, Status status) {
    Job* job = findByPid(pid);
    if (job == nullptr) {
        return false;
    }

    job->status = status;
    return true;
}

int Jobs::add(pid_t pid, const std::string& command) {
    int jobId = nextJobId++;
    backgroundJobs.push_back({jobId, pid, Status::Running, command});
    return jobId;
}

void Jobs::reap(bool notify) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        Job* job = findByPid(pid);
        if (job == nullptr) {
            continue;
        }

        if (WIFEXITED(status)) {
            job->status = Status::Done;
        }
        else if (WIFSIGNALED(status)) {
            job->status = Status::Terminated;
        }
        else if (WIFSTOPPED(status)) {
            job->status = Status::Stopped;
        }
        else if (WIFCONTINUED(status)) {
            job->status = Status::Running;
        }
        else {
            job->status = Status::Stopped;
        }

        if (notify) {
            std::cout << "[" << job->id << "] "
                      << statusToString(job->status) << "\t"
                      << job->command << std::endl;
        }
    }
}

void Jobs::print() {
    if (backgroundJobs.empty()) {
        std::cout << "No background jobs" << std::endl;
        return;
    }

    std::cout << "JOB\tPID\tSTATUS\t\tCOMMAND" << std::endl;
    for (const auto& job : backgroundJobs) {
        std::cout << "[" << job.id << "]\t"
                  << job.pid << "\t"
                  << statusToString(job.status) << "\t\t"
                  << job.command << std::endl;
    }
}
