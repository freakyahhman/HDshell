#include "parser.h"
#include "builtins.h"
#include "config.h"
#include "executor.h"
#include "jobs.h"
#include "command.h"
#include "path_utils.h"
#include "globals.h"
#include <iostream>
#include <string>
#include <unistd.h>

std::string getCurrentDirectory() {
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    } else {
        perror("getcwd");
        return "";
    }
}

void printPrompt() {
    std::string shellName = Config::get("name");
    std::string shellColor = Config::ansiColorCode(Config::get("color"));

    if (!shellColor.empty()) {
        std::cout << shellColor << shellName << Config::resetColorCode();
    }
    else {
        std::cout << shellName;
    }

    std::cout << " " << getCurrentDirectory() << " > ";
}

std::unique_ptr<Executor> executor = std::make_unique<Executor>();
std::unique_ptr<Parser> parser = std::make_unique<Parser>();

int main() {
    Config::load(path_utils::resolveDataFilePath("config.json"));
    std::cout << "Welcome to " << Config::get("name") << "!" << std::endl;
    std::string input;
    while (true) {
        Jobs::reap();
        printPrompt();
        std::getline(std::cin, input);
        Command* cmd = parser->parse(input);
        if (cmd) {
            executor->executeCommand(std::unique_ptr<Command>(cmd));
        }
    }
}
