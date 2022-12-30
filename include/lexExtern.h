#include <map>
#include <string>
#include <fstream>

enum Token
{
	tok_eof = -1,
	tok_def = -2,
	tok_extern = -3,
	tok_identifier = -4,
	tok_number = -5,
	tok_if = -6,
	tok_then = -7,
	tok_else = -8,
	tok_for = -9,
	tok_when = -10,
	tok_inc = -11,
	tok_do = -12,
	tok_binary = -13,
	tok_unary = -14,
	tok_var = -15,
	tok_in = -16,
};

struct SourceLocation
{
  int line;
  int col;
};

extern double numVal;
extern std::string identifierStr;
extern char curTok;
extern std::map<char, int> binOpPrecedence;
extern SourceLocation curLoc;
extern SourceLocation lexLoc;
extern std::ifstream file;
extern bool enableDebug;
extern bool printDebug;
extern bool printIR;
extern std::string outFileName;
extern std::string fileName;
