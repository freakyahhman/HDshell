#include "builtins.h"
#include "path_utils.h"
#include <fstream>
#include <string>
#include <iostream>
#include <unistd.h>

Builtins::Builtins() = default;


const std::map<std::string, std::function<int(const Command&)>> Builtins::builtinCommands = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"clear", builtin_clear},
    {"help", builtin_help},
    {"father", builtin_father},
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

