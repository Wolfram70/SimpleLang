#include <cstdio>

#include "../include/parser.h"
#include "../include/JIT.h"

extern int getToken();

char curTok;

std::map<char, int> binOpPrecedence;

std::unique_ptr<LLVMContext> theContext;
std::unique_ptr<IRBuilder<>> Builder;
std::unique_ptr<Module> theModule;
std::unique_ptr<legacy::FunctionPassManager> theFPM;
std::unique_ptr<llvm::orc::SimpleJIT> theJIT;
std::map<std::string, AllocaInst*> namedValues;
std::map<std::string, std::unique_ptr<PrototypeAST>> functionProtos;
ExitOnError exitOnErr;

GenerateCode codeGenerator;

int tempResolveTopLvlExpr = 0;

std::unique_ptr<ExprAST> parseNumberExpr();
std::unique_ptr<ExprAST> parseParenExpr();
std::unique_ptr<ExprAST> parsePrimary();
std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS);
std::unique_ptr<ExprAST> parseExpression();
std::unique_ptr<ExprAST> parseIdentifierExpr();
std::unique_ptr<PrototypeAST> parseProtoype();
std::unique_ptr<FunctionAST> parseDefinition();
std::unique_ptr<PrototypeAST> parseExtern();
std::unique_ptr<ExprAST> parseUnaryExpr();
std::unique_ptr<FunctionAST> parseTopLvlExpr();
std::unique_ptr<ExprAST> parseIfExpr();
std::unique_ptr<ExprAST> parseForExpr();
std::unique_ptr<ExprAST> parseVarExpr();

AllocaInst* createEntryBlockAlloca(Function* theFunction, std::string varName);

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
	case tok_if:
		return std::move(parseIfExpr());
	case tok_for:
		return std::move(parseForExpr());
	case tok_var:
		return std::move(parseVarExpr());
	default:
		return logError("Unknown token when expecting an expression");
	}
}

std::unique_ptr<ExprAST> parseUnaryExpr()
{
	if(!isascii(curTok) || curTok == '(')
	{
		return parsePrimary();
	}

	int opc = curTok;
	getNextToken();

	if(auto operand = parseUnaryExpr())
	{
		return std::make_unique<UnaryExprAST>(opc, std::move(operand));
	}
	return nullptr;
}

std::unique_ptr<ExprAST> parseForExpr()
{
	getNextToken();

	if(curTok != tok_identifier)
	{
		logError("Expected a variable name after for");
		return nullptr;
	}

	std::string idName = identifierStr;
	getNextToken();

	if(curTok != '=')
	{
		logError("Expected an '=' after for");
		return nullptr;
	}
	getNextToken();

	auto start = parseExpression();
	if(!start)
	{
		return nullptr;
	}

	if(curTok != tok_when)
	{
		logError("Expected when after for");
		return nullptr;
	}
	getNextToken();

	auto cond = parseExpression();
	if(!cond)
	{
		return nullptr;
	}

	std::unique_ptr<ExprAST> step;
	if(curTok == tok_inc)
	{
		getNextToken();
		step = parseExpression();
		if(!step)
		{
			return nullptr;
		}
	}

	if(curTok != tok_do)
	{
		logError("Expected do after for");
		return nullptr;
	}
	getNextToken();

	if(curTok != '(')
	{
		logError("Expected '(' after for ... ");
	}

	auto body = parseParenExpr();
	if(!body)
	{
		return nullptr;
	}

	return std::make_unique<ForExprAST>(idName, std::move(start), std::move(cond), std::move(step), std::move(body));
}

std::unique_ptr<ExprAST> parseVarExpr()
{
	getNextToken();

	std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> vars;

	if(curTok != tok_identifier)
	{ 
		return logError("Expected identifier after var");
	}

	while(true)
	{
		std::string name = identifierStr;
		getNextToken();

		std::unique_ptr<ExprAST> init;
		if(curTok == '=')
		{
			getNextToken();

			init = std::move(parseExpression());
			if(!init)
			{
				return nullptr;
			}
		}

		vars.push_back(std::make_pair(name, std::move(init)));

		if(curTok != ',')
		{
			break;
		}

		getNextToken();

		if(curTok != tok_identifier)
		{
			return logError("Expected identifier after var");
		}
	}

	if(curTok != tok_in)
	{
		return logError("Expected in after var");
	}
	getNextToken();

	auto body = parseExpression();
	if(!body)
	{
		return nullptr;
	}

	return std::make_unique<VarExprAST>(std::move(vars), std::move(body));
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

		auto RHS = parseUnaryExpr();

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
	auto LHS = std::move(parseUnaryExpr());

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

			if(curTok != ',')
			{
				return logError("Expected ',' after an argument in function call");
			}

			getNextToken();
		}
	}

	getNextToken();

	return std::make_unique<CallExprAST>(idName, std::move(args));
}

