#include "parser.h"
#include "builtins.h"
#include "config.h"
#include "executor.h"
#include "jobs.h"
#include "command.h"
#include "path_utils.h"
#include "globals.h"
#include <cctype>
#include <fstream>
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

namespace {
std::string trimLine(const std::string& line) {
    size_t start = 0;
    while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
        ++start;
    }

    size_t end = line.size();
    while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
        --end;
    }

    return line.substr(start, end - start);
}

bool hasShellScriptExtension(const std::string& path) {
    const std::string extension = ".sh";
    if (path.size() < extension.size()) {
        return false;
    }

    return path.compare(path.size() - extension.size(), extension.size(), extension) == 0;
}

bool shouldSkipScriptLine(const std::string& line) {
    std::string trimmed = trimLine(line);
    return trimmed.empty() || trimmed.rfind("#!", 0) == 0 || trimmed[0] == '#';
}

int executeInputLine(const std::string& input) {
    Command* parsedCommand = parser->parse(input);
    if (parsedCommand == nullptr) {
        return 0;
    }

    std::unique_ptr<Command> command(parsedCommand);
    int exitCode = command->execute();
    command->exit_code = exitCode;
    return exitCode;
}

int runScriptFile(const std::string& scriptPath) {
    if (!hasShellScriptExtension(scriptPath)) {
        std::cerr << "Error: script file must have .sh extension" << std::endl;
        return 1;
    }

    std::ifstream script(scriptPath);
    if (!script.is_open()) {
        perror(scriptPath.c_str());
        return 1;
    }

    std::string line;
    int lastExitCode = 0;
    while (std::getline(script, line)) {
        Jobs::reap();
        if (shouldSkipScriptLine(line)) {
            continue;
        }

        lastExitCode = executeInputLine(line);
    }
    Jobs::reap();

    return lastExitCode < 0 ? 1 : lastExitCode;
}
}

int main(int argc, char* argv[]) {
    Config::load(path_utils::resolveDataFilePath("config.json"));

    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [script.sh]" << std::endl;
        return 1;
    }

    if (argc == 2) {
        return runScriptFile(argv[1]);
    }

    std::cout << "Welcome to " << Config::get("name") << "!" << std::endl;
    std::string input;
    while (true) {
        Jobs::reap();
        printPrompt();
        if (!std::getline(std::cin, input)) {
            std::cout << std::endl;
            break;
        }
        executeInputLine(input);
    }

    return 0;
}
