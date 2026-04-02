// Test 1: Keywords
int char double void struct
if else while for break return

// Test 2: Identifiers
x myVar _private name123 _a_b_c

// Test 3: Integer constants
// Decimal
0 5 25 100 9999
// Octal
017 076 01
// Hex
0x0 0xAFd9 0xFF 0x1a2B

// Test 4: Real constants
3.14 0.5 100.0
1.5e3 2.0E10
3.14e-2 1.0e+5
0.001e2

// Test 5: Char constants
'a' 'Z' '0' '\n' '\t' '\\'

// Test 6: String constants
"hello" "world 123" "with\nnewline" ""

// Test 7: All operators
+ - * /
= == != !
< <= > >=
&& ||

// Test 8: All separators
( ) [ ] { } , ; .

// Test 9: A realistic program
int main() {
    int arr[10];
    double result = 0.0;
    char *msg = "done";

    for (int i = 0; i < 10; i = i + 1) {
        arr[i] = i * 2;
        result = result + 3.14e-1;
    }

    if (result >= 5.0 && !0) {
        return 0;
    } else {
        return 1;
    }
}

// Test 10: Line comment should be ignored
// this entire line is a comment
int afterComment = 42;