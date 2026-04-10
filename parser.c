//
// Created by ionut on 4/10/2026.
//
#include<stdio.h>
#include <stdlib.h>
#include "parser.h"

static Token *crtTk;
static Token *consumedTk;

static int consume(int code) {
    if (crtTk->code == code) {
        consumedTk = crtTk;
        crtTk = crtTk->next;
        return 1;
    }
    return 0;
}

static int typeBase() {
    Token *startTk = crtTk;
    if (consume(INT) || consume(DOUBLE) || consume(CHAR)) {
        return 1;
    }
    if (consume(STRUCT)) {
        if (!consume(ID)) {
            crtTk = startTk;
            return 0;
        }
        return 1;
    }
    return 0;
}

static int exprAssign();    static int exprAssignAux();
static int exprOr();        static int exprOrAux();
static int exprAnd();       static int exprAndAux();
static int exprEq();        static int exprEqAux();
static int exprRel();       static int exprRelAux();
static int exprAdd();       static int exprAddAux();
static int exprMul();       static int exprMulAux();
static int exprCast();
static int exprUnary();
static int exprPostfix();   static int exprPostfixAux();
static int exprPrimary();
static int stm();           static int stmCompound();

static int expr() {
    return exprAssign();
}

static int arrayDecl() {
    if (!consume(LBRACKET)) return 0;
    expr();
    if (!consume(RBRACKET)) {
        tkerr(crtTk,"missing ]");
    }
    return 1;
}

static int exprAssign() {
    if (!exprOr()) return 0;
    return exprAssignAux();
}

static int exprAssignAux() {
    if (consume(ASSIGN)) {
        if (!exprAssign()) tkerr(crtTk,"invalid expression after =");
    }
    return 1;
}

static int exprOr() {
    if (!exprAnd()) return 0;
    return exprOrAux();
}

static int exprOrAux() {
    if (consume(OR)) {
        if (!exprAnd()) tkerr(crtTk,"invalid expression after ||");
        return exprOrAux();
    }
    return 1;
}

static int exprAnd() {
    if (!exprEq()) return 0;
    return exprAndAux();
}

static int exprAndAux() {
    if (consume(AND)) {
        if (!exprEq()) tkerr(crtTk,"invalid expression after &&");
        return exprAndAux();
    }
    return 1;
}

static int exprEq() {
    if (!exprRel()) return 0; return exprEqAux();
}

static int exprEqAux() {
    if (consume(EQUALS) || consume(NOTEQ)) {
        if (!exprRel()) tkerr(crtTk, "invalid expression after == or !=");
        return exprEqAux();
    }
    return 1;
}

static int exprRel() {
    if (!exprAdd()) return 0; return exprRelAux();
}

static int exprRelAux() {
    if (consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)) {
        if (!exprAdd()) tkerr(crtTk, "invalid expression after relation operator");
        return exprRelAux();
    }
    return 1;
}

static int exprAdd() {
    if (!exprMul()) return 0; return exprAddAux();
}

static int exprAddAux() {
    if (consume(ADD) || consume(SUB)) {
        if (!exprMul()) tkerr(crtTk, "invalid expression after + or - operator");
        return exprAddAux();
    }
    return 1;
}

static int exprMul() {
    if (!exprCast()) return 0;
    return exprMulAux();
}

static int exprMulAux() {
    if (consume(MUL) || consume(DIV)) {
        if (!exprCast()) tkerr(crtTk, "invalid expression after multiplication operator");
        return exprMulAux();
    }
    return 1;
}

static int exprCast() {
    Token *startTk = crtTk;
    if (consume(LPAR)) {
        if (typeBase()) {
            arrayDecl();
            if (!consume(RPAR)) {
                crtTk = startTk;
                return exprUnary();
            }
            if (!exprCast()) {
                tkerr(crtTk,"invalid expression after cast");
            }
            return 1;
        }
        crtTk = startTk;
    }
    return exprUnary();
}

static int exprUnary() {
    if (consume(SUB) || consume(NOT)) {
        if (!exprUnary()) tkerr(crtTk,"invalid expression after unary operator");
        return 1;
    }
    return exprPostfix();
}

static int exprPostfix() {
    if (!exprPrimary()) return 0; return exprPostfixAux();
}

static int exprPostfixAux() {
    if (consume(LBRACKET)) {
        if (!expr()) tkerr(crtTk,"invalid expression in [ ]");
        if (!consume(RBRACKET)) tkerr(crtTk,"missing ]");
        return exprPostfixAux();
    }
    if (consume(DOT)) {
        if (!consume(ID)) tkerr(crtTk,"missing identifier after .");
        return exprPostfixAux();
    }
    return 1;
}

/* exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
             | CT_INT | CT_REAL | CT_CHAR | CT_STRING
             | LPAR expr RPAR */
