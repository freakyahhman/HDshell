#include "config.h"
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

namespace {
std::map<std::string, std::string> configData;
std::string configFilePath;

bool parseStringPair(const std::string& line, std::string& key, std::string& value) {
    size_t keyStartPos = line.find('"');
    if (keyStartPos == std::string::npos) {
        return false;
    }

    size_t keyStart = keyStartPos + 1;
    size_t keyEnd = line.find('"', keyStart);
    if (keyEnd == std::string::npos) {
        return false;
    }

    size_t colonPos = line.find(':', keyEnd + 1);
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t valueStartPos = line.find('"', colonPos + 1);
    if (valueStartPos == std::string::npos) {
        return false;
    }

    size_t valueStart = valueStartPos + 1;
    size_t valueEnd = line.find('"', valueStart);
    if (valueEnd == std::string::npos) {
        return false;
    }

    key = line.substr(keyStart, keyEnd - keyStart);
    value = line.substr(valueStart, valueEnd - valueStart);
    return true;
}

bool isStringKeyLine(const std::string& line, const std::string& key) {
    std::string parsedKey;
    std::string parsedValue;
    return parseStringPair(line, parsedKey, parsedValue) && parsedKey == key;
}

std::string escapeJsonString(const std::string& value) {
    std::string escaped;
    for (char c : value) {
        if (c == '"' || c == '\\') {
            escaped += '\\';
        }
        escaped += c;
    }

    return escaped;
}

std::string buildStringLine(const std::string& oldLine, const std::string& key, const std::string& value) {
    size_t indentEnd = oldLine.find_first_not_of(" \t");
    std::string indent = indentEnd == std::string::npos ? "" : oldLine.substr(0, indentEnd);
    bool hasComma = oldLine.find(',') != std::string::npos;

    return indent + "\"" + key + "\": \"" + escapeJsonString(value) + "\"" + (hasComma ? "," : "");
}

std::string toLower(std::string value) {
    for (char& c : value) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }

    return value;
}
}

bool Config::load(const std::string& filename) {
    configFilePath = filename;
    configData.clear();

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string key;
        std::string value;
        if (parseStringPair(line, key, value)) {
            configData[key] = value;
        }
    }

    return true;
}

std::string Config::get(const std::string& key) {
    auto it = configData.find(key);
    if (it != configData.end()) {
        return it->second;
    }

    return "";
}

bool Config::set(const std::string& key, const std::string& value) {
    if (configFilePath.empty()) {
        return false;
    }

    std::ifstream input(configFilePath);
    if (!input.is_open()) {
        std::cerr << "Failed to open config file: " << configFilePath << std::endl;
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    input.close();

    bool replaced = false;
    for (std::string& currentLine : lines) {
        if (isStringKeyLine(currentLine, key)) {
            currentLine = buildStringLine(currentLine, key, value);
            replaced = true;
            break;
        }
    }

    if (!replaced && key == "color") {
        for (auto it = lines.begin(); it != lines.end(); ++it) {
            if (isStringKeyLine(*it, "name")) {
                lines.insert(it + 1, "    \"color\": \"" + escapeJsonString(value) + "\",");
                replaced = true;
                break;
            }
        }
    }

    if (!replaced) {
        std::cerr << "Config key not found: " << key << std::endl;
        return false;
    }

    std::ofstream output(configFilePath);
    if (!output.is_open()) {
        std::cerr << "Failed to write config file: " << configFilePath << std::endl;
        return false;
    }

    for (const std::string& outputLine : lines) {
        output << outputLine << '\n';
    }

    configData[key] = value;
    return true;
}

bool Config::isValidColor(const std::string& color) {
    std::string normalized = toLower(color);
    return normalized == "default" ||
           normalized == "black" ||
           normalized == "red" ||
           normalized == "green" ||
           normalized == "yellow" ||
           normalized == "blue" ||
           normalized == "magenta" ||
           normalized == "cyan" ||
           normalized == "white";
}

std::string Config::ansiColorCode(const std::string& color) {
    std::string normalized = toLower(color);

    if (normalized == "black") return "\033[30m";
    if (normalized == "red") return "\033[31m";
    if (normalized == "green") return "\033[32m";
    if (normalized == "yellow") return "\033[33m";
    if (normalized == "blue") return "\033[34m";
    if (normalized == "magenta") return "\033[35m";
    if (normalized == "cyan") return "\033[36m";
    if (normalized == "white") return "\033[37m";

    return "";
}

std::string Config::resetColorCode() {
    return "\033[0m";
}
