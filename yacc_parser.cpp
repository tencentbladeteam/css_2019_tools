// yacc_parser.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <streambuf>
#include <ostream>
#include <iterator>
#include <algorithm>
#include <sstream> 

int help() {
    std::cout <<
R"(
  Usage: yacc_parser.exe PATH_TO_Y_FILE 

  Also, please put binary `yacc` in the same folder with me.

  Please, compile yacc from the source file.
)";
    return -1;
}

int error() {
    std::cout <<
        R"(
  ERROR

  Please check ther version of your ./yacc. A tested version is: yacc - 1.9
)";
    return -2;
}

int main(int argc, const char* argv[])
{
    if (argc == 1)
        return help();

    const char* fn = argv[1];
    std::string cmd("./yacc -r -o output ");
    cmd += fn;
    system(cmd.c_str()); //WARNING: THERE MIGHT BE A COMMAND INJECTION RISK HERE, USE IT LOCALLY AND BY YOURSELF ONLY.

    std::ifstream t(R"(./output)");
    std::string str((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
    t.close();

    std::string tofind1 = "const char *const yyname[] = {";
    std::string tofind2 = "};\nconst char *const yyrule[] = {";
    int location1 = str.find(tofind1);
    int location2 = str.find(tofind2);
    if (location1 == std::string::npos || location2 == std::string::npos || location2 <= location1 + tofind1.length()) {
        return error();
    }

    std::vector<std::string> keywords;
    std::string data = str.substr(location1 + tofind1.length(), location2 - location1 - tofind1.length());
    bool is_token = false;
    bool is_string = false;
    bool is_escapechar = false; 
    std::string token;
    
    int i = 0;
    while (isspace(data.at(i))) //jumps off the initial spaces
        i++;
    
    for (; i < data.size(); i++) {
        if (data.at(i) == '\"' && !is_escapechar && !is_token && !is_string) { //1. not a token; 2. not \"; 3.not in str, ex: ["]66", 0,
            is_token = true;
            is_escapechar = false;

            continue;
        }
        if (data.at(i) == '\"' && !is_escapechar && is_token && !is_string) { //1. in token; 2. not \"; 3.not in str, ex: "66["], 0,
            is_token = false; //terminates the token scanning
            is_escapechar = false;
            
            keywords.push_back(token);
            token = ""; //push the data into keywords and continue.

            continue;
        }
        else if (data.at(i) == '\"' && !is_escapechar && is_token && is_string) { //1. not \"; 2. inside a token; 3. inside a string. Example: "'["]'"
            is_escapechar = false;

            continue; 
        }
        else if (data.at(i) == '\"' && !is_escapechar && is_token) { //1. not \"; 2. inside a token; 3. outside a string. Example: "'c'["]
            is_token = false;
            is_escapechar = false;
            //TODO: add token to keywords
            continue;
        }
        else if (data.at(i) == '\\' && is_escapechar) {
            is_escapechar = false;
            token += '\\';
            continue;
        }
        else if (data.at(i) == '\\' && !is_escapechar) {
            is_escapechar = true;
            token += '\\';
            continue;
        }
        else if (i + 1 < data.size() && data.at(i) == '0' && data.at(i + 1) == ',') { // [0,] 0, 0, in yyname is just an empty placeholder, ignore them.
            i++;
            is_escapechar = false;

            continue;
        }
        else if (data.at(i) == '\'' && is_token && !is_escapechar && !is_string) { //1. inside a token; 2. not \'; 3. the first ', ex: "[']2'"
            is_string = true;
            is_escapechar = false;
            continue;
        }
        else if (data.at(i) == '\'' && is_token && !is_escapechar && is_string) { //1. inside a token; 2. not \'; 3. the last ', ex: "'2[']"
            is_string = false;
            is_escapechar = false;
            continue;
        }
        if (!is_token && !is_escapechar && !is_string) {
            continue;
        }
        is_escapechar = false;
        token += data.at(i);
    }

    std::cout << "Found these words: \n";
    std::ostringstream stream;
    std::string dict = "\"";
    std::copy(keywords.begin(), keywords.end(), std::ostream_iterator<std::string>(stream, "\"\n\""));
    dict += stream.str();
    dict.pop_back(); //remove the last junky \"

    std::cout << dict; 

    std::ofstream out("dict");
    out << dict;
    out.close();

    std::cout << "\n----\n Strings listed above has been written to `dict`. \n";
    return 0;
}
