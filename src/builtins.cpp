#include "builtins.h"
#include <iostream>
#include <unistd.h>

Builtins::Builtins() = default;


const std::map<std::string, std::function<int(const Command&)>> Builtins::builtinCommands = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"clear", builtin_clear},
    {"help", builtin_help}
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
    return 0;
}

int Builtins::builtin_exit(const Command& cmd) {
    (void)cmd;
    exit(0);
    return 0; // This line will never be reached, but it's here to satisfy the return type
}

int Builtins::builtin_clear(const Command& cmd) {
    (void)cmd;
    std::cout << "\033[2J\033[1;1H"; // ANSI escape code to clear the screen
    
    return 0;
}

int Builtins::builtin_help(const Command& cmd) {
    (void)cmd;
    std::cout << "===================== BUILTIN COMMANDS =====================" << std::endl;
    std::cout << "cd: Change the current directory." << std::endl;
    std::cout << "exit: Exit the shell." << std::endl;
    std::cout << "clear: Clear the terminal screen." << std::endl;
    std::cout << "help: Display this help message." << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}