std::unique_ptr<ExprAST> parseIfExpr()
{
	getNextToken();

	auto cond = parseExpression();
	if(!cond)
	{
		return nullptr;
	}

	if(curTok != tok_then)
	{
		logError("Expected then");
		return nullptr;
	}
	getNextToken();

	if(curTok != '(')
	{
		logError("Expected '(' after then");
		return nullptr;
	}

	auto then = parseParenExpr();
	if(!then)
	{
		return nullptr;
	}

	if(curTok != tok_else)
	{
		logError("Expected else");
		return nullptr;
	}
	getNextToken();

	if(curTok != '(')
	{
		logError("Expected '(' after else");
		return nullptr;
	}

	auto _else = parseParenExpr();
	if(!_else)
	{
		return nullptr;
	}

	return std::make_unique<IfExprAST>(std::move(cond), std::move(then), std::move(_else));
}

std::unique_ptr<PrototypeAST> parseProtoype()
{
	std::string fnName;

	unsigned kind = 0;
	unsigned binaryPrecedence = 30;

	switch(curTok)
	{
		default:
			return logErrorP("Expected function name in prototype");
			break;
		case tok_identifier:
			fnName = identifierStr;
			kind = 0;
			getNextToken();
			break;
		case tok_binary:
			getNextToken();
			if(!isascii(curTok))
			{
				return logErrorP("Expected binary operator");
			}
			fnName = "binary";
			fnName.push_back(curTok);
			kind = 2;
			getNextToken();

			if(curTok == tok_number)
			{
				if(numVal < 1 || numVal > 100)
				{
					return logErrorP("Precedence must be from 1 to 100");
				}
				binaryPrecedence = (unsigned)numVal;
				getNextToken();
			}
			break;
		case tok_unary:
			getNextToken();
			if(!isascii(curTok))
			{
				return logErrorP("Expected unary operator");
			}
			fnName = "unary";
			fnName.push_back(curTok);
			kind = 1;
			getNextToken();
	}

	if (curTok != '(')
	{
		return logErrorP("Expected '(' in function prototype");
	}

	std::vector<std::unique_ptr<ExprAST>> argNames;
	std::vector<std::string> argString;

	int tok = getNextToken();

	while (tok == tok_identifier)
	{
		argNames.push_back(std::move(std::make_unique<VariableExprAST>(identifierStr)));
		argString.push_back(identifierStr);

		tok = getNextToken();
		if(tok != ')')
		{
			if(tok != ',')
			{
				return logErrorP("Expected comma after formal argument in function definition");
			}
			tok = getNextToken();
		}
	}

	if (curTok != ')')
	{
		return logErrorP("Expected ')' in function prototype");
	}
	getNextToken();

	if(kind && (argNames.size() != kind))
	{
		return logErrorP("Invalid number of operands for an operator");
	}

	return std::make_unique<PrototypeAST>(fnName, std::move(argNames), std::move(argString), kind != 0, binaryPrecedence);
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
		auto proto = std::make_unique<PrototypeAST>("__anon_expr", std::move(std::vector<std::unique_ptr<ExprAST>>()), std::move(std::vector<std::string>()));
		return std::make_unique<FunctionAST>(std::move(proto), std::move(exp));
	}

	return nullptr;
}

//CODE GENERATION:

AllocaInst* createEntryBlockAlloca(Function* theFunction, std::string varName)
{
	IRBuilder<> tmpB(&theFunction->getEntryBlock(), theFunction->getEntryBlock().begin());

	return tmpB.CreateAlloca(Type::getDoubleTy(*theContext), 0, varName);
}

Function* getFunction(std::string name)
{
	if(auto *F = theModule->getFunction(name))
	{
		return F;
	}

	auto FI = functionProtos.find(name);
	if(FI != functionProtos.end())
	{
		return FI->second->Codegen(&codeGenerator);
	}
	else
	{
		return nullptr;
	}
}

