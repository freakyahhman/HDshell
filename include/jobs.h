#ifndef JOBS_H
#define JOBS_H

#include <string>
#include <sys/types.h>
#include <vector>
namespace Jobs {

enum class Status {
    Running,
    Done,
    Terminated,
    Stopped
};

struct Job {
    int id;
    pid_t pid;
    Status status;
    std::string command;
};

int add(pid_t pid, const std::string& command);
void reap(bool notify = true);
void print();
pid_t getPidByJobId(int jobId);
std::vector<pid_t> findPidsByCommandName(const std::string& name);
bool setStatusByPid(pid_t pid, Status status);
}

#endif // JOBS_H
