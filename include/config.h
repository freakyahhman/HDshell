#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace Config {

bool load(const std::string& filename);
std::string get(const std::string& key);
bool set(const std::string& key, const std::string& value);
bool isValidColor(const std::string& color);
std::string ansiColorCode(const std::string& color);
std::string resetColorCode();

}

#endif // CONFIG_H
