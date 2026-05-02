//
// Created by ionut on 4/10/2026.
//
#include<stdio.h>
#include <stdlib.h>
#include "parser.h"
#include "symbols.h"

static Token *crtTk;
static Token *consumedTk;

static int isRedefinedAtCrtDepth(const char *name) {
    Symbol *s = findSymbol(&symbols,name);
    return s!=NULL && s->depth == crtDepth;
}

static int consume(int code) {
    if (crtTk->code == code) {
        consumedTk = crtTk;
        crtTk = crtTk->next;
        return 1;
    }
    return 0;
}

static int typeBase(Type *ret) {
    Token *startTk = crtTk;
    ret->nElements = -1;
    ret->s = NULL;

    if (consume(INT)) {
        ret->typeBase = TB_INT;
        return 1;
    }
    if (consume(DOUBLE)) {
        ret->typeBase = TB_DOUBLE;
        return 1;
    }
    if (consume(CHAR)) {
        ret->typeBase = TB_CHAR;
        return 1;
    }
    if (consume(STRUCT)) {
        if (!consume(ID)) {
            crtTk = startTk;
            return 0;
        }
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(&symbols,tkName->text);
        if (s==NULL) tkerr(crtTk, "undefined symbol: %s", tkName->text);
        if (s->cls != CLS_STRUCT) tkerr(crtTk, "%s is not a struct", tkName->text);
        ret->typeBase = TB_STRUCT;
        ret->s = s;
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
static int stm();           static int stmCompound(int newDomain);

static int expr() {
    return exprAssign();
}

static int arrayDecl(Type *ret) {
    if (!consume(LBRACKET)) return 0;
    Token *startSize = crtTk;
    if (consume(CT_INT) && crtTk->code == RBRACKET) {
        Token *tkSize = consumedTk;
        ret->nElements = (int)tkSize->i;
    } else {
        crtTk = startSize;
        if (expr()) {
            ret->nElements = 1;
        }
        else {
            ret->nElements = 0;
        }
    }
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
        Type t;
        if (typeBase(&t)) {
            arrayDecl(&t);
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

// Helper: register one variable (name + already-resolved type).
// Handles all three cases: global, function-local, struct member.
static void addVarLikeSymbol(const char* name, Type t) {
    if (crtStruct) {
        // struct member — must be unique among the struct's members
        if (findSymbol(&crtStruct->members, name) != NULL)
            tkerr(crtTk, "symbol redefinition: %s", name);
        Symbol *s = addSymbol(&crtStruct->members, name, CLS_VAR);
        s->type = t;
        s->mem = MEM_GLOBAL;
        return;
    }

    if (isRedefinedAtCrtDepth(name))
        tkerr(crtTk, "symbol redefinition: %s", name);

    Symbol *s = addSymbol(&symbols, name, CLS_VAR);
    s->type = t;
    if (crtFunc) {
        s->mem = MEM_LOCAL;
    }
    else {
        s->mem = MEM_GLOBAL;
    }
}

static int varDef() {
    Token *startTk = crtTk;
    Type baseType;
    if (!typeBase(&baseType)) return 0;
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    Token *tkName = consumedTk;

    //optional array decl on this variable
    Type t = baseType;
    if (arrayDecl(&t)) {
        if (t.nElements ==0)
            tkerr(crtTk,"invalid array declaration: vector variable should have a specified dimension");
    }
    addVarLikeSymbol(tkName->text, t);
    while (consume(COMMA)) {
        if (!consume(ID)) {
            tkerr(crtTk,"missing identifier after ,");
        }
        tkName = consumedTk;

        t=baseType;
        t.nElements = -1;
        if (arrayDecl(&t)) {
            if (t.nElements ==0) {
                tkerr(crtTk,"invalid array declaration: vector variable should have a specified dimension");
            }
        }
        addVarLikeSymbol(tkName->text, t);
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
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    Token *tkName = consumedTk;
    if (!consume(LACC)) {
        crtTk = startTk;
        return 0;
    }

    if (isRedefinedAtCrtDepth(tkName->text))
        tkerr(crtTk, "symbol redefinition: %s", tkName->text);
    Symbol *s = addSymbol(&symbols, tkName->text, CLS_STRUCT);
    s->type.typeBase = TB_STRUCT;
    s->type.s = s;
    s->type.nElements = -1;
    s->mem = MEM_GLOBAL;
    initSymbols(&s->members);
    crtStruct = s;
    crtDepth++;

    while (varDef()) {}
    if (!consume(RACC)) {
        tkerr(crtTk,"missing } in struct def");
    }
    if (!consume(SEMICOLON)) {
        tkerr(crtTk,"missing ; in struct def");
    }

    crtStruct = NULL;
    crtDepth--;
    return 1;
}

static int stm() {
    Token *startTk = crtTk;
    if (stmCompound(1)) return 1;
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

    if (expr()) {
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ; after expression");
        return 1;
    }
    if (consume(SEMICOLON)) return 1;

    crtTk = startTk;
    return 0;
}

static int stmCompound(int newDomain) {
    if (!consume(LACC)) {
        return 0;
    }

    Symbol *scopeStart = NULL;
    if (newDomain) {
        scopeStart = (symbols.end > symbols.begin)? *(symbols.end - 1): NULL;
        crtDepth++;
    }
    while (1) {
        if (varDef()){}
        else if (stm()) {}
        else break;
    }
    if (!consume(RACC)) tkerr(crtTk,"missing }");

    if (newDomain) {
        deleteSymbolsAfter(&symbols, scopeStart);
        crtDepth--;
    }
    return 1;
}

static int fnParam() {
    Token *startTk = crtTk;
    Type t;
    if (!typeBase(&t)) return 0;
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    Token *tkName = consumedTk;
    if (arrayDecl(&t)) {
        t.nElements = 0;
    }

    if (isRedefinedAtCrtDepth(tkName->text))
        tkerr(crtTk,"symbol redefinition: %s", tkName->text);

    Symbol *p = addSymbol(&symbols, tkName->text, CLS_VAR);
    p->type = t;
    p->mem = MEM_ARG;

    Symbol *pArg = addSymbol(&crtFunc->args, tkName->text, CLS_VAR);
    pArg->type = t;
    pArg->mem = MEM_ARG;
    return 1;
}

static int fnDef() {
    Token *startTk = crtTk;
    Type t;
    if (!typeBase(&t)) {
        if (!consume(VOID)) {
            return 0;
        }
        else {
            t.typeBase = TB_VOID;
            t.s = NULL;
            t.nElements = -1;
        }
    }
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    Token *tkName = consumedTk;
    if (!consume(LPAR)) {
        crtTk = startTk;
        return 0;
    }

    if (isRedefinedAtCrtDepth(tkName->text))
        tkerr(crtTk,"symbol redefinition: %s", tkName->text);

    Symbol *fn = addSymbol(&symbols, tkName->text, CLS_FUNC);
    fn->type = t;
    fn->mem = MEM_GLOBAL;
    initSymbols(&fn->args);
    crtFunc = fn;
    crtDepth++;

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
    if (!stmCompound(0)) {
        tkerr(crtTk,"missing function body");
    }

    deleteSymbolsAfter(&symbols, fn);
    crtDepth--;
    crtFunc = NULL;
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
    crtDepth = 0;
    crtStruct = NULL;
    crtFunc = NULL;
    initSymbols(&symbols);

    unit();
    printf("Syntax OK\n");
    showSymbolTable();
}