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
    }
    return 0;
}

static int structDef() {
    Token *startTk = crtTk;
    if (!consume(STRUCT)) return 0;
    if (!consume(ID) || !consume(LACC)) {
        crtTk = startTk;
        return 0;
    }
    if (!consume(RACC)) {
        tkerr(crtTk,"missing } in struct def");
    }
    if (!consume(SEMICOLON)) {
        tkerr(crtTk,"missing ; in struct def");
    }
    return 1;
}

static int arrayDecl() {
    if (!consume(LBRACKET)) return 0;
    consume(CT_INT);
    if (!consume(RBRACKET)) {
        tkerr(crtTk,"missing ]");
    }
    return 1;
}

static int varDef() {
    Token *startTk = crtTk;
    if (!typeBase()) return 0;
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    if (!consume(SEMICOLON)) {
        crtTk = startTk;
        return 0;
    }
    return 1;
}

static int stmCompound() {
    if (!consume(LACC)) {
        return 0;
    }
    if (!consume(RACC)) {
        tkerr(crtTk, "missing }");
    }
    return 1;
}

static int expr() {
    return 0;
}

static int fnParam() {
    Token *startTk = crtTk;
    if (!typeBase()) return 0;
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
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
}