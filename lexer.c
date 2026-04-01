#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAFEALLOC(var,Type) if((var=(Type*)malloc(sizeof(Type)))==NULL)err("not enough memory");

enum {
    //CONSTANTS AND IDENTIFIERS
    ID, CT_INT, CT_REAL, CT_CHAR, CT_STRING,

    //SEPARATORS & OPERATORS
    AND, SEMICOLON, OR, SPACE, DOT, LPAR, RPAR, DIV, COMMA, LINECOMMENT, END,
    ASSIGN, EQUALS, LBRACKET, RBRACKET, NOT, NOTEQ, LACC, RACC, LESS, LESSEQ,
    GREATER, GREATEREQ, ADD, SUB, MUL,

    BREAK, CHAR, DOUBLE, ELSE, FOR, IF, INT, RETURN, STRUCT, VOID, WHILE,
};

const char *tokenNames[] = {
    [ID]="ID", [CT_INT]="CT_INT", [CT_REAL]="CT_REAL", [CT_CHAR]="CT_CHAR", [CT_STRING]="CT_STRING",
    [AND]="AND", [SEMICOLON]="SEMICOLON", [OR]="OR", [SPACE]="SPACE", [DOT]="DOT",
    [LPAR]="LPAR", [RPAR]="RPAR", [DIV]="DIV", [COMMA]="COMMA", [LINECOMMENT]="LINECOMMENT",
    [END]="END", [ASSIGN]="ASSIGN", [EQUALS]="EQUALS", [LBRACKET]="LBRACKET", [RBRACKET]="RBRACKET",
    [NOT]="NOT", [NOTEQ]="NOTEQ", [LACC]="LACC", [RACC]="RACC",
    [LESS]="LESS", [LESSEQ]="LESSEQ", [GREATER]="GREATER", [GREATEREQ]="GREATEREQ",
    [ADD]="ADD", [SUB]="SUB", [MUL]="MUL",
    [BREAK]="BREAK", [CHAR]="CHAR", [DOUBLE]="DOUBLE", [ELSE]="ELSE", [FOR]="FOR",
    [IF]="IF", [INT]="INT", [RETURN]="RETURN", [STRUCT]="STRUCT", [VOID]="VOID", [WHILE]="WHILE"
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

Token *tokens = NULL;
Token *lastToken = NULL;
int line = 1;
const char *pCrtCh;

void err(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, va);
    fputc('\n',stderr);
    va_end(va);
    exit(1);
}

Token *addToken(int code) {
    Token *newToken;
    SAFEALLOC(newToken, Token)
    newToken->code = code;
    newToken->line = line;
    newToken->next = NULL;
    if (lastToken) {
        lastToken->next = newToken;
    } else {
        tokens = newToken;
    }
    lastToken = newToken;
    return newToken;
}

void tkerr(const Token *tk, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error in line %d: ", tk->line);
    vfprintf(stderr, fmt, va);
    fputc('\n',stderr);
    va_end(va);
    exit(-1);
}

char *createString(const char *pStart, const char *pCrtCh) {
    int n = (int) (pCrtCh - pStart);
    char *s = (char *) malloc(n + 1);
    if (!s) {
        err("not enough memory");
    }
    memcpy(s, pStart, n);
    s[n] = '\0';
    return s;
}

