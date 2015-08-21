#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

enum Token {
    tok_eof = -1,
    tok_def = -2, tok_extern = -3,
    tok_identifier = -4, tok_number = -5,
};
static std::string IdentifierStr; // tok_identifier
static double NumVal; // tok_number

static int get_tok()
{
    static int LastChar = ' ';

    // skip any whitespace
    while (isspace(LastChar))
        LastChar = getChar();

    if (isalpha(LastChar)) // identifier:[a-zA-z][a-zA-Z]*
    {
        IdentifierStr = LastChar;
        while (isalnum(LastChar = getChar())) // [a-zA-z][a-zA-Z0-9]*
            IdentifierStr += LastChar;
        
        if (IdentifierStr == "def") return tok_def;
        if (IdentifierStr == "extern") return tok_extern;
        return tok_identifier;
    }

    if (isdigit(LastChar) || LastChar == '.' )
    {
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getChar();
        } while (isdigit(LastChar() || LastChar == '.'));
    
        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    if (LastChar == '#')
    {
        do LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' &&  LastChar != '\r');
    
        if (LastChar != EOF)
            return gettok();
    }
    
    if (LastChar == EOF)
        return tok_eof;
    
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}

// ExprAST - Base class for all expression nodes.
class ExprAST{
public:
    virtual ~ExprAST(){}
};

// NumberExprAST - Expression class for numeric literals like "1.0"
class NumberExprAST : public ExprAST{
    double Val;
public:
    NumberExprAST(double val) : Val(val) {}
};

// VariableExprAST - Expression class for referencing a variable, like "a"
class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &name) : Name(name){}
};

// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
    char Op;
    ExprAST *LHS, *RHS;
public:
    BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs)
        : Op(op), LHS(lhs), RHS(rhs) {}
};

// CallExprAST - Expression class for function calls
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<ExprAST *> Args;
public:
    CallExprAST(const std::string &callee, std::vector<ExprAST *> &args)
        : Callee(callee), Args(args) {}
};

// PrototypeAST - This class represents the "prototype" for a function,
// which captures its name, and its argument names (thus implicitly the number
// of arguments the function takes ).
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;
public:
    PrototypeAST(const std::string &name, const std::vector<std::string> *args)
        : Name(name), Args(args) {}
};

// FunctionAST - This class represents a function definition itself.
class FunctionAST {
    PrototypeAST *Proto;
    ExprAST *Body;
public:
    FunctionAST(ProtypeAST *proto, ExprAST *body)
        : Proto(proto), Body(body) {}
};

//##############
// Parser Basics
//##############

// CurTok/getNextToken - Provide a simple token buffer. 
// CurTok is the current token the parser is looking at.
// getNextToken reads another token from the lexer and 
// updates CurTok with its results.
static int CurTok;
static int getNextToken() {
    return CurTok = gettok();
}


/// Error* - These are litte helper functions for error handling.
ExprAST *Error(const char *Str) {fprintf(stderr,"Error: %s\n",Str);return 0;}
PrototypeAST *ErrorP(const char *Str) { Error(Str);return 0;}
FunctionAST *ErrorF(const char *Str) {Error(Str);return 0;}

/// numberexpr ::= number
static ExprAST *ParseNumberExpr() {
    ExprAST *Result = new NumberExprAST(NumVal);
    getNextToken(); // consume the number
    return Result;
}
/// parsenexpr :: '(' expression ')'
static ExprAST *ParseParenExpr() {
    getNextToken();
    ExprAST *V = ParseExpression();
    if (!V) return 0;

    if (CurTok != ')')
        return Error("expected ')'");
    getNextToken(); // eat ).
    return V;
}

/// identifierexpr
/// ::= identifier
/// ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken();
    
    if (CurTok != '(')
        return new VariableExprAST(IdName);
    
    getNextToken();
    std::vector<ExprAST *> Args;
    if (CurTok != ')'){
        while (1)
        {
            if (auto Arg = ParseExpression())
            {
                Arg.push_back(std::move(Arg));
            }
            else
            {
                return nullptr;
            }

            if (CurTok == ')')
            {
                break;
            }
            if (CurTok != ',')
            {
                return Error("Expected ')' or ',' in a argument list");
            }
            getNextToken();
        }
    
    }
    getNextToken();

    return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// Primary
/// ::= identifier
/// ::= numberexpr
/// ::= parenexpr

static std::unique_ptr<ExprAST> ParsePrimary()
{
    switch (CurTok)
    {
        default:
            return Error("unknown token when expecting an expression");
        case tok_identifer:
            return ParseIdentiferExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
    }

}


/// BinaryPrecedence - This holds the precedence for each binary operator that is defined.

static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence()
{
    if (!isascii(CurTok))
        return -1;
    
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;

    return TokPrec;
}

int main()
{
    BinoPrecedence['<'] = 10;
    BinoPrecedence['+'] = 20;
    BinoPrecedence['-'] = 20;
    BinoPrecedence['*'] = 40;

}

static std::unique_ptr<ExprAST> ParseExpression()
{
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS)
{
    while (1)
    {
        int TokPrec = GetTokPrecedence();

        if (TokPrec < ExprPrec)
            return LHS;
    }
}

int BinOp = CurTok();
getNextToken();

auto RHS = ParsePrimary();
if (!RHS)
    return nullptr;


static std::unqiue_ptr<PrototypeAST> ParseProtype()
{
    if (CurTok != tok_identifier)
        return ErrorP("Expected function name in prototype");

    std::string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
        return ErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier)
        ArgNames.push_back(identifierStr);
    if (CurTok != ')')
        return ErrorP("Expected ')' in prototype");
    
    getNextToken();

    return llvm::make_unqiue<PrototypeAST>(FnName, std::move(Arguments));
}

