#include "parser.h"
#include "builtins.h"
#include "executor.h"
#include "command.h"
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <unistd.h>


// doc du lieu tu file config.json
std::map<std::string, std::string> configData;
void readJsonFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Parse the line and store it in the configData map
        // Example: {"name": "MyShell"} -> configData["name"] = "MyShell"
        size_t keyStartPos = line.find('"');
        if (keyStartPos == std::string::npos) continue;
        size_t keyStart = keyStartPos + 1;
        size_t keyEnd = line.find('"', keyStart);
        if (keyEnd == std::string::npos) continue;
        size_t colonPos = line.find('"', keyEnd + 1);
        if (colonPos == std::string::npos) continue;
        size_t valueStart = colonPos + 1;
        size_t valueEnd = line.find('"', valueStart);
        if (valueEnd == std::string::npos) continue;
        std::string key = line.substr(keyStart, keyEnd - keyStart);
        std::string value = line.substr(valueStart, valueEnd - valueStart);
        configData[key] = value;
    }
}

std::string getConfigValue(const std::string& key) {
    auto it = configData.find(key);
    if (it != configData.end()) {
        return it->second;
    }
    return "";
}

std::string getCurrentDirectory() {
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    } else {
        perror("getcwd");
        return "";
    }
}

int main() {
    readJsonFile("data/config.json");
    std::cout << "Welcome to " << getConfigValue("name") << "!" << std::endl;
    std::string input;
    while (true) {
        std::cout << getConfigValue("name") << " " << getCurrentDirectory() << " > ";
        std::getline(std::cin, input);
        Command* cmd = Parser::parse(input);
        if (cmd) {
            Executor::executeCommand(std::unique_ptr<Command>(cmd));
        }
    }
}