Value* logErrorV(const char* str)
{
	logError(str);
	return nullptr;
}

Value* GenerateCode::codegen(NumberExprAST* a)
{
	return ConstantFP::get(*theContext, APFloat(a->val));
}

Value* GenerateCode::codegen(VariableExprAST* a)
{
	AllocaInst* A = namedValues[a->name];
	if(!A)
	{
		logErrorV("Unknown variable name.");
		return nullptr;
	}
	return Builder->CreateLoad(A->getAllocatedType(), A, llvm::StringRef(a->name));
}

Value* GenerateCode::codegen(BinaryExprAST* a)
{
	if(a->op == ':')
	{
		Value* L = a->LHS->codegen(this);
		if(!L)
		{
			return nullptr;
		}

		return a->RHS->codegen(this);
	}
	if(a->op == '=')
	{
		VariableExprAST *LHSe = static_cast<VariableExprAST*>(a->LHS.get());
		if(!LHSe)
		{
			return logErrorV("LHS of '=' must be a variable");
		}

		Value *val = a->RHS->codegen(this);
		if(!val)
		{
			return nullptr;
		}

		AllocaInst* variable = namedValues[LHSe->name];

		if(!variable)
		{
			return logErrorV("Unknown variable name");
		}

		Builder->CreateStore(val, variable);

		return val;
	}

	Value* L = a->LHS->codegen(this);
	Value* R = a->RHS->codegen(this);

	if(!L || !R)
	{
		return nullptr;
	}

	switch(a->op)
	{
		case '+':
			return Builder->CreateFAdd(L, R, "addtmp");
		case '-':
			return Builder->CreateFSub(L, R, "subtemp");
		case '*':
			return Builder->CreateFMul(L, R, "multemp");
		case '<':
			L = Builder->CreateFCmpULT(L, R, "cmptmp");
			return Builder->CreateUIToFP(L, Type::getDoubleTy(*theContext), "booltmp");
		default:
			break;
	}

	Function *F = getFunction(std::string("binary") + a->op);
	if(!F)
	{
		return logErrorV("Binary operator not found");
	}

	Value *ops[2] = {L, R};
	return Builder->CreateCall(F, ops, "binop");
}

Value* GenerateCode::codegen(UnaryExprAST* a)
{
	Value* operandV = a->operand->codegen(this);
	if(!operandV)
	{
		return nullptr;
	}

	Function *F = getFunction(std::string("unary") + a->op);
	if(!F)
	{
		return logErrorV("Unknown unary operator");
	}
	
	return Builder->CreateCall(F, operandV, "unop");
}

Value* GenerateCode::codegen(CallExprAST* a)
{
	Function *calleeF = getFunction(a->callee);
	// Function *calleeF = theModule->getFunction(a->callee);
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

	return Builder->CreateCall(calleeF, argsV, "calltmp");
}

Value* GenerateCode::codegen(IfExprAST* a)
{
	Value *condV = a->cond->codegen(this);
	if(!condV)
	{
		return nullptr;
	}

	condV = Builder->CreateFCmpONE(condV, ConstantFP::get(*theContext, APFloat(0.0)), "ifcond");

	Function *theFunction = Builder->GetInsertBlock()->getParent();

	BasicBlock *thenBB = BasicBlock::Create(*theContext, "then", theFunction);
	BasicBlock *elseBB = BasicBlock::Create(*theContext, "else");
	BasicBlock *mergeBB = BasicBlock::Create(*theContext, "ifcont");

	Builder->CreateCondBr(condV, thenBB, elseBB);

	Builder->SetInsertPoint(thenBB);

	Value *thenV = a->then->codegen(this);
	if(!thenV)
	{
		return nullptr;
	}

	Builder->CreateBr(mergeBB);

	thenBB = Builder->GetInsertBlock();

	theFunction->getBasicBlockList().push_back(elseBB);
	Builder->SetInsertPoint(elseBB);

	Value *elseV = a->_else->codegen(this);
	if(!elseV)
	{
		return nullptr;
	}

	Builder->CreateBr(mergeBB);

	elseBB = Builder->GetInsertBlock();

	theFunction->getBasicBlockList().push_back(mergeBB);
	Builder->SetInsertPoint(mergeBB);

	PHINode *PN = Builder->CreatePHI(Type::getDoubleTy(*theContext), 2, "iftmp");

	PN->addIncoming(thenV, thenBB);
	PN->addIncoming(elseV, elseBB);

	return PN;
}

