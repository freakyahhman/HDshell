#include "parser.h"
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

bool isPipeCommand(const std::vector<std::string>& tokens) {
    for (const auto& token : tokens) {
        if (token == "|") {
            return true; // Neu co token la pipe, tra ve true
        }
    }
    return false; // Neu khong co token la pipe, tra ve false
}

bool isSpecialOperator(char c) {
    return c == '|' || c == '<' || c == '>' || c == '&';
}

bool isEnvNameStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isEnvNameChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string expandEnvironmentVariables(const std::string& token) {
    std::string expanded;

    for (size_t i = 0; i < token.size(); ++i) {
        if (token[i] != '$') {
            expanded += token[i];
            continue;
        }

        if (i + 1 >= token.size()) {
            expanded += '$';
            continue;
        }

        if (token[i + 1] == '$') {
            expanded += std::to_string(getpid());
            ++i;
            continue;
        }

        std::string name;
        size_t nameEnd = i + 1;

        if (token[i + 1] == '{') {
            size_t closeBrace = token.find('}', i + 2);
            if (closeBrace == std::string::npos) {
                expanded += '$';
                continue;
            }

            name = token.substr(i + 2, closeBrace - (i + 2));
            nameEnd = closeBrace + 1;
        }
        else if (isEnvNameStart(token[i + 1])) {
            nameEnd = i + 2;
            while (nameEnd < token.size() && isEnvNameChar(token[nameEnd])) {
                ++nameEnd;
            }
            name = token.substr(i + 1, nameEnd - (i + 1));
        }
        else {
            expanded += '$';
            continue;
        }

        const char* value = getenv(name.c_str());
        if (value != nullptr) {
            expanded += value;
        }

        i = nameEnd - 1;
    }

    return expanded;
}

std::vector<std::string> expandEnvironmentTokens(const std::vector<std::string>& tokens) {
    std::vector<std::string> expandedTokens;
    expandedTokens.reserve(tokens.size());

    for (const std::string& token : tokens) {
        expandedTokens.push_back(expandEnvironmentVariables(token));
    }

    return expandedTokens;
}

void extractBackgroundFlag(std::vector<std::string>& tokens, bool& runInBackground) {
    if (!tokens.empty() && tokens.back() == "&") {
        runInBackground = true;
        tokens.pop_back();
    }
    else if (!tokens.empty() && tokens.back().size() > 1 && tokens.back().back() == '&') {
        runInBackground = true;
        tokens.back().pop_back();
    }
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

    std::vector<std::string> tokens = expandEnvironmentTokens(tokenize(trimmedInput));

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
        else if (quoteStack.empty() && isSpecialOperator(c)) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back(std::string(1, c));
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
    if (tokens.empty()) {
        return;
    }

    if (isPipe) {
        bool runInBackground = false;
        extractBackgroundFlag(tokens, runInBackground);
        pipeCmd->run_in_background = runInBackground;

        std::vector<std::string> segmentTokens;

        auto appendSubcommand = [&](std::vector<std::string>& segment) -> bool {
            if (segment.empty()) {
                std::cerr << "Syntax error: empty command in pipeline" << std::endl;
                return false;
            }

            auto subcommand = std::make_unique<SimpleCommand>(std::vector<std::string>{});
            SimpleCommand* previousSimpleCmd = simpleCmd;
            simpleCmd = subcommand.get();

            std::vector<std::string> cleanedTokens = extractRedirection(segment);
            simpleCmd = previousSimpleCmd;

            if (cleanedTokens.empty()) {
                std::cerr << "Syntax error: empty command in pipeline" << std::endl;
                return false;
            }

            subcommand->name = cleanedTokens[0];
            subcommand->args.assign(cleanedTokens.begin() + 1, cleanedTokens.end());
            pipeCmd->subcommands.push_back(std::move(subcommand));
            return true;
        };

        for (const std::string& token : tokens) {
            if (token == "|") {
                if (!appendSubcommand(segmentTokens)) {
                    pipeCmd->subcommands.clear();
                    return;
                }
                segmentTokens.clear();
            }
            else {
                segmentTokens.push_back(token);
            }
        }

        if (!appendSubcommand(segmentTokens)) {
            pipeCmd->subcommands.clear();
        }
    }
    else {
        // Extract redirection and clean tokens

        extractBackgroundFlag(tokens, simpleCmd->run_in_background);
        
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
