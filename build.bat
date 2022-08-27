@echo off
rem clang src/*.c tildebackend.lib -g -v -o bin/foo.exe

lemon\lemon src/parser.y
cl src/*.c tildebackend.lib /nologo /Zi /Fo:bin/ /Fe:bin/foo.exe /Fd:bin/foo.pdb
