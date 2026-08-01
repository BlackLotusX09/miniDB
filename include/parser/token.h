#pragma once

#include <string>

// ---------------------------------------------------------------------------
// TokenType — every category of token the lexer can produce
// ---------------------------------------------------------------------------
enum class TokenType {
    KEYWORD,        // SELECT, FROM, WHERE, INSERT, …
    IDENTIFIER,     // user-defined names: table names, column names
    INT_LITERAL,    // integer constants: 42, 100
    STRING_LITERAL, // quoted strings: 'hello'
    OPERATOR,       // >, >=, <, <=, =, !=
    LPAREN,         // (
    RPAREN,         // )
    COMMA,          // ,
    SEMICOLON,      // ;
    STAR,           // *
    DOT,            // .
    END_OF_FILE     // sentinel — returned when input is exhausted
};

// ---------------------------------------------------------------------------
// Token — the unit produced by the lexer and consumed by the parser
// ---------------------------------------------------------------------------
struct Token {
    TokenType   type;
    std::string value;  // raw text (identifiers/keywords are uppercased)
    int         line;   // 1-based line number (useful for error messages)
};