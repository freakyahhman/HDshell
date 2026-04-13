#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <cstdlib>
#include <filesystem>
#include <limits.h>
#include <string>
#include <unistd.h>

namespace path_utils {
inline std::string resolveDataFilePath(const std::string& fileName) {
    namespace fs = std::filesystem;

    const char* rootEnv = std::getenv("HDSHELL_ROOT");
    if (rootEnv != nullptr) {
        fs::path envPath = fs::path(rootEnv) / "data" / fileName;
        if (fs::exists(envPath)) {
            return envPath.string();
        }
    }

    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        fs::path exeDir = fs::path(exePath).parent_path();
        fs::path fromExe = exeDir / "data" / fileName;
        if (fs::exists(fromExe)) {
            return fromExe.string();
        }
    }

    return (fs::current_path() / "data" / fileName).string();
}
}  // namespace path_utils

#endif  // PATH_UTILS_H
