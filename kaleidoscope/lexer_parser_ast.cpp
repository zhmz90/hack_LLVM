#include "llvm/ADT/STLExtras.h"
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>


enum Token
{
    tok_eof = -1;

    tok_def = -2;
    tok_extern = -3;

    tok_identifier = -4;
    tok_number = -5;

};

static std::string IdentifierStr;
static double NumVal;

static int getTok()
{
    static int LastChar = ' ';
    while (isspace(LastChar))
        LastChar = getChar();

    if (isalpha(LastChar))
    {
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getChar())))
            IdentifierStr += LastChar;

        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;

        return tok_identifier;
    }

    if (isdigit(LastChar) || LastChar == '.')
    {
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getChar();
        } while (isdigit(LastChar) || LastChar == '.');
    
        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    if (LastChar == '#')
    {
        do
            LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
    
        if (LastChar != EOF)
            return gettok();
    }
    
    if (LastChar == EOF)
        return tok_eof;

    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}

// AST aka Parse tree

namespace {
class ExprAST {
public:
    virtual ~ExprAST() {}
}    
};

class NumberExprAST : public ExprAST{
    double Val;
public:
    NumberExprAST(double Val): Val(Val){}
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &Name): Name(Name){}

};

class BinaryExprAST : public ExprAST{
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
        : Op(Op), LHS(std::move(LHS), RHS(std::move(RHS))){}
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    CallExprAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}

};

class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
        : Name(Name), Args(std::move(Args)) {}

};

class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto), Body(std::move(Body)))
};

// Parser
// a simple  token buffer
static int CurTok;
static int getNextToken(){ return CurTok = gettok();}

static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence()
{
    if (!isascii(CurTok))
        return -1;
    
    int TokPrec = BinoPrecedence[CurTok];
    if (TokPrec <= 0)
        return -1;
    return TokPrec;
}

std::unique_ptr<ExprAST> Error(const char *Str){
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<ExprAST> ErrorP(const char *Str){
    Error(Str);
    return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();

static std::unique_ptr<ExprAST> ParseNumberExpr()
{
    auto Result = llvm::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return std::move(Result);
}

static std::unique_ptr<ExprAST> ParseParenExpr()
{
    getNextToken();
    auto V = ParseExpression();
    if (!V)
        return nullptr;

    if (CurTok != ')')
        return Error("expected ')'");

    getNextToken();
    return V;
}

static std::unique_ptr<ExprAST> ParseIdentifierExpr()
{
    std::string idName = IdentifierStr;
    
    getNextToken();

    if (CurTok != '(')
        return llvm::make_unique<VariableExprAST>(IdName);

    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Args;

    if (CurTok != ')')
    {
        while (1)
        {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else
                return nullptr;
            if (CurTok == ')')
                break;
            if (CurTok != ',')
                return Error("Expected ')' or ',' in argument list");
            getNextToken();
        }
    
    }
    getNextToken();
    return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}

static std::unique_ptr<ExprAST> ParsePrimary()
{
    switch (CurTok)
    {
        default:
            return Error("unknown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParentExpr();
    }

}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, 
                                std::unique_ptr<ExprAST> LHS)
{
    while (1)
    {
        int TokPrec = GetTokPrecedence();
        
        if (TokPrec < ExprPrec)
            return LHS;

        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParsePrimary();
        if (!RHS)
           return nullptr;

        if NextPrec =  GetTokPrecedence();        
        if (TokPrec < NextPrec)
        {
            RHS = ParseBinOpRHS(TokPrec+1,std::move(RHS));
            if(!RHS)
                return nullptr;
        }
        LHS = llvm::make_unique_ptr<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }

}

static std::unique_ptr<PrototypeAST> ParsePrototype()
{
    if (CurTok != tok_identifier)
        return ErrorP("Expected function name in prototype");

    std::string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
        return ErrorP("Expected '(' in prototype");
    
    std::vector<std::sting> ArgNames;
    while (getNextToken() == tok_identifer)
        ArgNames.push_back(IdentifierStr);
    if (CurTok != ')')
        return ErrorP("Expected ')' in prototype");

    getNextToken();
    return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

static std::unique_ptr<FunctionAST> ParseDefinition()
{
    getNextToken();
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;
    if (auto E = ParseExpression())
        return llvm::make_unique<FunctionAST>(std::move(Proto, std::move(E)));
}

static std::unique_ptr<PrototypAST> ParseExtern()
{
    getNextToken();
    return ParsePrototype();
}

// Top-Level Parsing

static void HandleDefinition()
{
    if (ParseDefinition())
    {
        fprintf(stderr, "Parsed a function definition.\n");
    }
    else
    {
        getNextToken();
    }
}
static void HandleExtern()
{
    if (ParseExtern())
    {
        fprintf(stderr,"Parsed a extern\n");
    }
    else
    {
        getNextToken();
    }

}
static void HandleTopLevelExpression()
{
     if (ParseTopLevelExpression())
    {
        fprintf(stderr,"Parsed a top-level expr\n");
    }
    else
    {
        getNextToken();
    }
   
}


static void MainLoop()
{
    while (1)
    {
        fprintf(stderr,"ready>");
        switch (CurTok)
        {
            case tok_eof:
                return;
            case ";":
                getNextToken();
                break;
            case tok_def:
                HandleDefinition();
            case tok_extern:
                HandleExern();
            default:
                HandleTopLevelExpression();
                break;
        }
    }

}

int main()
{
    BinoPrecedence['<'] = 10;
    BinoPrecedence['+'] = 20;
    BinoPrecedence['-'] = 20;
    BinoPrecedence['*'] = 40;

    fprintf(stderr, "ready>");
    MainLoop();

    return 0;
}
