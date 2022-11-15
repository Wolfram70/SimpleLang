#include <cstdio>

#include "../include/parser.h"

extern int getToken();

char curTok;

std::map<char, int> binOpPrecedence;

LLVMContext theContext;
IRBuilder<> Builder(theContext);
std::unique_ptr<Module> theModule;
std::map<std::string, Value *> namedValues;

std::unique_ptr<ExprAST> parseNumberExpr();
std::unique_ptr<ExprAST> parseParenExpr();
std::unique_ptr<ExprAST> parsePrimary();
std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS);
std::unique_ptr<ExprAST> parseExpression();
std::unique_ptr<ExprAST> parseIdentifierExpr();
std::unique_ptr<PrototypeAST> parseProtoype();
std::unique_ptr<FunctionAST> parseDefinition();
std::unique_ptr<PrototypeAST> parseExtern();
std::unique_ptr<FunctionAST> parseTopLvlExpr();

int getNextToken()
{
	return curTok = getToken();
}

std::unique_ptr<ExprAST> logError(const char *str)
{
	fprintf(stderr, "LogError: %s\n", str);
	return nullptr;
}

std::unique_ptr<PrototypeAST> logErrorP(const char *str)
{
	logError(str);
	return nullptr;
}

std::unique_ptr<ExprAST> parseNumberExpr()
{
	auto result = std::make_unique<NumberExprAST>(numVal);
	getNextToken();
	return std::move(result);
}

std::unique_ptr<ExprAST> parseParenExpr()
{
	getNextToken();
	auto V = std::move(parseExpression());
	if (!V)
	{
		return nullptr;
	}

	if (curTok != ')')
	{
		return logError("Expected ')'");
	}
	else
	{
		getNextToken();
		return std::move(V);
	}
}

std::unique_ptr<ExprAST> parsePrimary()
{
	switch (curTok)
	{
	case tok_identifier:
		return std::move(parseIdentifierExpr());
	case tok_number:
		return std::move(parseNumberExpr());
	case '(':
		return std::move(parseParenExpr());
	default:
		return logError("Unknown token when expecting an expression");
	}
}

int getTokPrecedence()
{
	if (!isascii(curTok))
	{
		return -1;
	}

	int tokPrec = binOpPrecedence[curTok];

	if (tokPrec <= 0)
	{
		return -1;
	}

	return tokPrec;
}

std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS)
{
	while (true)
	{
		int tokPrec = getTokPrecedence();

		if (tokPrec < exprPrec) // to check if this is really a binary operator as invalid tokens have precedence of -1
		{
			return std::move(LHS);
		}

		int binOp = curTok;
		getNextToken();

		auto RHS = parsePrimary();

		if (!RHS)
		{
			return nullptr;
		}

		int nextPrec = getTokPrecedence();

		if (tokPrec < nextPrec)
		{
			RHS = parseBinOpRHS(tokPrec + 1, std::move(RHS));

			if (!RHS)
			{
				return nullptr;
			}
		}

		LHS = std::make_unique<BinaryExprAST>(binOp, std::move(LHS), std::move(RHS));
	}
}

std::unique_ptr<ExprAST> parseExpression()
{
	auto LHS = std::move(parsePrimary());

	if (!LHS)
	{
		return nullptr;
	}

	return std::move(parseBinOpRHS(0, std::move(LHS)));
}

std::unique_ptr<ExprAST> parseIdentifierExpr()
{
	std::string idName = identifierStr;

	getNextToken();
	if (curTok != '(')
	{
		return std::make_unique<VariableExprAST>(idName);
	}
	getNextToken();

	std::vector<std::unique_ptr<ExprAST>> args;

	if (curTok != ')')
	{
		while (true)
		{
			if (auto arg = parseExpression())
			{
				args.push_back(std::move(arg));
			}
			else
			{
				return nullptr;
			}

			if (curTok == ')')
			{
				break;
			}
		}
	}

	getNextToken();

	return std::make_unique<CallExprAST>(idName, std::move(args));
}

std::unique_ptr<PrototypeAST> parseProtoype()
{
	if (curTok != tok_identifier)
	{
		return logErrorP("Expected function name in prototype");
	}

	std::string fnName = identifierStr;
	getNextToken();

	if (curTok != '(')
	{
		return logErrorP("Expected '(' in function prototype");
	}

	std::vector<std::unique_ptr<ExprAST>> argNames;

	while (getNextToken() == tok_identifier)
	{
		argNames.push_back(std::move(std::make_unique<VariableExprAST>(identifierStr)));
	}

	if (curTok != ')')
	{
		return logErrorP("Expected ')' in function prototype");
	}
	getNextToken();

	return std::make_unique<PrototypeAST>(fnName, std::move(argNames)); // fixed this????? does it work????
}

