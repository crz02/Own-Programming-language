// =============================================================================
//  ZapLang — basic.h
//  Declarations: Tokens, Values, AST Nodes, Lexer, Parser, Interpreter
// =============================================================================
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>

// All AST nodes are reference-counted so functions can store their bodies.
struct ASTNode;
using NodePtr = std::shared_ptr<ASTNode>;

// =============================================================================
//  TOKEN TYPES
// =============================================================================
enum class TT {
    // Literals
    NUMBER, STRING, IDENT,
    // Arithmetic
    PLUS, MINUS, STAR, SLASH, PERCENT, CARET,
    // Comparison
    EQ, NEQ, LT, GT, LTE, GTE,
    // Assignment / grouping / separators
    ASSIGN, LPAREN, RPAREN, COMMA, NEWLINE,
    // Keywords
    KW_LET, KW_PRINT, KW_IF, KW_THEN, KW_ELSE,
    KW_END, KW_WHILE, KW_FUN, KW_CALL, KW_RETURN,
    KW_AND, KW_OR, KW_NOT,
    // End of source
    EOS
};

struct Token {
    TT          type;
    std::string val;
    int         line = 1;
};

// =============================================================================
//  RUNTIME VALUE
// =============================================================================
struct ZapVal {
    enum class Kind { Num, Str, Nil } kind = Kind::Nil;
    double      num = 0.0;
    std::string str;

    ZapVal() = default;
    explicit ZapVal(double n)      : kind(Kind::Num), num(n) {}
    explicit ZapVal(std::string s) : kind(Kind::Str), str(std::move(s)) {}

    bool isNum() const { return kind == Kind::Num; }
    bool isStr() const { return kind == Kind::Str; }
    bool isNil() const { return kind == Kind::Nil; }

    // Truthy: non-zero number or non-empty string
    bool truthy() const;

    // Human-readable representation (used by PRINT)
    std::string repr() const;
};

// =============================================================================
//  AST NODES
// =============================================================================
struct ASTNode { virtual ~ASTNode() = default; };

// Literals
struct NumNode   : ASTNode { double val; explicit NumNode(double v) : val(v) {} };
struct StrNode   : ASTNode { std::string val; explicit StrNode(std::string v) : val(std::move(v)) {} };

// Variable access
struct VarNode   : ASTNode { std::string name; explicit VarNode(std::string n) : name(std::move(n)) {} };

// LET x = expr
struct AssignNode : ASTNode {
    std::string name;
    NodePtr     expr;
    AssignNode(std::string n, NodePtr e) : name(std::move(n)), expr(std::move(e)) {}
};

// Binary operation: left op right
struct BinNode : ASTNode {
    NodePtr left;
    TT      op;
    NodePtr right;
    BinNode(NodePtr l, TT o, NodePtr r) : left(std::move(l)), op(o), right(std::move(r)) {}
};

// Unary operation: op expr  (-x, NOT x)
struct UnNode : ASTNode {
    TT      op;
    NodePtr expr;
    UnNode(TT o, NodePtr e) : op(o), expr(std::move(e)) {}
};

// PRINT expr
struct PrintNode : ASTNode {
    NodePtr expr;
    explicit PrintNode(NodePtr e) : expr(std::move(e)) {}
};

// IF cond [THEN] thenBody [ELSE elseBody] END
struct IfNode : ASTNode {
    NodePtr              cond;
    std::vector<NodePtr> thenB;
    std::vector<NodePtr> elseB;
    IfNode(NodePtr c, std::vector<NodePtr> t, std::vector<NodePtr> e)
        : cond(std::move(c)), thenB(std::move(t)), elseB(std::move(e)) {}
};

// WHILE cond ... END
struct WhileNode : ASTNode {
    NodePtr              cond;
    std::vector<NodePtr> body;
    WhileNode(NodePtr c, std::vector<NodePtr> b) : cond(std::move(c)), body(std::move(b)) {}
};

// FUN name(params) ... END
struct FunDefNode : ASTNode {
    std::string              name;
    std::vector<std::string> params;
    std::vector<NodePtr>     body;
    FunDefNode(std::string n, std::vector<std::string> p, std::vector<NodePtr> b)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
};

// CALL name(args)  or  name(args) inside an expression
struct CallNode : ASTNode {
    std::string          name;
    std::vector<NodePtr> args;
    CallNode(std::string n, std::vector<NodePtr> a) : name(std::move(n)), args(std::move(a)) {}
};

// RETURN [expr]
struct RetNode : ASTNode {
    NodePtr expr; // may be nullptr
    explicit RetNode(NodePtr e) : expr(std::move(e)) {}
};

// =============================================================================
//  LEXER
// =============================================================================
class Lexer {
public:
    explicit Lexer(std::string src) : src_(std::move(src)) {}
    std::vector<Token> tokenize();

private:
    std::string src_;
    int         pos_  = 0;
    int         line_ = 1;

    char cur()  const;
    char peek() const;
    void adv();
    void skipWS();

    Token numTok();
    Token strTok();
    Token identTok();
};

// =============================================================================
//  PARSER
// =============================================================================
class Parser {
public:
    explicit Parser(std::vector<Token> toks) : toks_(std::move(toks)) {}
    std::vector<NodePtr> parse();

private:
    std::vector<Token> toks_;
    int                pos_ = 0;

    Token& cur();
    Token& lookahead(int n = 1);
    Token  consume();
    Token  expect(TT t, const std::string& msg);
    void   skipNL();

    std::vector<NodePtr> block(std::initializer_list<TT> ends);

    NodePtr stmt();
    NodePtr printStmt();
    NodePtr letStmt();
    NodePtr ifStmt();
    NodePtr whileStmt();
    NodePtr funStmt();
    NodePtr callStmt();
    NodePtr retStmt();

    // Expression precedence (lowest → highest)
    NodePtr expr();
    NodePtr orExpr();
    NodePtr andExpr();
    NodePtr notExpr();
    NodePtr cmpExpr();
    NodePtr addExpr();
    NodePtr mulExpr();
    NodePtr powExpr();
    NodePtr unaryExpr();
    NodePtr primary();
};

// =============================================================================
//  INTERPRETER
// =============================================================================

// Stored function (params + body AST)
struct ZapFn {
    std::vector<std::string> params;
    std::vector<NodePtr>     body;
};

// Exception used to unwind the call stack on RETURN
struct ReturnSignal {
    ZapVal val;
    explicit ReturnSignal(ZapVal v) : val(std::move(v)) {}
};

class Interpreter {
public:
    Interpreter();

    // Execute a list of statements (top-level or block)
    void exec(const std::vector<NodePtr>& stmts);

    // Clear all variables and function definitions
    void reset();

private:
    // Scope stack (innermost is last)
    std::vector<std::unordered_map<std::string, ZapVal>> scopes_;
    // Function table (global)
    std::unordered_map<std::string, ZapFn> functions_;

    void    pushScope();
    void    popScope();
    ZapVal& getVar(const std::string& name);
    void    setVar(const std::string& name, ZapVal val);

    ZapVal eval(const NodePtr& node);
    ZapVal evalBin(const BinNode* n);
    void   execBlock(const std::vector<NodePtr>& stmts);
};
