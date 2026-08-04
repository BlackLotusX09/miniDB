#include "catalog/schema.h"
#include "parser/ast.h"
#include "parser/token.h"
#include <memory>
#include <parser/parser.h>
#include <stdexcept>

void Parser::Advance() { current_token_ = lexer_.NextToken(); }


std::unique_ptr<Statement> Parser::Parse() {
  if (current_token_.type == TokenType::KEYWORD &&
      current_token_.value == "SELECT")
    return ParseSelect();
  if (current_token_.type == TokenType::KEYWORD &&
      current_token_.value == "INSERT")
    return ParseInsert();
  if (current_token_.type == TokenType::KEYWORD &&
      current_token_.value == "DELETE")
    return ParseDelete();
  if (current_token_.type == TokenType::KEYWORD &&
      current_token_.value == "CREATE")
    return ParseCreate();
  // if(current_token_.type==TokenType::KEYWORD &&
  // current_token_.value=="INDEX")return ParseCreateIndex();
  throw std::runtime_error("Parse error line " +
                           std::to_string(current_token_.line) +
                           ": unexpected token '" + current_token_.value + "'");
}
Token Parser::Expect(TokenType type, const std::string &val) {
  if (current_token_.type != type ||
      (!val.empty() && val != current_token_.value)) {
    throw std::runtime_error(
        "Parse error line " + std::to_string(current_token_.line) +
        ": expected '" + val + "' got '" + current_token_.value + "')");
  }
  Token t = current_token_;
  Advance();
  return t;
}
Token Parser::Expect(TokenType type) {
  if (current_token_.type != type) {
    throw std::runtime_error("Parse error line " +
                             std::to_string(current_token_.line));
  }
  Token t = current_token_;
  Advance();
  return t;
}
bool Parser::Match(TokenType type, const std::string &val) {
  if (current_token_.type == type &&
      (val.empty() || current_token_.value == val)) {
    Advance();
    return true;
  }
  return false;
}
std::vector<Literal> Parser::ParseValueRow() {
  Expect(TokenType::LPAREN);
  std::vector<Literal> row;
  do {
    Literal v;
    if (current_token_.type == TokenType::INT_LITERAL) {
      v.type = TypeId::INT;
      v.int_value = std::stoi(current_token_.value);
      Advance();
    } else if (current_token_.type == TokenType::STRING_LITERAL) {
      v.type = TypeId::VARCHAR;
      v.str_val = current_token_.value;
      Advance();
    } else if (current_token_.type == TokenType::BOOL_LITERAL) {
      v.type = TypeId::BOOL;
      v.bool_val = (current_token_.value == "TRUE");
      Advance();
    } else {
      throw std::runtime_error(
          "Parse error line " + std::to_string(current_token_.line) +
          ": expected a literal value, got '" + current_token_.value + "'");
    }
    row.push_back(std::move(v));
  } while (Match(TokenType::COMMA, ","));
  Expect(TokenType::RPAREN);
  return row;
}
std::unique_ptr<InsertStatement> Parser::ParseInsert() {
  Expect(TokenType::KEYWORD, "INSERT");
  Expect(TokenType::KEYWORD, "INTO");
  std::string table_name = Expect(TokenType::IDENTIFIER).value;
  // Optional: Parse target columns if present, e.g., INSERT INTO users (id,
  // name) ...
  std::vector<std::string> columns;
  if (Match(TokenType::LPAREN, "(")) {
    do {
      columns.push_back(Expect(TokenType::IDENTIFIER).value);
    } while (Match(TokenType::COMMA, ","));
    Expect(TokenType::RPAREN, ")");
  }
  // Must match the 'VALUES' keyword
  Expect(TokenType::KEYWORD, "VALUES");

  // Parse one or more value rows: (val1, val2), (val3, val4)
  std::vector<std::vector<Literal>> vals;
  do {
    vals.push_back(
        ParseValueRow()); // ParseValueRow handles its own '(' and ')'
  } while (Match(TokenType::COMMA, ","));

  Expect(TokenType::SEMICOLON, ";");

  // Pass table_name, (optional columns), and parsed rows to AST node
  return std::make_unique<InsertStatement>(std::move(table_name),
                                           std::move(vals));
}
TypeId Parser::ParseColumnType() {
  if (current_token_.type == TokenType::IDENTIFIER &&
      current_token_.value == "INT") {
    Advance();
    return TypeId::INT;
  }
  if (current_token_.type == TokenType::IDENTIFIER &&
      current_token_.value == "VARCHAR") {
    Advance();
    return TypeId::VARCHAR;
  }
  if (current_token_.type == TokenType::IDENTIFIER &&
      current_token_.value == "BOOL") {
    Advance();
    return TypeId::BOOL;
  }
  throw std::runtime_error(
      "Parse error line " + std::to_string(current_token_.line) +
      ": expected a column type, got '" + current_token_.value + "'");
}

