#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include "../include/lexExtern.h"

using namespace llvm;

// extern LLVMContext theContext;
// extern IRBuilder<> Builder(theContext);
// extern std::unique_ptr<Module> theModule;
// extern std::map<std::string, Value *> namedValues;

class ExprAST;
class NumberExprAST;
class VariableExprAST;
class BinaryExprAST;
class CallExprAST;
class PrototypeAST;
class FunctionAST;
class ExprAST;
class GenerateCode;

class GenerateCode
{
	public:
	Value* codegen(NumberExprAST*);
	Value* codegen(VariableExprAST*);
	Value* codegen(ExprAST*);
	Value* codegen(BinaryExprAST*);
	Value* codegen(CallExprAST*);
	Function* Codegen(FunctionAST*);
	Function* Codegen(PrototypeAST*);
};

class ExprAST
{
	public:
	virtual ~ExprAST() = default;
	virtual Value* codegen(GenerateCode* codeGenerator) {}
	virtual Function* Codegen(GenerateCode* codeGenerator) {}
};

class NumberExprAST : public ExprAST
{
	public:
	double val;

	public:
	NumberExprAST(double val) : val(val) {}
	Value* codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->codegen(this);
	}
};

class VariableExprAST : public ExprAST
{
	public:
	std::string name;

	public:
	VariableExprAST(const std::string &name) : name(name) {}
	Value* codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->codegen(this);
	}
};

class BinaryExprAST : public ExprAST
{
	public:
	char op;
	std::unique_ptr<ExprAST> LHS, RHS;

	public:
	BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS) : op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
	Value* codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->codegen(this);
	}
};

class CallExprAST : public ExprAST
{
	public:
	std::string callee;
	std::vector<std::unique_ptr<ExprAST>> args;

	public:
	CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args) : callee(callee), args(std::move(args)) {}
	Value* codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->codegen(this);
	}
};

class PrototypeAST : public ExprAST
{
	public:
	std::string name;
	std::vector<std::unique_ptr<ExprAST>> args;
	std::vector<std::string> argString;

	public:
	PrototypeAST(const std::string &name, std::vector<std::unique_ptr<ExprAST>> args, std::vector<std::string> argString) : name(name), args(std::move(args)), argString(std::move(argString)) {}
	const std::string getName() const
	{
		return name;
	}
	Function* Codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->Codegen(this);
	}
};

class FunctionAST : public ExprAST
{
	public:
	std::unique_ptr<PrototypeAST> proto;
	std::unique_ptr<ExprAST> body;

	public:
	FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body) : proto(std::move(proto)), body(std::move(body)) {}
	Function* Codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->Codegen(this);
	}
};