#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>


// Lexer
//#######################
enum Token
{
    tok_eof = -1;

    tok_def = -2;
    tok_extern = -3;

    tok_identifier = -4;
    tok_number = -5;

};

static std::string IdentifierStr; // tok_identifier
static double NumVal;   // tok_number


// return the next token from STDIN
static int gettok()
{
    static int LastChar = ' ';
    while (isspace(LastChar))
        LastChar = getchar();

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
    virtual Value *Codegen() = 0;
};    

class NumberExprAST : public ExprAST{
    double Val;

public:
    NumberExprAST(double Val): Val(Val){}
    Value *Codegen() override;
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &Name): Name(Name){}
    Value *Codegen() override;
};

class BinaryExprAST : public ExprAST{
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
        : Op(Op), LHS(std::move(LHS), RHS(std::move(RHS))){}
    Value *Codegen() override;
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    CallExprAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}
    Value *Codegen() override;

};

class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
        : Name(Name), Args(std::move(Args)) {}
    Function *Codegen();
};

class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto), Body(std::move(Body)))
    Function *Codegen();
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

std::unique_ptr<PrototypeAST> ErrorP(const char *Str){
    Error(Str);
    return nullptr;
}

std::unique_ptr<FunctionAST> ErrorF(const char *Str){
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
    std::string IdName = IdentifierStr;
    
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

// binoprhs
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

static std::unqiue_ptr<ExprAST> ParseExpression() 
{
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;
    
    return ParseBinOpRHS(0, std::move(LHS));
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
        return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr()
{
    if (auto E = ParseExpression()){
        auto Proto =
            llvm::make_unique<PrototypeAST>("",std::vector<std::string>());
        return llvm::make_unique<FunctionAST>(std::move(Proto),std::move(E));
    }
    return nullptr;
}

static std::unique_ptr<PrototypAST> ParseExtern()
{
    getNextToken();
    return ParsePrototype();
}

//=========================
// Quick and dirty hack
//=========================
std::string GenerateUniqueName(const char *root)
{
    static int i=0;
    char s[16];
    sprintf(s, "%s%d", root, i++);
    std::string S=s;
    return S;
}

std::string MakeLegalFunctionName(std::string Name)
{
    std::string NewName;
    if (!Name.length())
        return GenerateUniqueName("auto_func_");

    NewName = Name;

    if (NewName.find_first_of("0123456789") == 0)
    {
        NewName.insert(0, 1, 'n');
    }

    std::string legal_elements = 
        "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t pos;
    while ((pos = NewName.find_first_not_of(legal_elements)) !=
            std::string::npos){
        char old_c = NewName.at(pos);
        char new_str[16];
        sprintf(new_str, "%d", (int)old_c);
        NewName = NewName.replace(pos, 1, new_str)
    }
    return NewName;
}

//=====================
// MCJIT helper class
// ====================
class MCJITHelper {
public:
    MCJITHelper(LLVMContext &C) : Context(C), Open



}



//======================================================
// Code Generation
//=====================================================
Value *ErrorV(const char *Str)
{
    Error(Str);
    return nullptr;
}
static Module *TheModule;
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, Value *> NamedValues;

Value *NumberExprAST::Codegen()
{
    return ConstantFP::get(getGlobalContext(), APFloat(Val));
}

Value *VariableExprAST()::Codegen()
{
    Value *V = NamedValues[Name];
    if (!V)
        return ErrorV("Unkown variable name");
    return V;
}

Value *BinaryExprAST::Codegen() 
{
    Value *L = LHS->Codegen();
    Value *R = RHS->Codegen();
    if (!L||!R)
        return nullptr;
    switch (Op){
    case '+':
        return Builder.CreateFAdd(L, R, "addtmp");
    case '-':
        return Builder.CreateFSub(L, R, "subtmp");
    case '*':
        return Builder.CreateFMul(L, R, "multmp");
    case '<':
        L = Builder.CreateFCmpULT(L, R, "cmptmp");
        return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()),
                                    "booltmp");
    default:
        return ErrorV("invalid binary operator");
    }
}

Value *CallExprAST::Codegen() {
    Function *CalleeF = TheModule->getFunction(Callee);
    if (!CalleeF)
        return ErrorV("unkown function referenced");
    
    if (CalleeF->arg_size() != Args.size())
        return ErrorV("Incorrect # argmuments passed");

    std::vector<Value *> ArgsV;
    for (unsigned i=0, e = Args.size(); i != e; i++ ){
        ArgsV.push_back(Args[i]->Codegen());
        if (!ArgsV.back())
            return nullptr;
    }
    return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::Codegen()
{
    std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(getGlobalContext()));
    
    FunctionType *FT = 
        FunctionType::get(Type::getDoubleTy(getGlobalContext()), Doubles, false)
    Function *F =
        Function::Create(FT, Function::ExternalLinkage, Name, TheModule);
    
    if (F->getName() != Name){
        F->eraseFromParent();
        F = TheModule->getFunction(Name);

        if (!F->empty()){
            ErrorF("redefinition of function");
            return nullptr;
        }

        if (F->arg_size() != Args.size()){
            ErrorF(redefinition of function with different # args);
            return nullptr;
        }
    }
    
    unsigned Idx = 0;
    for (Function::arg_iterator AI = F->arg_begin(); Idx != Args.size(); AI++, Idx++)
    {
        AI->setName(Args[Idx]);
        NamedValues[Args[Idx]] = AI;
    }

    return F;
}

Function *FunctionAST::Codegen() {
    NamedValues.clear();
    
    Function *TheFunction = Proto->Codegen();
    if (!TheFunction)
        return nullptr;

    BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    if (Value *RetVal = Body->Codegen()){
        Builder.CreateRet(RetVal);
        verifyFunction(*TheFunction);
        return TheFunction;
    }

    TheFunction->eraseFromParent();
    return nullptr;
}

//
//  Top-Level parsing and JIT Driver
//

    
static void HandleDefinition()
{
    if (auto FnAST = ParseDefinition())
    {
        if (auto *FnIR = FnAST->Codegen()){
            fprintf(stderr, "Parsed a function definition.\n");
            FnIR->dump();
        }
    }
    else
    {
        getNextToken();
    }
}
static void HandleExtern()
{
    if (auto ProtoAST = ParseExtern())
    {
        if (auto *FnIR = ProtoAST->Codegen()){
            fprintf(stderr,"Read extern: ");
            FnIR->dump();
        }
    }
    else
    {
        getNextToken();
    }

}
static void HandleTopLevelExpression()
{
     if (auto FnAST = ParseTopLevelExpression())
    {
        if (auot *FnAST = FnAST->Codegen() ){
            fprintf(stderr,"Read a top-level expr: ");
            FnIR->dump();
        }
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
                break;
            case tok_extern:
                HandleExern();
                break;
            default:
                HandleTopLevelExpression();
                break;
        }
    }

}

int main()
{
    LLVMContext &context = getGlobalContext();

    BinoPrecedence['<'] = 10;
    BinoPrecedence['+'] = 20;
    BinoPrecedence['-'] = 20;
    BinoPrecedence['*'] = 40;

    fprintf(stderr, "ready>");
    getNextToken();

    std::unique_ptr<Module> Owner = 
        llvm::make_unique<Module>("my cool jit", Context);
    TheModule = Owner.get();

    MainLoop();

    TheModule->dump();

    return 0;
}