std::unique_ptr<CreateTable> Parser::ParseCreateTable() {
    Expect(TokenType::KEYWORD, "CREATE");
    Expect(TokenType::KEYWORD, "TABLE");
    std::string name = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::LPAREN);
    std::vector<Column> columns;
    do {
        std::string col_name = Expect(TokenType::IDENTIFIER).value;
        TypeId col_type = ParseColumnType();
        columns.push_back(Column{col_name, col_type});
    } while (Match(TokenType::COMMA, ","));
    Expect(TokenType::RPAREN);
    Expect(TokenType::SEMICOLON);
    return std::make_unique<CreateTable>(std::move(name), std::move(columns));
}

// ---------------------------------------------------------------------------
// StringToOp — converts an operator token string to the Op enum.
// This is the fix for: "no viable conversion from 'std::string' to 'Op'"
// You cannot assign token.value (a std::string) directly to an Op field —
// an explicit mapping is required.
// ---------------------------------------------------------------------------
static Op StringToOp(const std::string& s) {
    if (s == "=")  return Op::EQ;
    if (s == "!=") return Op::NEQ;
    if (s == "<")  return Op::LT;
    if (s == ">")  return Op::GT;
    if (s == "<=") return Op::LTE;
    if (s == ">=") return Op::GTE;
    throw std::runtime_error("Unknown operator: " + s);
}

// ---------------------------------------------------------------------------
// ParseWhere — parses "WHERE col OP literal"
// ---------------------------------------------------------------------------
std::unique_ptr<Predicate> Parser::ParseWhere() {
    Expect(TokenType::KEYWORD, "WHERE");
    std::string col = Expect(TokenType::IDENTIFIER).value;
    // Convert the std::string operator token to the Op enum explicitly:
    Op op = StringToOp(Expect(TokenType::OPERATOR).value);
    Literal rhs;
    if (current_token_.type == TokenType::INT_LITERAL) {
        rhs.type = TypeId::INT;
        rhs.int_value = std::stoi(current_token_.value);
        Advance();
    } else if (current_token_.type == TokenType::STRING_LITERAL) {
        rhs.type = TypeId::VARCHAR;
        rhs.str_val = current_token_.value;
        Advance();
    } else if (current_token_.type == TokenType::BOOL_LITERAL) {
        rhs.type = TypeId::BOOL;
        rhs.bool_val = (current_token_.value == "TRUE");
        Advance();
    } else {
        throw std::runtime_error("Parse error line " + std::to_string(current_token_.line)
            + ": expected literal in WHERE clause, got '" + current_token_.value + "'");
    }
    return std::make_unique<Predicate>(std::move(col), op, rhs);
}

// ---------------------------------------------------------------------------
// ParseJoin — parses "JOIN table [AS alias] ON left_col = right_col"
// ---------------------------------------------------------------------------
std::unique_ptr<JoinClause> Parser::ParseJoin() {
    Expect(TokenType::KEYWORD, "JOIN");
    JoinClause jc;
    jc.right_table = Expect(TokenType::IDENTIFIER).value;
    if (Match(TokenType::KEYWORD, "AS")) {
        jc.right_alias = Expect(TokenType::IDENTIFIER).value;
    }
    Expect(TokenType::KEYWORD, "ON");
    jc.left_col  = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::OPERATOR, "=");
    jc.right_col = Expect(TokenType::IDENTIFIER).value;
    return std::make_unique<JoinClause>(std::move(jc));
}

