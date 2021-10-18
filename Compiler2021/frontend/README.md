# 编译器前端

## 一、词法分析

### 1. 终结符Token名对应表

| 序号 | 名称       | 内容       |
| ---- | ---------- | ---------- |
| 1    | IDENFR     | 标识符     |
| 2    | INTCON     | 数值常量   |
| 3    | STRCON     | 字符串常量 |
| 4    | CONSTTK    | const      |
| 5    | INTTK      | int        |
| 6    | VOIDTK     | void       |
| 7    | IFTK       | if         |
| 8    | ELSETK     | else       |
| 9    | WHILETK    | while      |
| 10   | BREAKTK    | break      |
| 11   | CONTINUETK | continue   |
| 12   | RETURNTK   | return     |
| 13   | PLUS       | +          |
| 14   | MINU       | -          |
| 15   | MULT       | *          |
| 16   | DIV        | /          |
| 17   | MOD        | %          |
| 18   | LSS        | <          |
| 19   | LEQ        | <=         |
| 20   | GRE        | \>          |
| 21   | GEQ        | \>=         |
| 22   | EQL        | ==         |
| 23   | NEQ        | !=         |
| 24   | AND        | &&         |
| 25   | OR         | \|\|       |
| 26   | NOT        | !          |
| 27   | ASSIGN     | =          |
| 28   | SEMICN     | ;          |
| 29   | COMMA      | ,          |
| 30   | LPARENT    | (          |
| 31   | RPARENT    | )          |
| 32   | LBRACK     | [          |
| 33   | RBRACK     | ]          |
| 34   | LBRACE     | {          |
| 35   | RBRACE     | }          |
| 36   | EOFTK     | 程序结束          |


### 2. 词法分析说明

1. 数值常量包含三种类型：八进制、十进制、十六进制
2. 字符串常量不包含双引号



## 二、语法分析

语法分析采用递归下降法

### 1. 文法定义改良版本：

------------------------------------------------------------------------------------------

- 常量变量函数声明（及初始化）

CompUnit ::= (Decl | FuncDef) {Decl | FuncDef}

 Decl ::= ConstDecl | VarDecl

ConstDecl ::= **'const'** **'int'** ConstDef { ',' ConstDef } ';'

ConstDef ::= **Ident** { '[' ConstExp ']' } '=' ConstInitVal

ConstInitVal ::= ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'

VarDecl ::= **'int'** VarDef { ',' VarDef } ';'

VarDef ::= **Ident** { '[' ConstExp ']' }  [ '=' InitVal ]

InitVal ::= Exp | '{' [ InitVal { ',' InitVal } ] '}' 

FuncDef ::= FuncType **Ident** '(' [FuncFParams] ')' Block 

FuncType ::= **'void'** |  **'int'** 

FuncFParams ::= FuncFParam { ',' FuncFParam } 

FuncFParam ::= **'int'** **Ident** [ '[' ']' { '[' Exp ']' } ] 

Block ::= '{' { BlockItem } '}' 

BlockItem ::= Decl | Stmt 



- 语句 or 表达式

Stmt ::= LVal '=' Exp ';' | [Exp] ';' | Block | **'if'** '( Cond ')' Stmt [ **'else'** Stmt ] | **'while'** '(' Cond ')' Stmt | **'break'** ';' | **'continue'** ';' | **'return'** [Exp] ';'

ConstExp ::= AddExp 	*注：使用的 Ident 必须是常量*

Exp ::= AddExp 	*注：SysY 表达式是 int 型表达式*



Cond ::= LOrExp 

LOrExp ::= LAndExp { '||' LAndExp }

LAndExp ::= EqExp { '&&' EqExp }

EqExp ::= RelExp { ('==' | '!=') RelExp }

RelExp ::= AddExp { ('<' | '>' | '<=' | '>=') AddExp }



AddExp ::= MulExp { ('+' | '−') MulExp }

MulExp ::= UnaryExp { ('*' | '/' | '%') UnaryExp }

UnaryExp ::= PrimaryExp | **Ident** '(' [FuncRParams] ')' | UnaryOp UnaryExp 

PrimaryExp ::= '(' Exp ')' | LVal | **IntConst** 

LVal ::= **Ident** {'[' Exp ']'} 

FuncRParams ::= Exp { ',' Exp } 

UnaryOp ::= '+' | '−' | '!' 	*注：'!'仅出现在条件表达式中*

---------------------------------------------

## 三、代码生成

- [x] 多维数组、数组初始化、函数调用的HIR示例
  - 多维数组构造
  - 数组初始化
- [x] 构造符号栈、当前Func、当前Block、划分基本块
- [x] 复杂条件表达式的实现
- [ ] 常量折叠
  - [x] 一般常量运算折叠
  - [x] 常量数组折叠
  - [ ] 跳转条件简化（当条件为常量时）
  - [ ] 函数实参常量折叠（待研究）
- [x] 全局变量初始化
- [x] break的时候要Jump到while循环后的block
  - [x] 在while语句时就先把头尾Block都定义好

