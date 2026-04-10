#ifndef PARSER_H
#define PARSER_H

#include "command.h"
#include <string>
#include <vector>
#include <stack>

class Parser {
public:
    // Phan tich cau lenh va tra ve mot Command tuong ung
    static Command* parse(const std::string& input);
private:

    //Tokenize input
    static std::vector<std::string> tokenize(const std::string& input);

    //Xu ly cac toan tu dac biet nhu redirection va pipe
    // (chua hoan thien)
    static void handleSpecialOperators(std::vector<std::string>& tokens);
    
    //Bo di dau space thua o dau va cuoi chuoi
    static std::string trim(const std::string& str);
};

#endif
