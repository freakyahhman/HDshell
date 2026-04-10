#include "parser.h"
#include <iostream>

Command* Parser::parse(const std::string& input) {
    std::string trimmedInput = trim(input);
    if (trimmedInput.empty()) {
        return nullptr; // Tra ve nullptr neu input rong
    }

    std::vector<std::string> tokens = tokenize(trimmedInput);
    handleSpecialOperators(tokens);

    if (tokens[0] == "exit") {
        return new ExitCommand(tokens);
    }
    return nullptr;
}

std::vector<std::string> Parser::tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;
    std::stack<char> quoteStack;
    for (char c : input) {
        if (c == ' ' && quoteStack.empty()) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
        else if (c == '"' || c == '\\') {
            if (!quoteStack.empty() && quoteStack.top() == c) {
                quoteStack.pop();
            }
            else {
                quoteStack.push(c);
            }
        }
        else {
            token += c;
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

void Parser::handleSpecialOperators(std::vector<std::string>& tokens) {
    // TODO: handle redirection (<, >) and pipe (|)
    (void)tokens;
}

std::string Parser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');
    if (first == std::string::npos || last == std::string::npos) {
        return ""; // Tra ve chuoi rong neu chi co space
    }
    return str.substr(first, last - first + 1);
}