#include <map>

enum Token
{
	tok_eof = -1,
	tok_def = -2,
	tok_extern = -3,
	tok_identifier = -5,
	tok_number = -10,
};

extern double numVal;
extern std::string identifierStr;
extern char curTok;
extern std::map<char, int> binOpPrecedence;