std::unique_ptr<FunctionAST> parseDefinition()
{
	getNextToken();

	auto proto = std::move(parseProtoype()); 
	if (!proto)
	{
		return nullptr;
	}

	if (auto exp = std::move(parseExpression()))
	{
		return std::make_unique<FunctionAST>(std::move(proto), std::move(exp));
	}

	return nullptr;
}

std::unique_ptr<PrototypeAST> parseExtern()
{
	getNextToken();

	return std::move(parseProtoype());
}

std::unique_ptr<FunctionAST> parseTopLvlExpr()
{
	if (auto exp = std::move(parseExpression()))
	{
		auto proto = std::make_unique<PrototypeAST>("", std::move(std::vector<std::unique_ptr<ExprAST>>()));
		return std::make_unique<FunctionAST>(std::move(proto), std::move(exp));
	}

	return nullptr;
}

Value* logErrorV(const char* str)
{
	logError(str);
	return nullptr;
}

Value* GenerateCode::codegen(NumberExprAST* a)
{
	return ConstantFP::get(theContext, APFloat(a->val));
}

Value* GenerateCode::codegen(VariableExprAST* a)
{
	Value* V = namedValues[a->name];
	if(!V)
	{
		logErrorV("Unknown variable name.");
		return nullptr;
	}
	return V;
}

Value* GenerateCode::codegen(BinaryExprAST* a)
{
	Value* L = a->LHS->codegen(this);
	Value* R = a->RHS->codegen(this);

	if(!L || !R)
	{
		return nullptr;
	}

	switch(a->op)
	{
		case '+':
			return Builder.CreateFAdd(L, R, "addtmp");
		case '-':
			return Builder.CreateFSub(L, R, "subtemp");
		case '*':
			return Builder.CreateFMul(L, R, "multemp");
		case '<':
			L = Builder.CreateFCmpULT(L, R, "cmptmp");
			return Builder.CreateUIToFP(L, Type::getDoubleTy(theContext), "booltmp");
		default:
			return logErrorV("Invalid binary operator");
	}
}

Value* GenerateCode::codegen(CallExprAST* a)
{
	Function *calleeF = theModule->getFunction(a->callee);
	if(!calleeF)
	{
		return logErrorV("Unknown function referenced");
	}

	if(calleeF->arg_size() != a->args.size())
	{
		return logErrorV("Incorrect number of arguments passed");
	}

	std::vector<Value*> argsV;
	for(unsigned i = 0, e = a->args.size(); i != e; i++)
	{
		argsV.push_back(a->args[i]->codegen(this));
		if(!argsV.back())
		{
			return nullptr;
		}
	}

	return Builder.CreateCall(calleeF, argsV, "calltmp");
}

Function* GenerateCode::Codegen(PrototypeAST* a)
{
	std::vector<Type*> doubles(a->args.size(), Type::getDoubleTy(theContext));
	
	FunctionType *FT = FunctionType::get(Type::getDoubleTy(theContext), doubles, false);

	Function *F = Function::Create(FT, Function::ExternalLinkage, a->name, theModule.get());

	unsigned idx = 0;
	for(auto &arg : F->args())
	{
		//Setting name of arguments to make IR more readable - may need to change the PrototypeAST (change std::unique_ptr<ExprAST> to std::string - but might break something else :((())))
		//arg.setName((std::string)a->args[idx++]);
	}

	return F;
}

Function* GenerateCode::Codegen(FunctionAST* a)
{
	Function *theFunction = theModule->getFunction(a->proto->getName());

	if(!theFunction)
	{
		theFunction = a->proto->Codegen(this);
	}

	if(!theFunction)
	{
		return nullptr;
	}

	if(!theFunction->empty())
	{
		return (Function*)logErrorV("Function cannot be redefined");
	}

	if(Value *retVal = a->body->codegen(this))
	{
		Builder.CreateRet(retVal);

		verifyFunction(*theFunction);

		return theFunction;
	}

	theFunction->eraseFromParent();
	return nullptr;
}

//Known bug: extern foo(a) followed by def foo(b) gives an error, the declaration using extern is given more precedence and is not validated properly


//Driver Code:: (Until I figure out how to share the llvm::Context with other files)