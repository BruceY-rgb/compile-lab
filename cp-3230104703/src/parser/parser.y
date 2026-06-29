%{
#include <cstdio>
#include <memory>
#include <string>
#include <iostream>
#include "ast/tree.hpp"
#define YYDEBUG 1
void yyerror(const char *s);
extern int yylex(void);
extern int yylineno;
using namespace AST;
extern NodePtr root;

template <typename T>
inline std::shared_ptr<T> shared_cast(Node *ptr) {
  return std::shared_ptr<T>(static_cast<T *>(ptr));
}

inline std::string take_string(char *s) {
    std::string result(s);
    free(s);
    return result;
}

%}

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 这里的 union 是一种特殊的数据结构，所有成员共享相同的内存地址
// 因此只能存储一个成员的值，且占用的内存大小等于其最大成员的大小
// union 中的类型必须是 trivially copyable 的类型
// 也就是说不能包含有自定义的构造函数、析构函数、虚函数等
// 符合这个条件的类型有基本数据类型、指针、C 结构体、枚举等
// 因此我们不能使用 std::shared_ptr，而只能使用普通指针
%union{
    int int_val;
    char *str_val;
    UnaryOp op;
    BasicType btype;
    AST::Node *node;
}

%error-verbose
%destructor { free($$); } IDENT

%start AstRoot
%token ADD "+"
%token SUB "-"
%token ASSIGN "="
%token MUL "*"
%token DIV "/"
%token MOD "%"
%token LT "<"
%token GT ">"
%token LE "<="
%token GE ">="
%token EQ "=="
%token NEQ "!="
%token AND "&&"
%token OR "||"
%token NOT "!"
%token SEMICOLON ";"
%token COMMA ","
%token LPAREN "("
%token RPAREN ")"
%token LBRACE "{"
%token RBRACE "}"
%token IF "if"
%token ELSE "else"
%token WHILE "while"
%token VOID "void"
%token LSQB "["
%token RSQB "]"
%token INT "int"
%token RETURN "return"
%token <str_val> IDENT
%token <int_val> INTCONST

%type <node> AstRoot CompUnit Decl VarDecl VarDefs VarDef FuncDef Block BlockItem BlockItems Stmt Exp LVal PrimaryExp IntConst UnaryExp FuncRParams MulExp AddExp RelExp EqExp LAndExp LOrExp Cond FuncParams InitVal InitVals ArrayDims LValIndices
%type <btype> FuncType
%type <op> UnaryOp
%type <node> FuncParam

%%

// 由于我们在后续代码中所有指针都是 std::shared_ptr
// 而在 union 中没法直接定义 std::shared_ptr
// 所以我们在这里需要将普通的指针转换成 std::shared_ptr
AstRoot : CompUnit { root = NodePtr($1); }
    ;

CompUnit : FuncDef { $$ = new CompUnit(shared_cast<FuncDef>($1)); }
    | VarDecl { $$ = new CompUnit(shared_cast<VarDecl>($1)); }
    | CompUnit FuncDef { static_cast<CompUnit*>($1)->add_unit(shared_cast<FuncDef>($2)); $$ = $1; }
    | CompUnit VarDecl { static_cast<CompUnit*>($1)->add_unit(shared_cast<VarDecl>($2)); $$ = $1; }
    ;

Decl : VarDecl { $$ = $1; }
    ;

// 由于 union 中的类型不能是 std::shared_ptr
// 只是普通的指针，所以我们需要通过 static_cast 来转换类型
// 才能访问到对应的成员和函数
VarDecl : "int" VarDefs ";" { static_cast<VarDecl *>($2)->btype = BasicType::Int; $$ = $2; }
    ;

VarDefs : VarDef { $$ = new VarDecl(shared_cast<VarDef>($1)); }
    | VarDefs "," VarDef { static_cast<VarDecl*>($1)->add_def(shared_cast<VarDef>($3)); $$ = $1; }
    ;

