#include "command.h"
#include "builtins.h"
#include "globals.h"


int SimpleCommand::execute() {
    if (name.empty()) {
        return 0; // Neu khong co lenh nao duoc nhap, tra ve 0
    }

    if (Builtins::isBuiltin(name)) {
        Builtins builtins;
        exit_code = builtins.executeBuiltin(*this);
        return exit_code; // Tra ve exit code cua lenh builtin
    }

    return executor->handleFork(this); // Neu khong phai builtin, thuc thi lenh binh thuong
}


int PipeCommand::execute() {
    // Ham nay se duoc su dung de thuc thi cac lenh co pipe
    // (chua hoan thien)
    return 0;
}