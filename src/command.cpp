#include "command.h"
#include "builtins.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

int SimpleCommand::execute() {
    if (name.empty()) {
        return 0; // Neu khong co lenh nao duoc nhap, tra ve 0
    }

    if (Builtins::isBuiltin(name)) {
        Builtins builtins;
        exit_code = builtins.executeBuiltin(*this);
        return exit_code; // Tra ve exit code cua lenh builtin
    }

    int pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        return -1;
    }
    else if (pid == 0) {
        // Process con
        // Thiet lap redirection neu can thiet
        if (!input_file.empty()) {
            freopen(input_file.c_str(), "r", stdin);
        }
        if (!output_file.empty()) {
            freopen(output_file.c_str(), "w", stdout);
        }

        // Chuyen doi args sang dang char*[]
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(name.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr); // Ket thuc mang args bang nullptr

        execvp(name.c_str(), argv.data());
        perror("execvp");
        _exit(127);
    }
    else {
        // Process cha
        int status;
        waitpid(pid, &status, 0); // Cho process con ket thuc
        if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status); // Lay exit code cua process con
        }
        else {
            exit_code = -1; // Neu process con bi loi, tra ve -1
        }
    }

    return 0;
}
