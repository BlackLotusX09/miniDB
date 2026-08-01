#include "parser/lexer.h"
#include "parser/token.h"
#include<cctype>
#include<stdexcept>
#include<unordered_set>
#include<algorithm>

using namespace std;

Lexer::Lexer(std::string input) : input_(std::move(input)){}

void Lexer::SkipWhitespace(){
    while(pos_<input_.size()){
        if(input_[pos_]=='\n'){
            line_++;
            pos_++;
        }
        else if(input_[pos_]==' ' || input_[pos_]=='\t' || input_[pos_]=='\r'){
            pos_++;
        }
        else{
            break;
        }
    }
}

bool Lexer::IsKeyword(const std::string& word){
    unordered_set<std::string> kw = {"SELECT", "FROM","WHERE","INSERT","INTO","VALUES","CREATE","TABLE","INDEX","ON","JOIN","DELETE","INT","VARCHAR","BOOL","AND","OR","NOT","IS","NULL","AS"};
    return kw.count(word)>0;
}

Token Lexer::ScanIdentifierOrKeyword(){
    int start_line_ = line_;
    string store="";
    while(pos_<input_.size()){
        char c = input_[pos_];
        if(isalnum(c) || c=='_'){
            store+=c;
            pos_++;
        }
        else{
            break;
        }
    }
    for(auto &c:store){
        c=toupper(c);
    }
    if(IsKeyword(store)){
        return Token{TokenType::KEYWORD,store,start_line_};
    }
    else{
        return Token{TokenType::IDENTIFIER,store,start_line_};
    }
}

Token Lexer::ScanIntLiteral(){
    int start_line = line_;
    std::string digit="";
    while(pos_<input_.size()){
        char c = input_[pos_];
        if(isdigit(c)){
            digit+=c;
            pos_++;
        }
        else{
            break;
        }
    }
    return Token{TokenType::INT_LITERAL,digit,start_line};
}

Token Lexer::ScanStringLiteral(){
    int start_line = line_;
    pos_++;
    std::string content="";
    bool notEnd = true;
    while(pos_<input_.size())
    {
        char c = input_[pos_];
        if(c=='\''){
            pos_++;
            notEnd = false;
            break;
        }
        else{
            content+=c;
            pos_++;
        }
    }
    if(notEnd==true){
        throw std::runtime_error("Unterminated string literal at line " + std::to_string(start_line));
    }
    return Token{TokenType::STRING_LITERAL,content,start_line};
    
}    

Token Lexer::NextToken(){
    SkipWhitespace();
    if(pos_>=input_.size()){
        return Token{TokenType::END_OF_FILE,"",line_};
    }

    char c = input_[pos_];
    if(isalpha(c) || c=='_')return ScanIdentifierOrKeyword();
    if(isdigit(c))return ScanIntLiteral();
    if(c=='\'')return ScanStringLiteral();
    if(c=='*'){
        pos_++;
        return Token{TokenType::STAR,"*",line_};
    }
    if(c=='('){
        pos_++;
        return Token{TokenType::LPAREN,"(",line_};
    }
    if(c==')'){
        pos_++;
        return Token{TokenType::RPAREN,")",line_};
    }
    if(c==','){
        pos_++;
        return Token{TokenType::COMMA,",",line_};
    }
    if(c==';'){
        pos_++;
        return Token{TokenType::SEMICOLON,";",line_};
    }
    if(c=='.'){
        pos_++;
        return Token{TokenType::DOT,".",line_};
    }
    if(c=='>'){
        pos_++;
        return Token{TokenType::OPERATOR,">",line_};
    }
    if(c=='<'){
        pos_++;
        return Token{TokenType::OPERATOR,"<",line_};
    }
    if(c=='='){
        pos_++;
        return Token{TokenType::OPERATOR,"=",line_};
    }
    if(c=='!' && pos_+1 < input_.size() && input_[pos_+1]=='='){
        pos_ += 2;
        return Token{TokenType::OPERATOR,"!=",line_};
    }
    throw std::runtime_error("Unexpected character " + std::string(1, c) + " at line " + std::to_string(line_));
}