Value* GenerateCode::codegen(ForExprAST* a)
{

	Function *theFunction = Builder->GetInsertBlock()->getParent();
	BasicBlock *preHeaderBB = Builder->GetInsertBlock();
	
	AllocaInst* alloca = createEntryBlockAlloca(theFunction, a->varName);

	Value* startVal = a->start->codegen(this);
	if(!startVal)
	{
		return nullptr;
	}

	Builder->CreateStore(startVal, alloca);

  AllocaInst *oldVal = namedValues[a->varName];
	namedValues[a->varName] = alloca;

  BasicBlock *startCondition = BasicBlock::Create(*theContext, "startcond", theFunction);
  Builder->CreateBr(startCondition);

  Builder->SetInsertPoint(startCondition);

  Value *startCond = a->cond->codegen(this);
	if(!startCond)
	{
		return nullptr;
	}
	startCond = Builder->CreateFCmpONE(startCond, ConstantFP::get(*theContext, APFloat(0.0)), "startcond");

  BasicBlock *loopBB = BasicBlock::Create(*theContext, "loop", theFunction);

	//Builder->CreateBr(loopBB);
	Builder->SetInsertPoint(loopBB);

	// PHINode *variable = Builder->CreatePHI(Type::getDoubleTy(*theContext), 2, a->varName.c_str());
	
	// variable->addIncoming(startVal, preHeaderBB);

	if(!a->body->codegen(this))
	{
		return nullptr;
	}
  
  Value *stepVal = nullptr;
	if(a->step)
	{
		stepVal = a->step->codegen(this);
		if(!stepVal)
		{
			return nullptr;
		}
	}
	else
	{
		stepVal = ConstantFP::get(*theContext, APFloat(1.0));
	}

	Value *curVar = Builder->CreateLoad(alloca->getAllocatedType(), alloca, a->varName.c_str());
	Value *nextVar = Builder->CreateFAdd(curVar, stepVal, "nextvar");

	Builder->CreateStore(nextVar, alloca);
	BasicBlock *loopEndBB = Builder->GetInsertBlock();
  Builder->CreateBr(startCondition);

	BasicBlock *afterBB = BasicBlock::Create(*theContext, "afterloop", theFunction);
  
  Builder->SetInsertPoint(startCondition);
  Builder->CreateCondBr(startCond, loopBB, afterBB);
	Builder->SetInsertPoint(afterBB);

	if(oldVal)
	{
		namedValues[a->varName] = oldVal;
	}
	else
	{
		namedValues.erase(a->varName);
	}

	return Constant::getNullValue(Type::getDoubleTy(*theContext));
}

Value* GenerateCode::codegen(VarExprAST* a)
{
	std::map<std::string, AllocaInst*> oldNamedValues(namedValues);

	Function *theFunction = Builder->GetInsertBlock()->getParent();

	for(unsigned i = 0, e = (a->vars).size(); i != e; ++i)
	{
		std::string varName = (a->vars[i]).first;
		auto init = std::move((a->vars[i]).second);
		Value *initVal;

		if(init)
		{
			initVal = init->codegen(this);
			if(!initVal)
			{
				return nullptr;
			}
		}
		else
		{
			initVal = ConstantFP::get(*theContext, APFloat(0.0));
		}

		AllocaInst *alloca = createEntryBlockAlloca(theFunction, varName);
		Builder->CreateStore(initVal, alloca);

		namedValues[varName] = alloca;

		init.release();
	}

	Value *bodyVal = a->body->codegen(this);
	if(!bodyVal)
	{
		return nullptr;
	}

	namedValues = oldNamedValues;

	return bodyVal;
}

Function* GenerateCode::Codegen(PrototypeAST* a)
{
	std::vector<Type*> doubles(a->args.size(), Type::getDoubleTy(*theContext));
	
	FunctionType *FT = FunctionType::get(Type::getDoubleTy(*theContext), doubles, false);

	Function *F = Function::Create(FT, Function::ExternalLinkage, a->name, theModule.get());

	unsigned idx = 0;
	for(auto &arg : F->args())
	{
		//Setting name of arguments to make IR more readable - may need to change the PrototypeAST (change std::unique_ptr<ExprAST> to std::string - but might break something else :((())))
		arg.setName(a->argString[idx++]);
	}

	return F;
}

