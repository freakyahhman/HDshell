#include "command.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

int SimpleCommand::execute() {
    //Not now
    return 0;
}

int ExitCommand::execute() {
    exit_code = 0; // Co the them logic de xac dinh exit_code tu tokens neu can
    exit(0); // Thoat khoi shell
}