VarDef : IDENT { $$ = new VarDef(take_string($1)); }
    | IDENT "=" InitVal { $$ = new VarDef(take_string($1), NodePtr($3)); }
    | IDENT ArrayDims {
        auto dims = static_cast<DimList*>($2)->dims;
        delete $2;
        $$ = new VarDef(take_string($1), dims);
      }
    | IDENT ArrayDims "=" InitVal {
        auto dims = static_cast<DimList*>($2)->dims;
        delete $2;
        $$ = new VarDef(take_string($1), dims, NodePtr($4));
      }
    ;

ArrayDims : "[" INTCONST "]" {
        auto list = new DimList();
        list->add_dim($2);
        $$ = list;
    }
    | ArrayDims "[" INTCONST "]" {
        auto list = static_cast<DimList*>($1);
        list->add_dim($3);
        $$ = list;
    }
    ;

InitVal : Exp { $$ = $1; }
    | "{" "}" { $$ = new InitList(); }
    | "{" InitVals "}" { $$ = $2; }
    ;

InitVals : InitVal { 
        auto list = new InitList();
        list->add_value(NodePtr($1));
        $$ = list;    
    }
    | InitVals "," InitVal {
        auto list = static_cast<InitList*>($1);
        list->add_value(NodePtr($3));
        $$ = list;
    }
    ;

// FuncType 直接内联到 FuncDef 中，避免与 VarDecl 冲突
FuncType : "int" { $$ = BasicType::Int; }
    | "void" { $$ = BasicType::Void; }
    ;

Cond : LOrExp
Stmt : LVal "=" Exp ";" { $$ = new AssignStmt(shared_cast<LVal>($1), NodePtr($3)); }
    | Exp ";" { $$ = $1; }
    | ";" { $$ = nullptr; }
    | Block { $$ = $1; }
    | "if" "(" Cond ")" Stmt { $$ = new IfStmt(NodePtr($3), NodePtr($5)); }
    | "if" "(" Cond ")" Stmt "else" Stmt { $$ = new IfStmt(NodePtr($3), NodePtr($5), NodePtr($7)); }
    | "while" "(" Cond ")" Stmt { $$ = new WhileStmt(NodePtr($3), NodePtr($5)); }
    | "return" Exp ";" { $$ = new ReturnStmt(NodePtr($2)); }
    | "return" ";" { $$ = new ReturnStmt(); }
    ;

// 使用 FuncType 直接在规则中，而不是通过非终结符
// 这样可以避免 shift/reduce 冲突
FuncDef : "int" IDENT "(" ")" Block { $$ = new FuncDef(BasicType::Int, take_string($2), shared_cast<Block>($5)); }
    | "void" IDENT "(" ")" Block { $$ = new FuncDef(BasicType::Void, take_string($2), shared_cast<Block>($5)); }
    | "int" IDENT "(" FuncParams ")" Block { $$ = new FuncDef(BasicType::Int, take_string($2), shared_cast<FuncParams>($4), shared_cast<Block>($6)); }
    | "void" IDENT "(" FuncParams ")" Block { $$ = new FuncDef(BasicType::Void, take_string($2), shared_cast<FuncParams>($4), shared_cast<Block>($6)); }
    ;

FuncParams : FuncParam { $$ = new FuncParams(); static_cast<FuncParams*>($$)->add_param(shared_cast<FuncParam>($1)); }
    | FuncParams "," FuncParam { static_cast<FuncParams*>($1)->add_param(shared_cast<FuncParam>($3)); $$ = $1; }
    ;

FuncParam : "int" IDENT { $$ = new FuncParam(take_string($2), BasicType::Int); }
    | "int" IDENT "[" "]" { $$ = new FuncParam(take_string($2), BasicType::Int, true); }
    | "int" IDENT "[" "]" ArrayDims {
        auto dims = static_cast<DimList*>($5)->dims;
        dims.insert(dims.begin(), 0);
        delete $5;
        $$ = new FuncParam(take_string($2), BasicType::Int, dims);
      }
    ;

Block : "{" "}" { $$ = new Block(); }
    | "{" BlockItems "}" { $$ = $2; }
    ;

BlockItems : BlockItem
    {
      auto block = new Block();
      if ($1) {
        block->add_stmt(NodePtr($1));
      }
      $$ = block;
    }
    | BlockItems BlockItem
    {
      if ($2) {
        static_cast<Block*>($1)->add_stmt(NodePtr($2));
      }
      $$ = $1;
    }
    ;