Function* GenerateCode::Codegen(FunctionAST* a)
{
	auto &p = *(a->proto);
	functionProtos[a->proto->getName()] = std::move(a->proto);
	Function *theFunction = getFunction(p.getName());

	// Function *theFunction = theModule->getFunction(a->proto->getName());

	// if(!theFunction)
	// {
	// 	theFunction = a->proto->Codegen(this);
	// }

	if(!theFunction)
	{
		return nullptr;
	}

	if(p.isBinaryOp())
	{
		binOpPrecedence[p.getName()[p.name.size() - 1]] = p.precedence;
	}

	// if(!theFunction->empty())
	// {
	// 	return (Function*)logErrorV("Function cannot be redefined");
	// }

	BasicBlock *BB = BasicBlock::Create(*theContext, "entry:", theFunction);
	Builder->SetInsertPoint(BB);

	namedValues.clear();
	for(auto &arg : theFunction->args())
	{
		AllocaInst* alloca = createEntryBlockAlloca(theFunction, arg.getName().str());
		Builder->CreateStore(&arg, alloca);
		namedValues[arg.getName().str()] = alloca;
	}

	if(Value *retVal = a->body->codegen(this))
	{
		Builder->CreateRet(retVal);

		verifyFunction(*theFunction);

		theFPM->run(*theFunction);

		return theFunction;
	}

	theFunction->eraseFromParent();
	return nullptr;
}

//Known bug: extern foo(a) followed by def foo(b) gives an error, the declaration using extern is given more precedence and is not validated properly


//Driver Code:: (Until I figure out how to share the llvm::Context with other files)

void initialiseJIT()
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
  	InitializeNativeTargetAsmParser();
	theJIT = exitOnErr(llvm::orc::SimpleJIT::create());
}

void initialiseModule()
{
	theContext = std::make_unique<LLVMContext>();
	theModule = std::make_unique<Module>("FirstLang", *theContext);
	theModule->setDataLayout(theJIT->getDataLayout());
	Builder = std::make_unique<IRBuilder<>>(*theContext);

	theFPM = std::make_unique<legacy::FunctionPassManager>(theModule.get());

	theFPM->add(createInstructionCombiningPass());
	theFPM->add(createReassociatePass());
	theFPM->add(createGVNPass());
	theFPM->add(createCFGSimplificationPass());
  theFPM->add(createPromoteMemoryToRegisterPass());
	theFPM->doInitialization();
}

bool genDefinition()
{
	if(auto fnAST = parseDefinition())
	{
		if(auto *fnIR = fnAST->Codegen(&codeGenerator))
		{
			fprintf(stderr, "Read function definition:\n");
			fnIR->print(errs());
			// fnIR->viewCFG();
			fprintf(stderr, "\n");
			exitOnErr(theJIT->addModule(llvm::orc::ThreadSafeModule(std::move(theModule), std::move(theContext))));
			initialiseModule();
		}
		return true;
	}

	return false;
}

bool genExtern()
{
	if(auto protoAST = parseExtern())
	{
		if(auto *fnIR = protoAST->Codegen(&codeGenerator))
		{
			fprintf(stderr, "Read extern function:\n");
			fnIR->print(errs());
			fprintf(stderr, "\n");
			functionProtos[protoAST->getName()] = std::move(protoAST);
		}
		return true;
	}

	return false;
}

bool genTopLvlExpr()
{
	if(auto fnAST = parseTopLvlExpr())
	{
		if(auto *fnIR = fnAST->Codegen(&codeGenerator))
		{
			fprintf(stderr, "Read top-level expression:\n");
			fnIR->print(errs());
			fprintf(stderr, "\n");

			auto RT = theJIT->getMainJITDylib().createResourceTracker();
			auto TSM = llvm::orc::ThreadSafeModule(std::move(theModule), std::move(theContext));
			exitOnErr(theJIT->addModule(std::move(TSM), RT));

			initialiseModule();

			auto exprSymbol = exitOnErr(theJIT->lookup("__anon_expr"));
			// assert(exprSymbol && "Function not found.");

			double (*FP)() = (double (*)())(intptr_t)exprSymbol.getAddress(); //casting a function to the right type to call it natively
			fprintf(stderr, "Evaluated to %f\n", FP());

			exitOnErr(RT->remove());
		}
		return true;
	}

	return false;
}

void printALL()
{
	theModule->print(errs(), nullptr);
}
