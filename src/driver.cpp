#include "../include/parser.h"

extern int getNextToken();
extern std::unique_ptr<FunctionAST> parseDefinition();
extern std::unique_ptr<PrototypeAST> parseExtern();
extern std::unique_ptr<FunctionAST> parseTopLvlExpr();

std::ifstream file;
std::string outFileName = "out.o";
std::string fileName;

bool enableDebug = true;
bool printDebug = false;
bool printIR = true;

extern void initialiseModule();
extern bool genDefinition();
extern bool genExtern();
extern bool genTopLvlExpr();
extern void printALL();
extern void initialize();
extern void compileToObject();

static void handleDefinition() {
  if (!genDefinition()) {
    getNextToken();
  }
}

static void handleExtern() {
  if (!genExtern()) {
    getNextToken();
  }
}

static void handleTopLvlExpr() {
  if (!genTopLvlExpr()) {
    getNextToken();
  }
}

static void mainLoop() {
  while (true) {
    switch (curTok) {
      case tok_eof:
        return;
      case ';':
        // fprintf(stderr, "Ready>>");
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
        // fprintf(stderr, "Ready>>");
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

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Invalid number of arguments" << std::endl;
    return 1;
  }

  std::string arg;

  for (int i = 1; i < argc; i++) {
    switch (argv[i][0]) {
      case '-':
        switch (argv[i][1]) {
          case 'm':
            i++;
            arg = argv[i];
            if (arg == "debug") {
              enableDebug = true;
            } else if (arg == "release") {
              std::cout << "Disabled debug" << std::endl;
              enableDebug = false;
            } else {
              std::cout << "Invalid argument for -m" << std::endl;
              return 1;
            }
            break;
          case 'e':
            printDebug = true;
            break;
          case 'o':
            i++;
            outFileName = argv[i];
            if (outFileName == "") {
              std::cout << "Invalid argument for -o" << std::endl;
              return 1;
            }
            if (outFileName[outFileName.size() - 1] != 'o') {
              outFileName = outFileName + ".o";
            }
            break;
          default:
            std::cout << "Invalid argument: " << argv[i] << std::endl;
            return 1;
        }
        break;
      default:
        fileName = argv[i];
        break;
    }
  }

  file.open(fileName, std::ios::in);

  if (!file.is_open()) {
    std::cout << "Could not open file \"" << fileName << "\"" << std::endl;
    return 1;
  }

  binOpPrecedence[':'] = 1;
  binOpPrecedence['='] = 2;
  binOpPrecedence['<'] = 10;
  binOpPrecedence['+'] = 20;
  binOpPrecedence['-'] = 20;
  binOpPrecedence['*'] = 40;

  // fprintf(stderr, "Ready>>");
  getNextToken();

  initialiseModule();
  initialize();

  mainLoop();

  printALL();

  compileToObject();

  // initialize();

  return 0;
}
