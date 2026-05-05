#ifndef JOBS_H
#define JOBS_H

#include <string>
#include <sys/types.h>

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

}

#endif // JOBS_H