BlockItem : Stmt { $$ = $1; }
    | Decl { $$ = $1; }
    ;



Exp : AddExp { $$ = $1; }
    ;

LVal : IDENT { $$ = new LVal(take_string($1)); }
    | IDENT LValIndices {
        auto indices = static_cast<IndexList*>($2)->indices;
        delete $2;
        $$ = new LVal(take_string($1), indices);
      }
    ;

LValIndices : "[" Exp "]" {
        auto list = new IndexList();
        list->add_index(NodePtr($2));
        $$ = list;
    }
    | LValIndices "[" Exp "]" {
        auto list = static_cast<IndexList*>($1);
        list->add_index(NodePtr($3));
        $$ = list;
    }
    ;

PrimaryExp : LVal { $$ = $1; }
    | IntConst { $$ = $1; }
    | "(" Exp ")" { $$ = $2; }
    ;

IntConst : INTCONST { $$ = new IntConst($1); }
    ;

UnaryExp : PrimaryExp { $$ = $1; }
    | IDENT "(" ")" { $$ = new FuncCall(take_string($1)); }
    | IDENT "(" FuncRParams ")" { static_cast<FuncCall*>($3)->name = take_string($1); $$ = $3; }
    | UnaryOp UnaryExp { $$ = new UnaryExp($1, NodePtr($2)); }
    ;

UnaryOp : "+" { $$ = UnaryOp::Pos; }
    | "-" { $$ = UnaryOp::Neg; }
    | "!" { $$ = UnaryOp::Lnot; }
    ;

FuncRParams : Exp { $$ = new FuncCall(NodePtr($1)); }
    | FuncRParams "," Exp { static_cast<FuncCall*>($1)->add_arg(NodePtr($3)); $$ = $1; }
    ;

MulExp : UnaryExp { $$ = $1; }
    | MulExp "*" UnaryExp { $$ = new BinaryExp(BinaryOp::Mul, NodePtr($1), NodePtr($3)); }
    | MulExp "/" UnaryExp { $$ = new BinaryExp(BinaryOp::Div, NodePtr($1), NodePtr($3)); }
    | MulExp "%" UnaryExp { $$ = new BinaryExp(BinaryOp::Mod, NodePtr($1), NodePtr($3)); }
    ;

AddExp : MulExp { $$ = $1; }
    | AddExp "+" MulExp { $$ = new BinaryExp(BinaryOp::Add, NodePtr($1), NodePtr($3)); }
    | AddExp "-" MulExp { $$ = new BinaryExp(BinaryOp::Sub, NodePtr($1), NodePtr($3)); }
    ;

RelExp : AddExp { $$ = $1; }
    | RelExp "<" AddExp { $$ = new BinaryExp(BinaryOp::Lt, NodePtr($1), NodePtr($3)); }
    | RelExp ">" AddExp { $$ = new BinaryExp(BinaryOp::Gt, NodePtr($1), NodePtr($3)); }
    | RelExp "<=" AddExp { $$ = new BinaryExp(BinaryOp::Le, NodePtr($1), NodePtr($3)); }
    | RelExp ">=" AddExp { $$ = new BinaryExp(BinaryOp::Ge, NodePtr($1), NodePtr($3)); }
    ;

EqExp : RelExp { $$ = $1; }
    | EqExp "==" RelExp { $$ = new BinaryExp(BinaryOp::Eq, NodePtr($1), NodePtr($3)); }
    | EqExp "!=" RelExp { $$ = new BinaryExp(BinaryOp::Neq, NodePtr($1), NodePtr($3)); }
    ;

LAndExp : EqExp { $$ = $1; }
    | LAndExp "&&" EqExp { $$ = new BinaryExp(BinaryOp::And, NodePtr($1), NodePtr($3));}

LOrExp : LAndExp { $$ = $1; }
    | LOrExp "||" LAndExp { $$ = new BinaryExp(BinaryOp::Or, NodePtr($1), NodePtr($3)); }
    ;
%%

void yyerror(const char *s) {
    std::cerr << "Error at line " << yylineno << ": " << s << std::endl;
}
