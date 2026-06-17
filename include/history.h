#ifndef HISTORY_H
#define HISTORY_H

#include <cstddef>
#include <string>
#include <vector>

namespace History {

void initialize(const std::string& filePath);
void sync();
void add(const std::string& line);
void print();
void clear();
bool setLimit(size_t limit);
size_t getLimit();
size_t maximumLimit();
std::vector<std::string> getEntries();

}

#endif // HISTORY_H
