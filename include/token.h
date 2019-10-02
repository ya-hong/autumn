#include <iostream>
#include <map>
#include <string>

namespace autumn {

struct Token {
    enum Type {
        ILLEGAL = 0,
        ASSIGN,
        PLUS,
        MINUS,
        LPAREN,
        RPAREN,
        LBRACE,
        RBRACE,
        COMMA,
        SEMICOLON,
        LET,
        IDENT,
        FUNCTION,
        INT,
        TRUE,
        FALSE,
        IF,
        ELSE,
        RETURN,
        END,
    };

    static std::map<std::string, Token::Type> keywords;
    static Type lookup(const std::string& token);

    Type type;
    std::string literal;

    bool operator==(const Token& rhs) const;
    friend std::ostream& operator<<(std::ostream& out, const Token& token);
};

std::ostream& operator<<(std::ostream& out, const Token& token);

} // namespace autumn
