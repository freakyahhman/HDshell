#include "command.h"
#include "builtins.h"
#include "globals.h"
#include <cstdio>
#include <unistd.h>


int SimpleCommand::execute() {
    if (name.empty()) {
        return 0; // Neu khong co lenh nao duoc nhap, tra ve 0
    }

    if (Builtins::isBuiltin(name)) {
        int savedStdin = -1;
        int savedStdout = -1;

        if (!input_file.empty()) {
            savedStdin = dup(STDIN_FILENO);
            if (savedStdin < 0) {
                perror("dup");
                return -1;
            }
        }

        if (!output_file.empty()) {
            savedStdout = dup(STDOUT_FILENO);
            if (savedStdout < 0) {
                perror("dup");
                if (savedStdin != -1) {
                    close(savedStdin);
                }
                return -1;
            }
        }

        if (Executor::setupRedirection(*this) != 0) {
            if (savedStdin != -1) {
                dup2(savedStdin, STDIN_FILENO);
                close(savedStdin);
            }
            if (savedStdout != -1) {
                dup2(savedStdout, STDOUT_FILENO);
                close(savedStdout);
            }
            return -1;
        }

        Builtins builtins;
        exit_code = builtins.executeBuiltin(*this);

        if (savedStdin != -1) {
            if (dup2(savedStdin, STDIN_FILENO) < 0) {
                perror("dup2");
            }
            close(savedStdin);
        }

        if (savedStdout != -1) {
            if (dup2(savedStdout, STDOUT_FILENO) < 0) {
                perror("dup2");
            }
            close(savedStdout);
        }

        return exit_code; // Tra ve exit code cua lenh builtin
    }

    return executor->handleFork(this); // Neu khong phai builtin, thuc thi lenh binh thuong
}


int PipeCommand::execute() {
    // Ham nay se duoc su dung de thuc thi cac lenh co pipe
    // (chua hoan thien)
    return 0;
}
