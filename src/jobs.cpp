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
}

int Jobs::add(pid_t pid, const std::string& command) {
    int jobId = nextJobId++;
    backgroundJobs.push_back({jobId, pid, Status::Running, command});
    return jobId;
}

void Jobs::reap(bool notify) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
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
