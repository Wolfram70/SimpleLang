#include "../include/parser.h"

extern int getNextToken();
extern std::unique_ptr<FunctionAST> parseDefinition();
extern std::unique_ptr<PrototypeAST> parseExtern();
extern std::unique_ptr<FunctionAST> parseTopLvlExpr();

extern void initialiseModule();
extern bool genDefinition();
extern bool genExtern();
extern bool genTopLvlExpr();
extern void printALL();

static void handleDefinition()
{
	if(!genDefinition())
	{
		getNextToken();
	}
}

static void handleExtern()
{
	if(!genExtern())
	{
		getNextToken();
	}
}

static void handleTopLvlExpr()
{
	if(!genTopLvlExpr())
	{
		getNextToken();
	}
}

static void mainLoop()
{
	while(true)
	{
		switch(curTok)
		{
			case tok_eof:
				return;
			case ';':
				fprintf(stderr, "Ready>>");
				getNextToken();
				break;
			case tok_def:
				handleDefinition();
				break;
			case tok_extern:
				handleExtern();
				break;
			case tok_identifier:
				handleTopLvlExpr();
				break;
			case tok_number:
				handleTopLvlExpr();
				break;
			default:
				fprintf(stderr, "Ready>>");
				getNextToken();
				break;
		}
	}
}

int main()
{
	binOpPrecedence['<'] = 10;
	binOpPrecedence['+'] = 20;
	binOpPrecedence['-'] = 20;
	binOpPrecedence['*'] = 40;

	fprintf(stderr, "Ready>>");
	getNextToken();

	initialiseModule();

	mainLoop();

	printALL();

	return 0;
}