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
#include "llvm/Transforms/Utils.h"
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
class VarExprAST;
class BinaryExprAST;
class UnaryExprAST;
class CallExprAST;
class PrototypeAST;
class FunctionAST;
class IfExprAST;
class ForExprAST;
class GenerateCode;

class GenerateCode
{
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

class VarExprAST : public ExprAST
{
	public:
	std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> vars;
	std::unique_ptr<ExprAST> body;

	public:
	VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> vars, std::unique_ptr<ExprAST> body) : vars(std::move(vars)), body(std::move(body)) {}
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

class UnaryExprAST : public ExprAST
{
	public:
	char op;
	std::unique_ptr<ExprAST> operand;

	public:
	UnaryExprAST(char op, std::unique_ptr<ExprAST> operand) : op(op), operand(std::move(operand)) {}
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
	bool isOperator;
	unsigned precedence;

	public:
	PrototypeAST(const std::string &name, std::vector<std::unique_ptr<ExprAST>> args, std::vector<std::string> argString, bool isOperator = false, unsigned precedence = 0)
	: name(name), args(std::move(args)), argString(std::move(argString)), isOperator(isOperator), precedence(precedence) {}
	const std::string getName() const
	{
		return name;
	}
	Function* Codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->Codegen(this);
	}
	bool isUnaryOp()
	{
		return (isOperator && (args.size() == 1));
	}
	bool isBinaryOp()
	{
		return (isOperator && (args.size() == 2));
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

class IfExprAST : public ExprAST
{
	public:
	std::unique_ptr<ExprAST> cond, then, _else;

	public:
	IfExprAST(std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> then, std::unique_ptr<ExprAST> _else) : cond(std::move(cond)), then(std::move(then)), _else(std::move(_else)) {}
	Value* codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->codegen(this);
	}
};

class ForExprAST : public ExprAST
{
	public:
	std::string varName;
	std::unique_ptr<ExprAST> start, cond, step, body;

	public:
	ForExprAST(const std::string &varName, std::unique_ptr<ExprAST> start, std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> step, std::unique_ptr<ExprAST> body)
	: varName(varName), start(std::move(start)), cond(std::move(cond)), step(std::move(step)), body(std::move(body)) {}
	Value* codegen(GenerateCode* codeGenerator) override
	{
		return codeGenerator->codegen(this);
	}
};
