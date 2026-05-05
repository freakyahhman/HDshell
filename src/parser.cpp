#include "parser.h"
#include <iostream>

bool isPipeCommand(const std::vector<std::string>& tokens) {
    for (const auto& token : tokens) {
        if (token == "|") {
            return true; // Neu co token la pipe, tra ve true
        }
    }
    return false; // Neu khong co token la pipe, tra ve false
}

SimpleCommand* simpleCmd = nullptr;
PipeCommand* pipeCmd = nullptr;

Command* Parser::parse(const std::string& input) {
    if (simpleCmd) {
        delete simpleCmd; // Xoa command cu neu con ton tai
        simpleCmd = nullptr;
    }

    if (pipeCmd) {
        delete pipeCmd; // Xoa command cu neu con ton tai
        pipeCmd = nullptr;
    }

    std::string trimmedInput = trim(input);
    if (trimmedInput.empty()) {
        return nullptr; // Tra ve nullptr neu input rong
    }

    std::vector<std::string> tokens = tokenize(trimmedInput);

    if (isPipeCommand(tokens)) {
        pipeCmd = new PipeCommand({}); // Tao mot PipeCommand moi, sau nay se can them logic de phan tich tokens de tao cac subcommand va thiet lap redirection
        handleSpecialOperators(tokens, true); // Xu ly cac toan tu dac biet neu co pipe
        Command* result = pipeCmd;
        pipeCmd = nullptr;
        return result;
    }

    simpleCmd = new SimpleCommand({}); // Tao mot SimpleCommand moi voi tokens da duoc tokenize
    handleSpecialOperators(tokens, false); // Xu ly cac toan tu dac biet neu khong co pipe

    Command* result = simpleCmd;
    simpleCmd = nullptr;
    return result;
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

void Parser::handleSpecialOperators(std::vector<std::string>& tokens, bool isPipe) {
    // TODO: handle redirection (<, >) and pipe (|)
    (void)tokens;

    if (tokens.empty()) {
        return;
    }

    if (isPipe) {
        // Handle pipe command
    }
    else {
        // Extract redirection and clean tokens

        if (!tokens.empty() && tokens.back() == "&") {
            simpleCmd->run_in_background = true;
            tokens.pop_back();
        }
        else if (!tokens.empty() && tokens.back().size() > 1 && tokens.back().back() == '&') {
            simpleCmd->run_in_background = true;
            tokens.back().pop_back();
        }
        
        std::vector<std::string> cleanedTokens = extractRedirection(tokens);
        
        // Set name and args for simpleCmd
        if (!cleanedTokens.empty()) {
            simpleCmd->name = cleanedTokens[0];
            simpleCmd->args.assign(cleanedTokens.begin() + 1, cleanedTokens.end());
        }
    }
}

std::string Parser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');
    if (first == std::string::npos || last == std::string::npos) {
        return ""; // Tra ve chuoi rong neu chi co space
    }
    return str.substr(first, last - first + 1);
}

std::vector<std::string> Parser::extractRedirection(std::vector<std::string>& tokens) {
    std::vector<std::string> cleanedTokens;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];
        if (token == "<") {
            if (i + 1 < tokens.size()) {
                simpleCmd->input_file = tokens[i + 1];
                ++i; 
            }
        }
        else if (token == ">") {
            if (i + 1 < tokens.size()) {
                simpleCmd->output_file = tokens[i + 1];
                ++i;
            }
        }
        else {
            cleanedTokens.push_back(token);
        }
    }
    
    return cleanedTokens;
}
