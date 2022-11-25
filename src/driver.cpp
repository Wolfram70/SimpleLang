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
extern void initialize();

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
			case tok_number:
				handleTopLvlExpr();
				break;
			case tok_identifier:
				handleTopLvlExpr();
				break;
			default:
				fprintf(stderr, "Ready>>");
				getNextToken();
				break;
		}
	}
}

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

extern "C" DLLEXPORT double clear() {
	printf("\e[1;1H\e[2J");
	return 0;
}

int main()
{
	binOpPrecedence[':'] = 1;
	binOpPrecedence['='] = 2;
	binOpPrecedence['<'] = 10;
	binOpPrecedence['+'] = 20;
	binOpPrecedence['-'] = 20;
	binOpPrecedence['*'] = 40;

	fprintf(stderr, "Ready>>");
	getNextToken();

	initialiseModule();

	mainLoop();

  initialize();

	return 0;
}
