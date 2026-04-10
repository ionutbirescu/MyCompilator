//
// Created by ionut on 4/10/2026.
//

#ifndef LEXER_H
#define LEXER_H

enum {
    //CONSTANTS AND IDENTIFIERS
    ID, CT_INT, CT_REAL, CT_CHAR, CT_STRING,

    //SEPARATORS & OPERATORS
    AND, SEMICOLON, OR, SPACE, DOT, LPAR, RPAR, DIV, COMMA, LINECOMMENT, END,
    ASSIGN, EQUALS, LBRACKET, RBRACKET, NOT, NOTEQ, LACC, RACC, LESS, LESSEQ,
    GREATER, GREATEREQ, ADD, SUB, MUL,

    BREAK, CHAR, DOUBLE, ELSE, FOR, IF, INT, RETURN, STRUCT, VOID, WHILE,
};

typedef struct Token {
    int code;

    union {
        char *text; // for ID, CT_STRING
        long int i; // for CT_INT, CT_CHAR
        double r; // for CT_REAL
    };

    int line;
    struct Token *next;
} Token;

// Global token list (defined in lexer.c)
extern Token *tokens;

int getNextToken(void);
void showTokens(void);
void done(void);
char *loadFile(const char *fileName);

void tkerr(const Token *tk, const char *fmt, ...);
void err(const char *fmt, ...);

extern const char *pCrtCh;

#endif //LEXER_H
