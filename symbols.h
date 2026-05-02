//
// Created by I769044 on 5/2/2026.
//

#ifndef MYCOMPILATOR_SYMBOLS_H
#define MYCOMPILATOR_SYMBOLS_H

#include "lexer.h"

//Type base codes
enum {
    TB_INT,
    TB_DOUBLE,
    TB_CHAR,
    TB_VOID,
    TB_STRUCT
};

//symbol classes
enum {
    CLS_VAR, // variable(global,local,struct memb)
    CLS_FUNC,
    CLS_EXTFUNC,
    CLS_STRUCT
};

//symbol mem allocation
enum {
    MEM_GLOBAL,
    MEM_ARG,
    MEM_LOCAL
};

struct _Symbol;
typedef struct _Symbol Symbol;

typedef struct {
    Symbol **begin, **end, **after;
} Symbols;

typedef struct {
    int typeBase;
    Symbol *s;
    int nElements;
}Type;

struct _Symbol {
    const char *name;
    int cls;
    int mem;
    Type type;
    int depth;

    union {
        Symbols args;
        Symbols members;
    };
};

extern Symbols symbols;
extern int crtDepth;
extern Symbol *crtStruct;
extern Symbol *crtFunc;

void initSymbols(Symbols *symbols);
Symbol *addSymbol(Symbols *symbols, const char *name, int cls);
Symbol *findSymbol(Symbols *symbols, const char *name);
void deleteSymbolsAfter(Symbols* symbols, Symbol* start);

Symbol *addExtFunc(const char* name, Type type); // adds a CLS_EXTFUNC to global symbols
Symbol *addFuncArg(Symbol *func, const char* name, Type type); // adds a CLS_VAR/MEM_ARG to func->args

void showSymbolTable(void);

#endif //MYCOMPILATOR_SYMBOLS_H