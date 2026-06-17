#include "parser.h"
#include "builtins.h"
#include "config.h"
#include "executor.h"
#include "history.h"
#include "jobs.h"
#include "command.h"
#include "path_utils.h"
#include "globals.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

std::string getCurrentDirectory() {
    char buffer[1024];

    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    }

    perror("getcwd");
    return "";
}

std::string buildPrompt() {
    std::string shellName = Config::get("name");
    std::string shellColor = Config::ansiColorCode(Config::get("color"));
    std::string prompt;

    if (!shellColor.empty()) {
        prompt += shellColor + shellName + Config::resetColorCode();
    }
    else {
        prompt += shellName;
    }

    prompt += " " + getCurrentDirectory() + " > ";
    return prompt;
}

void printPrompt() {
    std::cout << buildPrompt();
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

bool isScriptCommand(const std::string& input) {
    std::string trimmed = trimLine(input);

    if (trimmed.empty()) {
        return false;
    }

    // Chỉ nhận dạng lệnh kiểu:
    // ./test.sh
    // /home/user/test.sh
    //
    // Không xử lý:
    // ./test.sh arg1 arg2
    if (trimmed.find(' ') != std::string::npos || trimmed.find('\t') != std::string::npos) {
        return false;
    }

    if (!hasShellScriptExtension(trimmed)) {
        return false;
    }

    return trimmed.rfind("./", 0) == 0 || trimmed[0] == '/';
}

void redrawInputLine(const std::string& prompt, const std::string& line) {
    std::cout << "\r\033[2K" << prompt << line << std::flush;
}

bool readInteractiveInputLine(const std::string& prompt, std::string& input) {
    if (!isatty(STDIN_FILENO)) {
        return static_cast<bool>(std::getline(std::cin, input));
    }

    termios originalMode {};
    if (tcgetattr(STDIN_FILENO, &originalMode) < 0) {
        return static_cast<bool>(std::getline(std::cin, input));
    }

    termios rawMode = originalMode;
    rawMode.c_lflag &= static_cast<unsigned int>(~(ECHO | ICANON));
    rawMode.c_cc[VMIN] = 1;
    rawMode.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawMode) < 0) {
        return static_cast<bool>(std::getline(std::cin, input));
    }

    auto restoreTerminal = [&originalMode]() {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalMode);
    };

    input.clear();
    std::string draftLine;
    std::vector<std::string> historyEntries = History::getEntries();
    size_t historyIndex = historyEntries.size();

    while (true) {
        char c;
        ssize_t bytesRead = read(STDIN_FILENO, &c, 1);

        if (bytesRead <= 0) {
            restoreTerminal();
            return false;
        }

        if (c == '\n' || c == '\r') {
            restoreTerminal();
            std::cout << std::endl;
            return true;
        }

        if (c == 4) {
            if (input.empty()) {
                restoreTerminal();
                return false;
            }
            continue;
        }

        if (c == 8 || c == 127) {
            if (!input.empty()) {
                input.pop_back();
                redrawInputLine(prompt, input);
            }
            continue;
        }

        if (c == '\033') {
            char sequence[2];
            if (read(STDIN_FILENO, &sequence[0], 1) <= 0 ||
                read(STDIN_FILENO, &sequence[1], 1) <= 0) {
                continue;
            }

            if (sequence[0] != '[') {
                continue;
            }

            if (sequence[1] == 'A') {
                historyEntries = History::getEntries();
                if (historyEntries.empty()) {
                    continue;
                }

                if (historyIndex > historyEntries.size()) {
                    historyIndex = historyEntries.size();
                }

                if (historyIndex == historyEntries.size()) {
                    draftLine = input;
                }

                if (historyIndex > 0) {
                    --historyIndex;
                    input = historyEntries[historyIndex];
                    redrawInputLine(prompt, input);
                }
                continue;
            }

            if (sequence[1] == 'B') {
                historyEntries = History::getEntries();
                if (historyIndex < historyEntries.size()) {
                    ++historyIndex;
                    if (historyIndex == historyEntries.size()) {
                        input = draftLine;
                    }
                    else {
                        input = historyEntries[historyIndex];
                    }
                    redrawInputLine(prompt, input);
                }
                continue;
            }

            continue;
        }

        if (std::isprint(static_cast<unsigned char>(c))) {
            input += c;
            std::cout << c << std::flush;
        }
    }
}

int runScriptFile(const std::string& scriptPath);

int executeInputLine(const std::string& input) {
    std::string trimmed = trimLine(input);

    if (trimmed.empty()) {
        return 0;
    }

    // Cho phép chạy script trong tinyshell bằng:
    // ./test.sh
    if (isScriptCommand(trimmed)) {
        return runScriptFile(trimmed);
    }

    Command* parsedCommand = parser->parse(trimmed);

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
    History::initialize(path_utils::resolveShellFilePath("history.txt"));
    Executor::initializeShellJobControl();

    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [script.sh]" << std::endl;
        return 1;
    }

    // Vẫn giữ hỗ trợ chạy kiểu:
    // ./tinyshell test.sh
    if (argc == 2) {
        return runScriptFile(argv[1]);
    }

    std::cout << "Welcome to " << Config::get("name") << "!" << std::endl;

    std::string input;

    while (true) {
        Jobs::reap();

        std::string prompt = buildPrompt();
        std::cout << prompt << std::flush;

        if (!readInteractiveInputLine(prompt, input)) {
            std::cout << std::endl;
            break;
        }

        History::add(trimLine(input));
        executeInputLine(input);
    }

    return 0;
}
