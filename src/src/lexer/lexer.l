%option noinput
%option nounput
%option noyywrap
%option yylineno

%{
#include "ast/tree.hpp"
#include "parser/parser.tab.hh"
#include <iostream>
#include <string>
#include <cstdlib>
extern FILE *input;
void yyerror(const char *s);
%}

digit [0-9]
blank [ \t\n\r]
letter [_a-zA-Z]
linecomment \/\/[^\n]*
comment \/\*([^*]|\*[^/])*\*\/
identifier {letter}({letter}|{digit})*
octal 0[0-7]+
hex 0[xX][0-9a-fA-F]+

%%

{octal}         { yylval.int_val = strtol(yytext, NULL, 8); return INTCONST; }
{hex}           { yylval.int_val = strtol(yytext, NULL, 16); return INTCONST; }
"0"             { yylval.int_val = 0; return INTCONST; }
[1-9]{digit}*   { yylval.int_val = atoi(yytext); return INTCONST; }
"+"             { return ADD; }
"-"             { return SUB; }
"*"             { return MUL; }
"/"             { return DIV; }
"%"             { return MOD; }
"<"             { return LT; }
">"             { return GT; }
"=="            { return EQ; }
"!="            { return NEQ; }
"<="            { return LE; }
">="            { return GE; }
"("             { return LPAREN; }
")"             { return RPAREN; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
"="             { return ASSIGN; }
","             { return COMMA; }
";"             { return SEMICOLON; }
"&&"            { return AND; }
"||"            { return OR; }
"!"             { return NOT; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"void"          { return VOID; }
"["             { return LSQB; }
"]"             { return RSQB; }
"return"        { return RETURN; }
"int"           { return INT; }
{identifier}    { yylval.str_val = strdup(yytext); return IDENT; }
{linecomment}   { }
{comment}       { }
{blank}         { }
.               { std::string msg = "unknown character '" + std::string(yytext) + "'"; yyerror(msg.c_str()); exit(1); }

%%

