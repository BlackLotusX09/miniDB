#pragma once

#include <string>
#include "parser/token.h"

// ---------------------------------------------------------------------------
// Lexer — single-pass tokenizer for SQL input
//
// Usage:
//   Lexer lex("SELECT name FROM students WHERE age > 20");
//   Token t;
//   while ((t = lex.NextToken()).type != TokenType::END_OF_FILE) { … }
// ---------------------------------------------------------------------------
class Lexer {
public:
    explicit Lexer(std::string input);

    // Advance pos_ and return the next Token.
    // Calling NextToken() past the end always returns END_OF_FILE safely.
    Token NextToken();

private:
    // Skip spaces, tabs, carriage returns; increment line_ on newlines.
    void SkipWhitespace();

    // Consume [a-zA-Z_][a-zA-Z0-9_]*, uppercase it, check keyword set.
    Token ScanIdentifierOrKeyword();

    // Consume [0-9]+, return INT_LITERAL.
    Token ScanIntLiteral();

    // Consume '…', return STRING_LITERAL with content (quotes stripped).
    // Throws std::runtime_error on unterminated string.
    Token ScanStringLiteral();

    // Returns true if the (already uppercased) word is a SQL keyword.
    static bool IsKeyword(const std::string& word);

    std::string input_;
    size_t      pos_  = 0;
    int         line_ = 1;
};
