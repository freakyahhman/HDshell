#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#define MAX_ARGS 128
using namespace std;

struct Command {
    char *name;
    char *args[MAX_ARGS];
    int argc;
    char *input_file;
    char *output_file;
    bool is_background;
    int exit_code;
};

struct ShellState {
    Command last_command;
    Command hint_command;
    int last_exit_code;
};

int main() {
    ShellState shell_state;
    shell_state.last_exit_code = 0;

    while (true) {
        cout << "$ > ";
        string input;
        getline(cin, input);

        if (input.empty()) {
            continue;
        }
    }
}