int getNextToken() {
    int state = 0;
    int nCh;
    const char *pStartCh = 0;
    Token *tk;

    while (1) {
        char ch = *pCrtCh;
        switch (state) {
            case 0:
                if (ch >= '0' && ch <= '9'){ // Digit: start number (decimal, octal, or hex)
                    if (ch == '0') { // Starts with '0': could be octal, hex, or real
                        pStartCh = pCrtCh;
                        pCrtCh += 1;
                        state = 2;
                    }
                    else { // Starts with 1-9: decimal integer or real
                        pStartCh = pCrtCh;
                        pCrtCh += 1;
                        state = 1;
                    }
                } else if (ch == '"') { // Opening double quote: start string
                    pStartCh = pCrtCh + 1;
                    pCrtCh += 1;
                    state = 3;
                } else if (ch == '\'') { // Opening single quote: start character constant
                    pCrtCh++;
                    state = 4;
                } else if (isalpha(ch) || ch == '_') { // Letter or underscore: start ID/keyword
                    pStartCh = pCrtCh;
                    pCrtCh += 1;
                    state = 5;
                } else if (ch == '&') { // '&': expect '&&' (AND operator)
                    pCrtCh += 1;
                    state = 6;
                } else if (ch == ',') {
                    pCrtCh++;
                    addToken(COMMA);
                    return COMMA;
                } else if (ch == ';') {
                    pCrtCh += 1;
                    addToken(SEMICOLON);
                    return SEMICOLON;
                } else if (ch == '|') {
                    pCrtCh += 1;
                    state = 7;
                } else if (ch == ' ' || ch == '\r' || ch == '\t') {
                    pCrtCh += 1;
                } else if (ch == '\n') {
                    pCrtCh += 1;
                    line++;
                } else if (ch == '(') {
                    pCrtCh += 1;
                    addToken(LPAR);
                    return LPAR;
                } else if (ch == ')') {
                    pCrtCh += 1;
                    addToken(RPAR);
                    return RPAR;
                } else if (ch == '/') {
                    pCrtCh += 1;
                    state = 8; // DIV OR LINECOMMENT
                } else if (ch == '*') {
                    pCrtCh += 1;
                    addToken(MUL);
                    return MUL;
                } else if (ch == '+') {
                    pCrtCh += 1;
                    addToken(ADD);
                    return ADD;
                } else if (ch == '-') {
                    pCrtCh += 1;
                    addToken(SUB);
                    return SUB;
                } else if (ch == '}') {
                    pCrtCh += 1;
                    addToken(RACC);
                    return RACC;
                } else if (ch == '{') {
                    pCrtCh += 1;
                    addToken(LACC);
                    return LACC;
                } else if (ch == ']') {
                    pCrtCh += 1;
                    addToken(RBRACKET);
                    return RBRACKET;
                } else if (ch == '[') {
                    pCrtCh += 1;
                    addToken(LBRACKET);
                    return LBRACKET;
                } else if (ch == '.') {
                    pCrtCh += 1;
                    addToken(DOT);
                    return DOT;
                } else if (ch == '!') {
                    pCrtCh += 1;
                    state = 9;
                } else if (ch == '=') {
                    pCrtCh += 1;
                    state = 10;
                } else if (ch == '<') {
                    pCrtCh += 1;
                    state = 11;
                } else if (ch == '>') {
                    pCrtCh += 1;
                    state = 12;
                } else if (ch == 0) {
                    addToken(END);
                    return END;
                } else {
                    tkerr(addToken(END), "invalid character: %c", ch);
                }
                break;
            case 1: // Decimal integer (starts with 1-9)
                if (ch >= '0' && ch <= '9') {
                    pCrtCh++;
                }
                else if (ch == '.') {
                    pCrtCh++;
                    state = 16;
                }
                else if (ch == 'e' || ch == 'E') {
                    pCrtCh++;
                    state = 17;
                }
                else {
                    tk = addToken(CT_INT);
                    tk->i = strtol(pStartCh, NULL, 10);
                    return CT_INT;
                }
                break;
            case 2: // Octal/hex/real
                if (ch=='x' || ch=='X') {
                    pCrtCh++;
                    state = 18;
                } else if (ch>='0' && ch<='7') {
                    pCrtCh++;
                    state = 19;
                } else if (ch == 'e' || ch == 'E') {
                    pCrtCh++;
                    state = 17;
                } else if ( ch == '.') {
                    pCrtCh+=1;
                    state = 16;
                } else {
                    tk = addToken(CT_INT);
                    tk->i = 0;
                    return CT_INT;
                }
                break;
            case 3: // string constant
                if (ch == '"') {
                    tk = addToken(CT_STRING);
                    tk->text = createString(pStartCh, pCrtCh);
                    pCrtCh += 1;
                    return CT_STRING;
                } else if (ch == '\\') {
                    pCrtCh += 1; // skip escape backslash
                    pCrtCh += 1; // skip escaped character
                } else if (ch == 0) {
                    tkerr(addToken(END), "unterminated string");
                } else {
                    if (ch == '\n') {
                        line++;
                    }
                    pCrtCh += 1;
                }
                break;
            case 4: // char constant
                if (ch == '\\') {
                    pCrtCh += 1;
                    ch = *pCrtCh;
                    pCrtCh += 1;
                    state = 13;
                    switch (ch) {
                        case 'n': ch = '\n';
                            break;
                        case 'r': ch = '\r';
                            break;
                        case 't': ch = '\t';
                            break;
                        case '\\': ch = '\\';
                            break;
                        case '\'': ch = '\'';
                            break;
                        case '0': ch = '\0';
                            break;
                        default:
                            tkerr(addToken(END), "invalid escape sequence in char constant");
                    }
                    pStartCh = pCrtCh - 1;
                } else if (ch == '\'' || ch == 0) {
                    tkerr(addToken(END), "empty or invalid char constant");
                } else {
                    pCrtCh += 1;
                    state = 13;
                }
                break;
            case 5: // identifier or keyword
                if (isalnum(ch) || ch == '_') {
                    pCrtCh += 1;
                } else {
                    state = 14;
                }
                break;
            case 6: // got '&', expected second &
                if (ch == '&') {
                    pCrtCh += 1;
                    addToken(AND);
                    return AND;
                } else {
                    tkerr(addToken(END), "invalid character after '&' (expected '&&')");
                }
                break;
            case 7: // got '|', expected second |
                if (ch == '|') {
                    pCrtCh += 1;
                    addToken(OR);
                    return OR;
                } else {
                    tkerr(addToken(END), "invalid character after '|' (expected '||')");
                }
                break;
            case 8: // got '/' check for line comment or div
                if (ch == '/') { // line comment
                    pCrtCh += 1;
                    state = 15;
                } else {
                    addToken(DIV);
                    return DIV;
                }
                break;
            case 9: // got '!', expected '=' or NOT
                if (ch == '=') {
                    pCrtCh += 1;
                    addToken(NOTEQ);
                    return NOTEQ;
                } else {
                    addToken(NOT);
                    return NOT;
                }
            case 10: // got '=', expected EQUALS/ASSIGN
                if (ch == '=') {
                    pCrtCh += 1;
                    addToken(EQUALS);
                    return EQUALS;
                } else {
                    addToken(ASSIGN);
                    return ASSIGN;
                }
            case 11: // got '<'
                if (ch == '=') {
                    pCrtCh += 1;
                    addToken(LESSEQ);
                    return LESSEQ;
                } else {
                    addToken(LESS);
                    return LESS;
                }
            case 12: // got '>'
                if (ch == '=') {
                    pCrtCh += 1;
                    addToken(GREATEREQ);
                    return GREATEREQ;
                } else {
                    addToken(GREATER);
                    return GREATER;
                }
            case 13: // char constant, expected single quote
                if (ch == '\'') {
                    tk = addToken(CT_CHAR);
                    tk->i = *(pCrtCh - 1);
                    pCrtCh += 1;
                    return CT_CHAR;
                } else {
                    tkerr(addToken(END), "missing closing quote for char constant");
                }
                break;
            case 14: // identifier complete, either we have keyword or some ID
                int newChar = (int) (pCrtCh - pStartCh);
                // Test keywords
                if (newChar == 5 && !memcmp(pStartCh, "break", 5)) tk = addToken(BREAK);
                else if (newChar == 4 && !memcmp(pStartCh, "char", 4)) tk = addToken(CHAR);
                else if (newChar == 6 && !memcmp(pStartCh, "double", 6)) tk = addToken(DOUBLE);
                else if (newChar == 4 && !memcmp(pStartCh, "else", 4)) tk = addToken(ELSE);
                else if (newChar == 3 && !memcmp(pStartCh, "for", 3)) tk = addToken(FOR);
                else if (newChar == 2 && !memcmp(pStartCh, "if", 2)) tk = addToken(IF);
                else if (newChar == 3 && !memcmp(pStartCh, "int", 3)) tk = addToken(INT);
                else if (newChar == 6 && !memcmp(pStartCh, "return", 6)) tk = addToken(RETURN);
                else if (newChar == 6 && !memcmp(pStartCh, "struct", 6)) tk = addToken(STRUCT);
                else if (newChar == 4 && !memcmp(pStartCh, "void", 4)) tk = addToken(VOID);
                else if (newChar == 5 && !memcmp(pStartCh, "while", 5)) tk = addToken(WHILE);
                else {
                    tk = addToken(ID);
                    tk->text = createString(pStartCh, pCrtCh);
                }
                return tk->code;
            case 15: // Line comment - we just skip it, we consume until the end of it, then back to state 0
                if (ch == '\n') {
                    line++;
                    pCrtCh++;
                    state = 0;
                }
                else if (ch == '\r' || ch == 0) {
                    state = 0;
                }
                else {
                    pCrtCh++;
                }
                break;
            case 16: // got '.' after digits - expected one digit for real num
                if ( ch>= '0' && ch <= '9' ) {
                    pCrtCh++;
                    state = 20;
                }
                else {
                    tkerr(addToken(END), "digit expected after '.'");
                }
                break;
            case 17: // got 'e'/'E' after integer digit - exponent part (we can also have +/-)
                if (ch == '+' || ch == '-') {
                    pCrtCh++;
                    state = 22;
                } else if (ch>= '0' && ch <= '9') {
                    pCrtCh++;
                    state = 23;
                }
                else {
                    tkerr(addToken(END), "digit expected in exponent or +-");
                }
                break;
            case 18: // got '0x' - expect at least one hex digit
                if (isxdigit(ch)) {
                    pCrtCh++;
                    state = 24;
                }
                else {
                    tkerr(addToken(END), "invalid hex number");
                }
                break;
            case 19: // octal number'
                if (ch>='0' && ch<='7') {
                    pCrtCh++;
                } else {
                    tk = addToken(CT_INT);
                    tk->i = strtol(pStartCh, NULL, 8);
                    return CT_INT;
                }
                break;
            case 20: // real fractional part - consume digits, may get 'E'/'e' for exponent
                if (ch >= '0' && ch <= '9') {
                    pCrtCh++;
                }
                else if (ch == 'e' || ch == 'E') {
                    pCrtCh++;
                    state = 21;
                } else {
                    tk = addToken(CT_REAL);
                    tk->r = strtod(createString(pStartCh, pCrtCh), NULL);
                    return CT_REAL;
                }
                break;
            case 21: // get 'e'/'E' after fractional part - exponent ( +/- opt)
                if (ch == '+' || ch == '-') {
                    pCrtCh++;
                    state = 22;
                }
                else if (ch >= '0' && ch <= '9') {
                    pCrtCh++;
                    state = 23;
                }
                else {
                    tkerr(addToken(END), "digit expected in exponent");
                }
                break;
            case 22: // got '+' or '-' after exponent - expect at least one digit
                if (ch >= '0' && ch <= '9') {
                    pCrtCh++;
                    state = 23;
                }
                else {
                    tkerr(addToken(END), "digit expected after exponent sign");
                }
                break;
            case 23: // exponent digits
                if (ch>='0' && ch<='9') {
                    pCrtCh++;
                }
                else {
                    tk = addToken(CT_REAL);
                    tk->r = strtod(createString(pStartCh, pCrtCh), NULL);
                    return CT_REAL;
                }
                break;
            case 24: //hex digits
                if (isxdigit(ch)) {
                    pCrtCh++;
                } else {
                    tk = addToken(CT_INT);
                    tk->i = strtol(pStartCh, NULL, 16);
                    return CT_INT;
                }
                break;
            default:
                err("undefined state %d",state);
        }
    }
}

