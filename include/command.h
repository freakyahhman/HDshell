#ifndef COMMAND_H
#define COMMAND_H

#include <vector>
#include <string>
#include <iostream>
#include <memory>

class Command {
public:
    virtual ~Command() {};
    Command(const std::vector<std::string>& tokens) : tokens(tokens) {};
    virtual int execute() = 0; // Phuong thuc thuc thi lenh, duoc override boi cac lop con
    int exit_code = 0; //exit_code

protected:
    std::vector<std::string> tokens;
};


//SimpleCommand: 1 lenh don le, khong co pipe

class SimpleCommand : public Command {
public:
    std::string name;
    std::vector<std::string> args;
    std::string input_file;
    std::string output_file;
    bool run_in_background = false;

    SimpleCommand(const std::vector<std::string>& tokens) : Command(tokens) {
        if (!tokens.empty()) {
            name = tokens[0];
            args.assign(tokens.begin() + 1, tokens.end());
        }
        input_file = "";
        output_file = "";
    }

    int execute() override;

    void printCommandInfo() const {
        std::cerr << "Command Name: " << name << std::endl;
        std::cerr << "Arguments: ";
        for (const auto& arg : args) {
            std::cerr << arg << " ";
        }
        std::cerr << std::endl;
        std::cerr << "Input File: " << (input_file.empty() ? "None" : input_file) << std::endl;
        std::cerr << "Output File: " << (output_file.empty() ? "None" : output_file) << std::endl;
    }

};


//PipeCommand: Kieu nhu la g++ -o main main.cpp | ./main

class PipeCommand : public Command {
public:
    std::vector<std::unique_ptr<Command>> subcommands;

    PipeCommand(const std::vector<std::string>& tokens) : Command(tokens) {}

    int execute() override;
};

#endif // COMMAND_H