// ---------------------------------------------------------------------------
// ParseSelect — parses "SELECT col,... FROM table [AS alias] [JOIN ...] [WHERE ...]"
// ---------------------------------------------------------------------------
std::unique_ptr<SelectStatement> Parser::ParseSelect() {
    Expect(TokenType::KEYWORD, "SELECT");
    auto stmt = std::make_unique<SelectStatement>();

    if (current_token_.type == TokenType::STAR) {
        stmt->column.push_back("*");
        Advance();
    } else {
        do {
            stmt->column.push_back(Expect(TokenType::IDENTIFIER).value);
        } while (Match(TokenType::COMMA, ","));
    }

    Expect(TokenType::KEYWORD, "FROM");
    stmt->tables = Expect(TokenType::IDENTIFIER).value;

    if (Match(TokenType::KEYWORD, "AS")) {
        stmt->alias = Expect(TokenType::IDENTIFIER).value;
    }

    while (current_token_.type == TokenType::KEYWORD &&
           current_token_.value == "JOIN") {
        auto jc = ParseJoin();
        stmt->joins.push_back(std::move(*jc));
    }

    if (current_token_.type == TokenType::KEYWORD &&
        current_token_.value == "WHERE") {
        stmt->where = ParseWhere();
    }

    Expect(TokenType::SEMICOLON);
    return stmt;
}

// ---------------------------------------------------------------------------
// ParseDelete — parses "DELETE FROM table [WHERE ...]"
// ---------------------------------------------------------------------------
std::unique_ptr<DeleteStatement> Parser::ParseDelete() {
    Expect(TokenType::KEYWORD, "DELETE");
    Expect(TokenType::KEYWORD, "FROM");
    std::string table = Expect(TokenType::IDENTIFIER).value;

    std::unique_ptr<Predicate> where_clause;
    if (current_token_.type == TokenType::KEYWORD &&
        current_token_.value == "WHERE") {
        where_clause = ParseWhere();
    }

    Expect(TokenType::SEMICOLON);
    return std::make_unique<DeleteStatement>(std::move(table), std::move(where_clause));
}

// ---------------------------------------------------------------------------
// ParseCreate — dispatches to ParseCreateTable or ParseCreateIndex
// ---------------------------------------------------------------------------
std::unique_ptr<Create> Parser::ParseCreate() {
    Expect(TokenType::KEYWORD, "CREATE");
    if (current_token_.type == TokenType::KEYWORD &&
        current_token_.value == "TABLE") {
        Expect(TokenType::KEYWORD, "TABLE");
        std::string name = Expect(TokenType::IDENTIFIER).value;
        Expect(TokenType::LPAREN);
        std::vector<Column> columns;
        do {
            std::string col_name = Expect(TokenType::IDENTIFIER).value;
            TypeId col_type = ParseColumnType();
            columns.push_back(Column{col_name, col_type});
        } while (Match(TokenType::COMMA, ","));
        Expect(TokenType::RPAREN);
        Expect(TokenType::SEMICOLON);
        return std::make_unique<CreateTable>(std::move(name), std::move(columns));
    }
    if (current_token_.type == TokenType::KEYWORD &&
        current_token_.value == "INDEX") {
        return ParseCreateIndex();
    }
    throw std::runtime_error("Parse error line " + std::to_string(current_token_.line)
        + ": expected TABLE or INDEX after CREATE, got '" + current_token_.value + "'");
}

// ---------------------------------------------------------------------------
// ParseCreateIndex — parses "CREATE INDEX ON table (column)"
// ---------------------------------------------------------------------------
std::unique_ptr<CreateIndex> Parser::ParseCreateIndex() {
    // CREATE keyword already consumed by ParseCreate
    Expect(TokenType::KEYWORD, "INDEX");
    auto idx = std::make_unique<CreateIndex>();
    Expect(TokenType::KEYWORD, "ON");
    idx->table  = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::LPAREN);
    idx->column = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::RPAREN);
    Expect(TokenType::SEMICOLON);
    return idx;
}