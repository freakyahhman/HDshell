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
    std::vector<pid_t> pids;
};

int add(pid_t pid, const std::string& command);
int add(pid_t pid, const std::string& command, Status status, const std::vector<pid_t>& pids);
void reap(bool notify = true);
void print();
pid_t getPidByJobId(int jobId);
std::vector<pid_t> findPidsByCommandName(const std::string& name);
bool setStatusByPid(pid_t pid, Status status);
bool getJobByJobId(int jobId, Job& job);
bool getJobByPid(pid_t pid, Job& job);
bool getMostRecentJob(Job& job);
bool removeByJobId(int jobId, bool onlyFinished = true);
bool removeByPid(pid_t pid, bool onlyFinished = true);
int removeFinished();
bool isActive(Status status);
bool isFinished(Status status);
}

#endif // JOBS_H