static int exprPrimary() {
    if (consume(ID)) {
        if (consume(LPAR)) {
            if (expr()) {
                while (consume(COMMA)) {
                    if (!expr()) tkerr(crtTk,"invalid expression after ,");
                }
            }
            if (!consume(RPAR)) tkerr(crtTk,"missing ) in function call");
        }
        return 1;
    }

    if (consume(CT_INT) || consume(CT_REAL) || consume(CT_CHAR) || consume(CT_STRING)) {
        return 1;
    }

    if (consume(LPAR)) {
        if (!expr()) tkerr(crtTk,"invalid expression after (");
        if (!consume(RPAR)) tkerr(crtTk,"missing )");
        return 1;
    }
    return 0;
}

static int varDef() {
    Token *startTk = crtTk;
    if (!typeBase()) return 0;
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    arrayDecl();
    while (consume(COMMA)) {
        if (!consume(ID)) {
            tkerr(crtTk,"missing identifier after ,");
        }
        arrayDecl();
    }
    if (!consume(SEMICOLON)) {
        crtTk = startTk;
        return 0;
    }
    return 1;
}

static int structDef() {
    Token *startTk = crtTk;
    if (!consume(STRUCT)) return 0;
    if (!consume(ID) || !consume(LACC)) {
        crtTk = startTk;
        return 0;
    }
    while (varDef()) {}
    if (!consume(RACC)) {
        tkerr(crtTk,"missing } in struct def");
    }
    if (!consume(SEMICOLON)) {
        tkerr(crtTk,"missing ; in struct def");
    }
    return 1;
}

static int stm() {
    Token *startTk = crtTk;
    if (stmCompound()) return 1;
    if (consume(IF)) {
        if (!consume(LPAR)) tkerr(crtTk,"missing ( after if");
        if (!expr()) tkerr(crtTk,"invalid expression in if");
        if (!consume(RPAR)) tkerr(crtTk,"missing ) in if");
        if (!stm()) tkerr(crtTk,"missing if statement");
        if (consume(ELSE)) {
            if (!stm()) tkerr(crtTk,"missing else");
        }
        return 1;
    }

    if (consume(WHILE)) {
        if (!consume(LPAR)) tkerr(crtTk,"missing ( after while");
        if (!expr()) tkerr(crtTk,"invalid expression in while");
        if (!consume(RPAR)) tkerr(crtTk,"missing ) in while");
        if (!stm()) tkerr(crtTk,"missing while statement");
        return 1;
    }

    if (consume(FOR)) {
        if (!consume(LPAR)) tkerr(crtTk,"missing ( after for");
        expr();
        if (!consume(SEMICOLON)) tkerr(crtTk,"missing ; in for");
        expr();
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ; in for");
        expr();
        if (!consume(RPAR)) tkerr(crtTk,"missing ) in for");
        if (!stm()) tkerr(crtTk,"missing for statement");
        return 1;
    }

    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) tkerr(crtTk,"missing ; after break");
        return 1;
    }

    if (consume(RETURN)) {
        expr();
        if (!consume(SEMICOLON)) tkerr(crtTk,"missing ; after return");
        return 1;
    }

    expr();
    if (consume(SEMICOLON)) return 1;

    crtTk = startTk;
    return 0;
}

static int stmCompound() {
    if (!consume(LACC)) {
        return 0;
    }
    while (1) {
        if (varDef()){}
        else if (stm()) {}
        else break;
    }
    if (!consume(RACC)) tkerr(crtTk,"missing }");
    return 1;
}

static int fnParam() {
    Token *startTk = crtTk;
    if (!typeBase()) return 0;
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    arrayDecl();
    return 1;
}

static int fnDef() {
    Token *startTk = crtTk;
    if (!typeBase()) {
        if (!consume(VOID)) {
            return 0;
        }
    }
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    if (!consume(LPAR)) {
        crtTk = startTk;
        return 0;
    }
    if (fnParam()) {
        while (consume(COMMA)) {
            if (!fnParam()) {
                tkerr(crtTk,"missing parameter after ,");
            }
        }
    }
    if (!consume(RPAR)) {
        tkerr(crtTk, "missing ) in function definition");
    }
    if (!stmCompound()) {
        tkerr(crtTk,"missing function body");
    }
    return 1;
}

static int unit() { // the entry point for the program, represents an entire AtomC source file
    while (1) {
        if (structDef()){}
        else if (fnDef()) {}
        else if (varDef()) {}
        else break;
    }
    if (!consume(END)) {
        tkerr(crtTk,"missing END / invalid top-level declaration");
    }
    return 1;
}

void parse(Token *tokenList) {
    crtTk = tokenList;
    unit();
    printf("Syntax OK\n");
}