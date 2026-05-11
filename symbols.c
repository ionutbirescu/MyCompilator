//
// Created by ionut on 5/2/2026.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbols.h"

#define SAFEALLOC(var, Type) \
    if((var = (Type*)malloc(sizeof(Type))) == NULL) err("not enough memory");

Symbols symbols;
int crtDepth = 0;
Symbol *crtStruct = NULL;
Symbol *crtFunc = NULL;

void initSymbols(Symbols *symbols) {
    symbols->begin = symbols->end = symbols->after = NULL;
}

Symbol *addSymbol(Symbols *symbols, const char* name, int cls) {
    Symbol *s;
    if (symbols->end == symbols->after) {
        int count = (int)(symbols->after - symbols->begin);
        int n = count * 2;
        if (n==0) n=1;
        symbols->begin = (Symbol**)realloc(symbols->begin,sizeof(Symbol*)*n);
        if (symbols->begin == NULL) err("not enough memory");
        symbols->end   = symbols->begin + count;
        symbols->after = symbols->begin + n;
    }
    SAFEALLOC(s,Symbol);
    *symbols->end++ = s;

    s->name = name;
    s->cls = cls;
    s->depth = crtDepth;

    s->mem            = MEM_GLOBAL;
    s->type.typeBase  = TB_VOID;
    s->type.s         = NULL;
    s->type.nElements = -1;
    initSymbols(&s->args);

    return s;
}

Symbol *findSymbol(Symbols *symbols, const char* name) {
    if (symbols->begin == NULL) return NULL;
    for (Symbol **p = symbols->end-1; p >= symbols->begin; p--) {
        if (strcmp((*p)->name, name) == 0) return (*p);
    }
    return NULL;
}

void deleteSymbolsAfter(Symbols *symbols, Symbol* start) {
    if (symbols->begin == NULL) return;

    Symbol **cut;
    if (start == NULL) {
        cut = symbols->begin;
    } else {
        cut = symbols->end;
        for (Symbol **p = symbols->begin; p!=symbols->end; p++) {
            if (*p == start) {
                cut = p+1;
                break;
            }
        }
    }

    for (Symbol **p = cut; p != symbols->end; p++) {
        free(*p);
    }

    symbols->end = cut;
}

Symbol *addExtFunc(const char* name, Type type) {
    Symbol *s = addSymbol(&symbols, name, CLS_EXTFUNC);
    s->type = type;
    s->mem = MEM_GLOBAL;
    initSymbols(&s->args);
    return s;
}

Symbol *addFuncArg(Symbol *func, const char *name, Type type) {
    Symbol *a = addSymbol(&func->args, name, CLS_VAR);
    a->type  = type;
    a->mem   = MEM_ARG;
    return a;
}

static const char *clsName(int c) {
    switch (c) {
        case CLS_VAR:     return "VAR";
        case CLS_FUNC:    return "FUNC";
        case CLS_EXTFUNC: return "EXTFUNC";
        case CLS_STRUCT:  return "STRUCT";
        default:          return "?";
    }
}

static const char *memName(int m) {
    switch (m) {
        case MEM_GLOBAL: return "GLOBAL";
        case MEM_ARG:    return "ARG";
        case MEM_LOCAL:  return "LOCAL";
        default:         return "?";
    }
}

static const char *tbName(int tb) {
    switch (tb) {
        case TB_INT:    return "int";
        case TB_DOUBLE: return "double";
        case TB_CHAR:   return "char";
        case TB_VOID:   return "void";
        case TB_STRUCT: return "struct";
        default:        return "?";
    }
}

static void printType(Type *t) {
    printf("%s", tbName(t->typeBase));
    if (t->typeBase == TB_STRUCT && t->s) printf(" %s", t->s->name);
    if      (t->nElements == 0) printf("[]");
    else if (t->nElements >  0) printf("[%d]", t->nElements);
}

static void showSymbols(Symbols *list, int indent) {
    for (Symbol **p = list->begin; p != list->end; p++) {
        Symbol *s = *p;
        for (int i = 0; i < indent; i++) printf("  ");
        printf("%-12s  cls=%-7s  mem=%-6s  depth=%d  type=",
               s->name, clsName(s->cls), memName(s->mem), s->depth);
        printType(&s->type);
        //if (s->owner) printf("  owner=%s", s->owner->name);
        printf("\n");

        if (s->cls == CLS_FUNC || s->cls == CLS_EXTFUNC) {
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("args:\n");
            showSymbols(&s->args, indent + 2);
        } else if (s->cls == CLS_STRUCT) {
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("members:\n");
            showSymbols(&s->members, indent + 2);
        }
    }
}

int canBeScalar(Ret *r) { //neither array nor struct
    return r->type.nElements < 0 && r->type.typeBase != TB_STRUCT;
}

Type getArithType(Type *s1, Type *s2) {
    Type t;
    t.s = NULL;
    t.nElements = -1;
    if (s1->typeBase == TB_DOUBLE || s2->typeBase == TB_DOUBLE) {
        t.typeBase = TB_DOUBLE;
    } else if (s1->typeBase == TB_INT || s2->typeBase == TB_INT) {
        t.typeBase = TB_INT;
    } else {
        t.typeBase = TB_CHAR;
    }
    return t;
}

int arithTypeTo(Type *s1, Type *s2, Type *dst) {
    if (s1->typeBase == TB_STRUCT || s2->typeBase == TB_STRUCT) return 0;
    if (s1->nElements >= 0 || s2->nElements >= 0) return 0;
    *dst = getArithType(s1, s2);
    return 1;
}

Type createType(int typeBase, int nElements) {
    Type t;
    t.typeBase = typeBase;
    t.nElements = nElements;
    t.s = NULL;
    return t;
}

int convTo(Type *src, Type *dst) {
    if (src->nElements > -1) {
        // array: must match array of same base type
        if (dst->nElements > -1) {
            return src->typeBase == dst->typeBase;
        }
        return 0;
    }

    if (dst->nElements > -1) return 0;

    //both scalars
    switch (src->typeBase) {
        case TB_CHAR: case TB_INT: case TB_DOUBLE:
            switch (dst->typeBase) {
                case TB_CHAR: case TB_INT: case TB_DOUBLE: return 1;
            }
        case TB_STRUCT:
            return dst->typeBase == TB_STRUCT && src->s == dst->s;
    }
    return 0;
}

void addExtFuncs() {
    Symbol *s;
    s = addExtFunc("put_s", createType(TB_VOID, -1));
    addFuncArg(s, "s", createType(TB_CHAR, 0));
    s = addExtFunc("get_s", createType(TB_VOID, -1));
    addFuncArg(s, "s", createType(TB_CHAR, 0));
    s = addExtFunc("put_i", createType(TB_VOID, -1));
    addFuncArg(s, "i", createType(TB_INT, -1));
    s = addExtFunc("get_i", createType(TB_INT, -1));
    s = addExtFunc("put_d", createType(TB_VOID, -1));
    addFuncArg(s, "d", createType(TB_DOUBLE, -1));
    s = addExtFunc("get_d", createType(TB_DOUBLE, -1));
    s = addExtFunc("put_c", createType(TB_VOID, -1));
    addFuncArg(s, "c", createType(TB_CHAR, -1));
    s = addExtFunc("get_c", createType(TB_CHAR, -1));
    s = addExtFunc("seconds", createType(TB_DOUBLE, -1));
}

void showSymbolTable(void) {
    printf(" Symbol Table \n");
    showSymbols(&symbols, 0);
}