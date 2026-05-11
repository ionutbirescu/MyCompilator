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

static int exprAssign(Ret *r);    static int exprAssignAux(Ret *r);
static int exprOr(Ret *r);        static int exprOrAux(Ret *r);
static int exprAnd(Ret *r);       static int exprAndAux(Ret *r);
static int exprEq(Ret *r);        static int exprEqAux(Ret *r);
static int exprRel(Ret *r);       static int exprRelAux(Ret *r);
static int exprAdd(Ret *r);       static int exprAddAux(Ret *r);
static int exprMul(Ret *r);       static int exprMulAux(Ret *r);
static int exprCast(Ret *r);
static int exprUnary(Ret *r);
static int exprPostfix(Ret *r);   static int exprPostfixAux(Ret *r);
static int exprPrimary(Ret *r);
static int stm();           static int stmCompound(int newDomain);

static int expr(Ret *r) {
    return exprAssign(r);
}

static int arrayDecl(Type *ret) {
    if (!consume(LBRACKET)) return 0;
    Token *startSize = crtTk;
    if (consume(CT_INT) && crtTk->code == RBRACKET) {
        Token *tkSize = consumedTk;
        ret->nElements = (int)tkSize->i;
    } else {
        crtTk = startSize;
        Ret rSize;
        if (expr(&rSize)) {
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

static int exprAssign(Ret *r) {
    if (!exprOr(r)) return 0;
    return exprAssignAux(r);
}

static int exprAssignAux(Ret *r) {
    if (consume(ASSIGN)) {
        Ret rSrc;
        if (!exprAssign(&rSrc)) tkerr(crtTk,"invalid expression after =");
        if (!r->lval)           tkerr(crtTk,"the assign destination must be a left-value");
        if (r->ct)              tkerr(crtTk, "the assign destination cannot be constant");
        if (!canBeScalar(r))    tkerr(crtTk, "the assign destination must be scalar");
        if (!canBeScalar(&rSrc)) tkerr(crtTk, "the assign source must be scalar");
        if (!convTo(&rSrc.type, &r->type))
            tkerr(crtTk, "the assign source cannot be converted to destination");
        r->lval = 0;
        r->ct = 1;
    }
    return 1;
}

static int exprOr(Ret *r) {
    if (!exprAnd(r)) return 0;
    return exprOrAux(r);
}

static int exprOrAux(Ret *r){
    if (consume(OR)) {
        Ret right;
        if (!exprAnd(&right)) tkerr(crtTk,"invalid expression after ||");
        Type tDst;
        if (!arithTypeTo(&r->type, &right.type, &tDst))
            tkerr(crtTk,"invalid operand types for ||");
        *r = (Ret){{TB_INT, NULL, -1}, 0, 1};
        return exprOrAux(r);
    }
    return 1;
}

static int exprAnd(Ret *r) {
    if (!exprEq(r)) return 0;
    return exprAndAux(r);
}

static int exprAndAux(Ret *r) {
    if (consume(AND)) {
        Ret right;
        if (!exprEq(&right)) tkerr(crtTk,"invalid expression after &&");
        Type tDst;
        if (!arithTypeTo(&r->type, &right.type, &tDst))
            tkerr(crtTk,"invalid operand types for &&");
        *r = (Ret){{TB_INT, NULL, -1}, 0, 1};
        return exprAndAux(r);
    }
    return 1;
}

static int exprEq(Ret *r) {
    if (!exprRel(r)) return 0; return exprEqAux(r);
}

static int exprEqAux(Ret *r) {
    if (consume(EQUALS) || consume(NOTEQ)) {
        Ret right;
        if (!exprRel(&right)) tkerr(crtTk,"invalid expression after == or !=");
        Type tDst;
        if (!arithTypeTo(&r->type, &right.type, &tDst))
            tkerr(crtTk,"invalid operand types for == or !=");
        *r = (Ret){{TB_INT, NULL, -1}, 0, 1};
        return exprEqAux(r);
    }
    return 1;
}

static int exprRel(Ret* r) {
    if (!exprAdd(r)) return 0; return exprRelAux(r);
}

static int exprRelAux(Ret *r) {
    if (consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)) {
        Ret right;
        if (!exprAdd(&right)) tkerr(crtTk,"invalid expression after relational operator");
        Type tDst;
        if (!arithTypeTo(&r->type, &right.type, &tDst))
            tkerr(crtTk,"invalid operand types for <, <=, >, >=");
        *r = (Ret){{TB_INT, NULL, -1}, 0, 1};
        return exprRelAux(r);
    }
    return 1;
}

static int exprAdd(Ret *r) {
    if (!exprMul(r)) return 0; return exprAddAux(r);
}

static int exprAddAux(Ret *r) {
    if (consume(ADD) || consume(SUB)) {
        Ret right;
        if (!exprMul(&right)) tkerr(crtTk,"invalid expression after + or -");
        Type tDst;
        if (!arithTypeTo(&r->type, &right.type, &tDst))
            tkerr(crtTk,"invalid operand types for + or -");
        *r = (Ret){tDst, 0, 1};
        return exprAddAux(r);
    }
    return 1;
}

static int exprMul(Ret *r) {
    if (!exprCast(r)) return 0;
    return exprMulAux(r);
}

static int exprMulAux(Ret *r) {
    if (consume(MUL) || consume(DIV)) {
        Ret right;
        if (!exprCast(&right)) tkerr(crtTk,"invalid expression after * or /");
        Type tDst;
        if (!arithTypeTo(&r->type, &right.type, &tDst))
            tkerr(crtTk,"invalid operand types for * or /");
        *r = (Ret){tDst, 0, 1};
        return exprMulAux(r);
    }
    return 1;
}

static int exprCast(Ret *r) {
    Token *startTk = crtTk;
    if (consume(LPAR)) {
        Type t;
        if (typeBase(&t)) {
            arrayDecl(&t);
            if (!consume(RPAR)) {
                crtTk = startTk;
                return exprUnary(r);
            }
            Ret op;
            if (!exprCast(&op)) tkerr(crtTk,"invalid expression after cast");
            if (t.typeBase == TB_STRUCT)       tkerr(crtTk,"cannot convert to a struct type");
            if (op.type.typeBase == TB_STRUCT)  tkerr(crtTk,"cannot convert a struct");
            if (op.type.nElements >= 0 && t.nElements < 0)
                tkerr(crtTk,"an array can be converted only to another array");
            if (op.type.nElements < 0 && t.nElements >= 0)
                tkerr(crtTk,"a scalar can be converted only to another scalar");
            *r = (Ret){t, 0, 1};
            return 1;
        }
        crtTk = startTk;
    }
    return exprUnary(r);
}

static int exprUnary(Ret *r) {
    if (consume(SUB) || consume(NOT)) {
        if (!exprUnary(r)) tkerr(crtTk,"invalid expression after unary operator");
        if (!canBeScalar(r)) tkerr(crtTk,"unary - or ! must have a scalar operand");
        r->lval = 0;
        r->ct   = 1;
        return 1;
    }
    return exprPostfix(r);
}

static int exprPostfix(Ret *r) {
    if (!exprPrimary(r)) return 0;
    return exprPostfixAux(r);
}

static int exprPostfixAux(Ret *r) {
    if (consume(LBRACKET)) {
        Ret idx;
        if (!expr(&idx)) tkerr(crtTk,"invalid expression in [ ]");
        if (!consume(RBRACKET)) tkerr(crtTk,"missing ]");
        if (r->type.nElements < 0) tkerr(crtTk,"only an array can be indexed");
        Type tInt = {TB_INT, NULL, -1};
        if (!convTo(&idx.type, &tInt)) tkerr(crtTk,"index is not convertible to int");
        r->type.nElements = -1;
        r->lval = 1;
        r->ct   = 0;
        return exprPostfixAux(r);
    }
    if (consume(DOT)) {
        if (!consume(ID)) tkerr(crtTk,"missing identifier after .");
        Token *tkName = consumedTk;
        if (r->type.typeBase != TB_STRUCT)
            tkerr(crtTk,"a field can only be selected from a struct");
        Symbol *s = findSymbol(&r->type.s->members, tkName->text);
        if (!s) tkerr(crtTk,"the struct does not have a field named %s", tkName->text);
        *r = (Ret){s->type, 1, 0};
        return exprPostfixAux(r);
    }
    return 1;
}

/* exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
             | CT_INT | CT_REAL | CT_CHAR | CT_STRING
             | LPAR expr RPAR */
static int exprPrimary(Ret *r) {
    if (consume(ID)) {
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(&symbols, tkName->text);
        if (!s) tkerr(crtTk, "undefined id: %s", tkName->text);

        if (consume(LPAR)) {
            // function call
            if (s->cls != CLS_FUNC && s->cls != CLS_EXTFUNC)
                tkerr(crtTk, "only a function can be called");
            Ret rArg;
            int paramCount = (int)(s->args.end - s->args.begin);
            int argIdx = 0;

            if (expr(&rArg)) {
                if (argIdx >= paramCount) tkerr(crtTk, "too many arguments in function call");
                if (!convTo(&rArg.type, &s->args.begin[argIdx]->type))
                    tkerr(crtTk, "cannot convert argument type to parameter type");
                argIdx++;
                while (consume(COMMA)) {
                    if (!expr(&rArg)) tkerr(crtTk,"invalid expression after ,");
                    if (argIdx >= paramCount) tkerr(crtTk, "too many arguments in function call");
                    if (!convTo(&rArg.type, &s->args.begin[argIdx]->type))
                        tkerr(crtTk, "cannot convert argument type to parameter type");
                    argIdx++;
                }
            }
            if (argIdx != paramCount) tkerr(crtTk, "too few arguments in function call");
            if (!consume(RPAR)) tkerr(crtTk,"missing ) in function call");
            *r = (Ret){s->type, 0, 1};
        } else {
            // plain variable
            if (s->cls == CLS_FUNC || s->cls == CLS_EXTFUNC)
                tkerr(crtTk, "a function can only be called");
            *r = (Ret){s->type, 1, 0};
        }
        return 1;
    }

    if (consume(CT_INT)) {
        *r = (Ret){{TB_INT, NULL, -1}, 0, 1};
        return 1;
    }
    if (consume(CT_REAL)) {
        *r = (Ret){{TB_DOUBLE, NULL, -1}, 0, 1};
        return 1;
    }
    if (consume(CT_CHAR)) {
        *r = (Ret){{TB_CHAR, NULL, -1}, 0, 1};
        return 1;
    }
    if (consume(CT_STRING)) {
        *r = (Ret){{TB_CHAR, NULL, 0}, 0, 1};
        return 1;
    }

    if (consume(LPAR)) {
        if (!expr(r)) tkerr(crtTk,"invalid expression after (");
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
        Ret rCond;
        if (!expr(&rCond)) tkerr(crtTk,"invalid expression in if");
        if (!canBeScalar(&rCond)) tkerr(crtTk,"the if condition must be a scalar value");
        if (!consume(RPAR)) tkerr(crtTk,"missing ) in if");
        if (!stm()) tkerr(crtTk,"missing if statement");
        if (consume(ELSE)) {
            if (!stm()) tkerr(crtTk,"missing else");
        }
        return 1;
    }

    if (consume(WHILE)) {
        if (!consume(LPAR)) tkerr(crtTk,"missing ( after while");
        Ret rCond;
        if (!expr(&rCond)) tkerr(crtTk,"invalid expression in while");
        if (!canBeScalar(&rCond)) tkerr(crtTk,"the while condition must be a scalar value");
        if (!consume(RPAR)) tkerr(crtTk,"missing ) in while");
        if (!stm()) tkerr(crtTk,"missing while statement");
        return 1;
    }

    if (consume(FOR)) {
        if (!consume(LPAR)) tkerr(crtTk,"missing ( after for");
        Ret rInit, rCond, rStep;
        expr(&rInit);
        if (!consume(SEMICOLON)) tkerr(crtTk,"missing ; in for");
        if (expr(&rCond)) {
            if (!canBeScalar(&rCond)) tkerr(crtTk,"the for condition must be a scalar value");
        }
        if (!consume(SEMICOLON)) tkerr(crtTk,"missing ; in for");
        expr(&rStep);
        if (!consume(RPAR)) tkerr(crtTk,"missing ) in for");
        if (!stm()) tkerr(crtTk,"missing for statement");
        return 1;
    }

    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) tkerr(crtTk,"missing ; after break");
        return 1;
    }

    if (consume(RETURN)) {
        Ret rExpr;
        if (expr(&rExpr)) {
            if (crtFunc->type.typeBase == TB_VOID)
                tkerr(crtTk,"a void function cannot return a value");
            if (!canBeScalar(&rExpr))
                tkerr(crtTk,"the return value must be a scalar value");
            if (!convTo(&rExpr.type, &crtFunc->type))
                tkerr(crtTk,"cannot convert return expression to function return type");
        } else {
            if (crtFunc->type.typeBase != TB_VOID)
                tkerr(crtTk,"a non-void function must return a value");
        }
        if (!consume(SEMICOLON)) tkerr(crtTk,"missing ; after return");
        return 1;
    }

    Ret rExpr;
    if (expr(&rExpr)) {
        if (!consume(SEMICOLON)) tkerr(crtTk,"missing ; after expression");
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
    addExtFuncs();
    unit();
    printf("Syntax OK\n");
    showSymbolTable();
}