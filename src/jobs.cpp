#include "jobs.h"
#include <algorithm>
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

bool containsPid(const Jobs::Job& job, pid_t pid) {
    if (job.pid == pid) {
        return true;
    }

    return std::find(job.pids.begin(), job.pids.end(), pid) != job.pids.end();
}

void erasePid(Jobs::Job& job, pid_t pid) {
    job.pids.erase(std::remove(job.pids.begin(), job.pids.end(), pid), job.pids.end());
}

Jobs::Job* findByPid(pid_t pid) {
    for (auto& job : backgroundJobs) {
        if (containsPid(job, pid)) {
            return &job;
        }
    }

    return nullptr;
}

std::string getCommandName(const std::string& command) {
    size_t firstSpace = command.find(' ');
    if (firstSpace == std::string::npos) {
        return command;
    }

    return command.substr(0, firstSpace);
}
}

bool Jobs::isActive(Status status) {
    return status == Status::Running || status == Status::Stopped;
}

bool Jobs::isFinished(Status status) {
    return status == Status::Done || status == Status::Terminated;
}

pid_t Jobs::getPidByJobId(int jobId) {
    for (auto& job : backgroundJobs) {
        if (job.id == jobId && Jobs::isActive(job.status)) {
            return job.pid;
        }
    }

    return -1;
}

std::vector<pid_t> Jobs::findPidsByCommandName(const std::string& name) {
    std::vector<pid_t> pid_list;

    for (auto& job : backgroundJobs) {
        if (getCommandName(job.command) == name && Jobs::isActive(job.status)) {
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
    return add(pid, command, Status::Running, std::vector<pid_t>{pid});
}

int Jobs::add(pid_t pid, const std::string& command, Status status, const std::vector<pid_t>& pids) {
    int jobId = nextJobId++;
    std::vector<pid_t> trackedPids = pids.empty() ? std::vector<pid_t>{pid} : pids;
    backgroundJobs.push_back({jobId, pid, status, command, trackedPids});
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
            erasePid(*job, pid);
            if (job->pids.empty()) {
                job->status = Status::Done;
            }
        }
        else if (WIFSIGNALED(status)) {
            erasePid(*job, pid);
            if (job->pids.empty()) {
                job->status = Status::Terminated;
            }
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

    std::cout << "JOB\tPGID\tSTATUS\t\tCOMMAND" << std::endl;
    for (const auto& job : backgroundJobs) {
        std::cout << "[" << job.id << "]\t"
                  << job.pid << "\t"
                  << statusToString(job.status) << "\t\t"
                  << job.command << std::endl;
    }
}

bool Jobs::getJobByJobId(int jobId, Job& job) {
    for (const auto& currentJob : backgroundJobs) {
        if (currentJob.id == jobId) {
            job = currentJob;
            return true;
        }
    }

    return false;
}

bool Jobs::getJobByPid(pid_t pid, Job& job) {
    for (const auto& currentJob : backgroundJobs) {
        if (containsPid(currentJob, pid)) {
            job = currentJob;
            return true;
        }
    }

    return false;
}

bool Jobs::getMostRecentJob(Job& job) {
    for (auto it = backgroundJobs.rbegin(); it != backgroundJobs.rend(); ++it) {
        if (Jobs::isActive(it->status)) {
            job = *it;
            return true;
        }
    }

    return false;
}

bool Jobs::removeByJobId(int jobId, bool onlyFinished) {
    for (auto it = backgroundJobs.begin(); it != backgroundJobs.end(); ++it) {
        if (it->id == jobId) {
            if (onlyFinished && !Jobs::isFinished(it->status)) {
                return false;
            }

            backgroundJobs.erase(it);
            return true;
        }
    }

    return false;
}

bool Jobs::removeByPid(pid_t pid, bool onlyFinished) {
    for (auto it = backgroundJobs.begin(); it != backgroundJobs.end(); ++it) {
        if (containsPid(*it, pid)) {
            if (onlyFinished && !Jobs::isFinished(it->status)) {
                return false;
            }

            backgroundJobs.erase(it);
            return true;
        }
    }

    return false;
}

int Jobs::removeFinished() {
    int removed = 0;

    for (auto it = backgroundJobs.begin(); it != backgroundJobs.end();) {
        if (Jobs::isFinished(it->status)) {
            it = backgroundJobs.erase(it);
            ++removed;
        }
        else {
            ++it;
        }
    }

    return removed;
}
