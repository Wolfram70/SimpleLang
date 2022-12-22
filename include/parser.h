#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../include/lexExtern.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"

using namespace llvm;

// extern LLVMContext theContext;
// extern IRBuilder<> Builder(theContext);
// extern std::unique_ptr<Module> theModule;
// extern std::map<std::string, Value *> namedValues;

class ExprAST;
class NumberExprAST;
class VariableExprAST;
class VarExprAST;
class BinaryExprAST;
class UnaryExprAST;
class CallExprAST;
class PrototypeAST;
class FunctionAST;
class IfExprAST;
class ForExprAST;
class GenerateCode;

static raw_ostream& indent(raw_ostream& O, int size) {
  return O << std::string(size, ' ');
}

class GenerateCode {
 public:
  Value* codegen(NumberExprAST*);
  Value* codegen(VariableExprAST*);
  Value* codegen(VarExprAST*);
  Value* codegen(ExprAST*);
  Value* codegen(BinaryExprAST*);
  Value* codegen(UnaryExprAST*);
  Value* codegen(CallExprAST*);
  Value* codegen(IfExprAST*);
  Value* codegen(ForExprAST*);
  Function* Codegen(FunctionAST*);
  Function* Codegen(PrototypeAST*);
};

class ExprAST {
 public:
  SourceLocation loc;

 public:
  ExprAST(SourceLocation loc = curLoc) : loc(loc) {}
  virtual ~ExprAST() = default;
  virtual Value* codegen(GenerateCode* codeGenerator) {}
  virtual Function* Codegen(GenerateCode* codeGenerator) {}
  int getLine() const { return loc.line; }
  int getCol() const { return loc.col; }
  virtual raw_ostream& dump(raw_ostream& out, int ind) {
    return out << ':' << getLine() << ':' << getCol() << '\n';
  }
};

class NumberExprAST : public ExprAST {
 public:
  double val;

 public:
  NumberExprAST(double val) : val(val) {}
  Value* codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    return ExprAST::dump(out << val, ind);
  }
};

class VariableExprAST : public ExprAST {
 public:
  std::string name;

 public:
  VariableExprAST(SourceLocation loc, const std::string& name)
      : ExprAST(loc), name(name) {}
  Value* codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    return ExprAST::dump(out << name, ind);
  }
};

class VarExprAST : public ExprAST {
 public:
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> vars;
  std::unique_ptr<ExprAST> body;

 public:
  VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> vars,
             std::unique_ptr<ExprAST> body)
      : vars(std::move(vars)), body(std::move(body)) {}
  Value* codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    ExprAST::dump(out << "var", ind);
    for (const auto& namedVar : vars) {
      namedVar.second->dump(indent(out, ind) << namedVar.first << ':', ind + 1);
    }
    body->dump(indent(out, ind) << "Body:", ind + 1);
    return out;
  }
};

class BinaryExprAST : public ExprAST {
 public:
  char op;
  std::unique_ptr<ExprAST> LHS, RHS;

 public:
  BinaryExprAST(SourceLocation loc, char op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : ExprAST(loc), op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  Value* codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    ExprAST::dump(out << "binary" << op, ind);
    LHS->dump(indent(out, ind) << "LHS:", ind + 1);
    RHS->dump(indent(out, ind) << "RHS:", ind + 1);
    return out;
  }
};

class UnaryExprAST : public ExprAST {
 public:
  char op;
  std::unique_ptr<ExprAST> operand;

 public:
  UnaryExprAST(char op, std::unique_ptr<ExprAST> operand)
      : op(op), operand(std::move(operand)) {}
  Value* codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    ExprAST::dump(out << "unary" << op, ind);
    operand->dump(out, ind + 1);
    return out;
  }
};

class CallExprAST : public ExprAST {
 public:
  std::string callee;
  std::vector<std::unique_ptr<ExprAST>> args;

 public:
  CallExprAST(SourceLocation loc, const std::string& callee,
              std::vector<std::unique_ptr<ExprAST>> args)
      : ExprAST(loc), callee(callee), args(std::move(args)) {}
  Value* codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    ExprAST::dump(out << "call " << callee, ind + 1);
    for (const auto& arg : args) {
      arg->dump(indent(out, ind + 1), ind + 1);
    }
    return out;
  }
};

class PrototypeAST : public ExprAST {
 public:
  std::string name;
  std::vector<std::unique_ptr<ExprAST>> args;
  std::vector<std::string> argString;
  bool isOperator;
  unsigned precedence;
  int line;

 public:
  PrototypeAST(SourceLocation loc, const std::string& name,
               std::vector<std::unique_ptr<ExprAST>> args,
               std::vector<std::string> argString, bool isOperator = false,
               unsigned precedence = 0)
      : name(name),
        args(std::move(args)),
        argString(std::move(argString)),
        isOperator(isOperator),
        precedence(precedence),
        line(loc.line) {}
  const std::string getName() const { return name; }
  Function* Codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->Codegen(this);
  }
  bool isUnaryOp() { return (isOperator && (args.size() == 1)); }
  bool isBinaryOp() { return (isOperator && (args.size() == 2)); }
  char getOperatorName() {
    assert(isUnaryOp() || isBinaryOp());
    return name[name.size() - 1];
  }
  int getLine() const { return line; }
};

class FunctionAST : public ExprAST {
 public:
  std::unique_ptr<PrototypeAST> proto;
  std::unique_ptr<ExprAST> body;

 public:
  FunctionAST(std::unique_ptr<PrototypeAST> proto,
              std::unique_ptr<ExprAST> body)
      : proto(std::move(proto)), body(std::move(body)) {}
  Function* Codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->Codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    indent(out, ind) << "Function:\n";
    ind++;
    indent(out, ind) << "Body:\n";
    return body ? body->dump(out, ind) : out << "null\n";
  }
};

class IfExprAST : public ExprAST {
 public:
  std::unique_ptr<ExprAST> cond, then, _else;

 public:
  IfExprAST(SourceLocation loc, std::unique_ptr<ExprAST> cond,
            std::unique_ptr<ExprAST> then, std::unique_ptr<ExprAST> _else)
      : ExprAST(loc),
        cond(std::move(cond)),
        then(std::move(then)),
        _else(std::move(_else)) {}
  Value* codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    ExprAST::dump(out << "if", ind);
    cond->dump(indent(out, ind) << "condition:", ind + 1);
    then->dump(indent(out, ind) << "then:", ind + 1);
    _else->dump(indent(out, ind) << "else:", ind + 1);
    return out;
  }
};

class ForExprAST : public ExprAST {
 public:
  std::string varName;
  std::unique_ptr<ExprAST> start, cond, step, body;

 public:
  ForExprAST(const std::string& varName, std::unique_ptr<ExprAST> start,
             std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> step,
             std::unique_ptr<ExprAST> body)
      : varName(varName),
        start(std::move(start)),
        cond(std::move(cond)),
        step(std::move(step)),
        body(std::move(body)) {}
  Value* codegen(GenerateCode* codeGenerator) override {
    return codeGenerator->codegen(this);
  }
  raw_ostream& dump(raw_ostream& out, int ind) override {
    ExprAST::dump(out << "for", ind);
    start->dump(indent(out, ind) << "initialization:", ind + 1);
    cond->dump(indent(out, ind) << "condition:", ind + 1);
    step->dump(indent(out, ind) << "increment:", ind + 1);
    body->dump(indent(out, ind) << "body:", ind + 1);
    return out;
  }
};
