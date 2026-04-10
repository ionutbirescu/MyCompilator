cmake_minimum_required(VERSION 3.20)
project(MyCompilator C)

set(CMAKE_C_STANDARD 17)

add_executable(MyCompilator
        lexer.c
        parser.c
        parser.h
        lexer.h
)