char *loadFile(const char *fileName) {
    FILE *fis = fopen(fileName, "rb");
    if (!fis) {
        err("cannot open file %s", fileName);
    }
    fseek(fis, 0, SEEK_END);
    const long n = ftell(fis);
    fseek(fis, 0, SEEK_SET);
    char *buf = (char *) malloc(n + 1);
    if (!buf) {
        err("not enough memory");
    }
    fread(buf, 1, n, fis);
    buf[n] = '\0';
    fclose(fis);
    return buf;
}

void showTokens() {
    const Token *tk = tokens;
    while (tk) {
        printf(" %s", tokenNames[tk->code]);
        if (tk->code == ID || tk->code == CT_STRING) {
            printf(":%s", tk->text);
        } else if (tk->code == CT_INT || tk->code == CT_CHAR) {
            printf(":%ld", tk->i);
        } else if (tk->code == CT_REAL) {
            printf(":%g", tk->r);
        }
        printf(" line:%d\n", tk->line);
        tk = tk->next;
    }
}

void done() {
    Token *tk = tokens;
    while (tk) {
        Token *next = tk->next;
        if (tk->code == ID || tk->code == CT_STRING) {
            free(tk->text);
        }
        free(tk);
        tk = next;
    }
    tokens = NULL;
    lastToken = NULL;
}

int main(const int argc, char *argv[])  {
    if (argc < 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    char *buf = loadFile(argv[1]);
    pCrtCh = buf;

    while (getNextToken() != END) {
    }

    showTokens();
    done();
    free(buf);
    return 0;
}
