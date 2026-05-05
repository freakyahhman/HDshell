#ifndef BUILTINS_H
#define BUILTINS_H

#include <string>
#include <vector>
#include <functional>
#include <map>
#include "command.h"


class Builtins {
public:
    Builtins();
    static bool isBuiltin(const std::string& cmd);
    int executeBuiltin(const Command& cmd) const;
private:
    static const std::map<std::string, std::function<int(const Command&)>> builtinCommands;
    static int builtin_cd(const Command& cmd);
    static int builtin_exit(const Command& cmd);
    static int builtin_clear(const Command& cmd);
    static int builtin_help(const Command& cmd);
    static int builtin_father(const Command& cmd);
    static int builtin_ps(const Command& cmd);
    static int builtin_jobs(const Command& cmd);
    static int builtin_kill(const Command& cmd);
    static int builtin_killall(const Command& cmd);
    static int builtin_stop(const Command& cmd);
    static int builtin_resume(const Command& cmd);
    static int builtin_change(const Command& cmd);
};

#endif  //BUILTINS_H
