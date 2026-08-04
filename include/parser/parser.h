#pragma once

#include "lexer.h"
#include "parser/ast.h"
#include "parser/token.h"
#include <memory>

class Parser{
private:
    Lexer &lexer_;
    Token current_token_;
public: 
    explicit Parser(Lexer &lex) : lexer_(lex){
        current_token_ = lexer_.NextToken();
    }
    std::unique_ptr<Statement>Parse();
    Token Expect(TokenType type, const std::string& val);
    Token Expect(TokenType type);
private:
    bool Match(TokenType type, const std::string& val);
    void Advance();
    TypeId ParseColumnType();
    std::unique_ptr<SelectStatement> ParseSelect();
    std::unique_ptr<InsertStatement> ParseInsert();
    std::unique_ptr<DeleteStatement> ParseDelete();
    std::unique_ptr<Create> ParseCreate();
    std::unique_ptr<CreateTable> ParseCreateTable();
    std::unique_ptr<CreateIndex> ParseCreateIndex();
    std::unique_ptr<JoinClause> ParseJoin();
    std::unique_ptr<Predicate> ParseWhere();
    std::vector<Literal> ParseValueRow();
};