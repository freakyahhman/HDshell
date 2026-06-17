#include "history.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr size_t kMaximumHistoryLimit = 1000;
std::vector<std::string> entries;
std::string historyPath;
size_t currentLimit = kMaximumHistoryLimit;

std::vector<std::string> lastLines(const std::vector<std::string>& lines, size_t limit) {
    if (lines.size() <= limit) {
        return lines;
    }

    return std::vector<std::string>(lines.end() - static_cast<long>(limit), lines.end());
}

std::vector<std::string> parseLines(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    return lines;
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::string content;

    for (const std::string& line : lines) {
        content += line;
        content += '\n';
    }

    return content;
}

bool lockFile(int fd, int operation) {
    while (flock(fd, operation) < 0) {
        if (errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}

int openHistoryFile() {
    if (historyPath.empty()) {
        return -1;
    }

    int fd = open(historyPath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror(historyPath.c_str());
    }

    return fd;
}

bool readFileContent(int fd, std::string& content) {
    content.clear();

    if (lseek(fd, 0, SEEK_SET) < 0) {
        return false;
    }

    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, static_cast<size_t>(bytesRead));
    }

    return bytesRead == 0;
}

bool writeFileLines(int fd, const std::vector<std::string>& lines) {
    std::string content = joinLines(lines);

    if (ftruncate(fd, 0) < 0) {
        return false;
    }

    if (lseek(fd, 0, SEEK_SET) < 0) {
        return false;
    }

    const char* data = content.c_str();
    size_t remaining = content.size();

    while (remaining > 0) {
        ssize_t written = write(fd, data, remaining);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        data += written;
        remaining -= static_cast<size_t>(written);
    }

    return true;
}

void setEntriesFromAllLines(const std::vector<std::string>& lines) {
    entries = lastLines(lines, currentLimit);
}

bool syncLocked(int fd) {
    std::string content;
    if (!readFileContent(fd, content)) {
        return false;
    }

    std::vector<std::string> allLines = parseLines(content);

    if (allLines.size() > kMaximumHistoryLimit) {
        allLines = lastLines(allLines, kMaximumHistoryLimit);
        if (!writeFileLines(fd, allLines)) {
            return false;
        }
    }

    setEntriesFromAllLines(allLines);
    return true;
}

void trimMemoryOnly() {
    entries = lastLines(entries, currentLimit);
}
}

void History::initialize(const std::string& filePath) {
    historyPath = filePath;
    sync();
}

void History::add(const std::string& line) {
    if (line.empty()) {
        return;
    }

    if (historyPath.empty()) {
        entries.push_back(line);
        trimMemoryOnly();
        return;
    }

    int fd = openHistoryFile();
    if (fd < 0) {
        entries.push_back(line);
        trimMemoryOnly();
        return;
    }

    if (!lockFile(fd, LOCK_EX)) {
        close(fd);
        return;
    }

    std::string content;
    std::vector<std::string> allLines;
    if (readFileContent(fd, content)) {
        allLines = parseLines(content);
    }

    allLines.push_back(line);
    allLines = lastLines(allLines, kMaximumHistoryLimit);

    if (!writeFileLines(fd, allLines)) {
        perror(historyPath.c_str());
    }

    setEntriesFromAllLines(allLines);
    flock(fd, LOCK_UN);
    close(fd);
}

void History::print() {
    sync();

    for (size_t i = 0; i < entries.size(); ++i) {
        std::cout << i + 1 << "\t" << entries[i] << std::endl;
    }
}

void History::clear() {
    entries.clear();

    if (historyPath.empty()) {
        return;
    }

    int fd = openHistoryFile();
    if (fd < 0) {
        return;
    }

    if (!lockFile(fd, LOCK_EX)) {
        close(fd);
        return;
    }

    if (ftruncate(fd, 0) < 0) {
        perror(historyPath.c_str());
    }

    flock(fd, LOCK_UN);
    close(fd);
}

bool History::setLimit(size_t limit) {
    if (limit == 0 || limit > kMaximumHistoryLimit) {
        return false;
    }

    currentLimit = limit;
    sync();
    return true;
}

size_t History::getLimit() {
    return currentLimit;
}

size_t History::maximumLimit() {
    return kMaximumHistoryLimit;
}

std::vector<std::string> History::getEntries() {
    sync();
    return entries;
}

void History::sync() {
    if (historyPath.empty()) {
        trimMemoryOnly();
        return;
    }

    int fd = openHistoryFile();
    if (fd < 0) {
        return;
    }

    if (!lockFile(fd, LOCK_EX)) {
        close(fd);
        return;
    }

    if (!syncLocked(fd)) {
        perror(historyPath.c_str());
    }

    flock(fd, LOCK_UN);
    close(fd);
}
