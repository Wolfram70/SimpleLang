#include "../include/parser.h"

extern int getNextToken();
extern std::unique_ptr<FunctionAST> parseDefinition();
extern std::unique_ptr<PrototypeAST> parseExtern();
extern std::unique_ptr<FunctionAST> parseTopLvlExpr();

static void handleDefinition()
{
	if(parseDefinition())
	{
		fprintf(stderr, "Parsed a function definition \n");
	}
	else
	{
		getNextToken();
	}
}

static void handleExtern()
{
	if(parseExtern())
	{
		fprintf(stderr, "Parsed an external function \n");
	}
	else
	{
		getNextToken();
	}
}

static void handleTopLvlExpr()
{
	if(parseTopLvlExpr())
	{
		fprintf(stderr, "Parsed a top level expression \n");
	}
	else
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

	mainLoop();

	return 0;
}