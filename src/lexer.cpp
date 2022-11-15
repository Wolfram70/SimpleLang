#include <iostream>
#include <string>
#include <cstdlib>

#include "../include/lexExtern.h"

std::string identifierStr;
double numVal;

extern int getToken()
{
	static char lastChar = ' ';

	while(isspace(lastChar))
	{
		lastChar = getchar();
	}

	if(isalpha(lastChar))
	{
		identifierStr.erase();
		identifierStr.push_back(lastChar);

		while(isalnum(lastChar = getchar()))
		{
			identifierStr.push_back(lastChar);
		}

		if(identifierStr == "def")
		{
			return tok_def;
		}
		else if(identifierStr == "extern")
		{
			return tok_extern;
		}
		else
		{
			return tok_identifier;
		}
	}
	else if(isdigit(lastChar) || lastChar == '.')
	{
		std::string numStr;
		do
		{
			numStr.push_back(lastChar);
			lastChar = getchar();
		} while (isdigit(lastChar) || lastChar == '.');

		numVal = strtod(numStr.c_str(), 0);
		return tok_number;
	}
	else if(lastChar == '#')
	{
		do
		{
			lastChar = getToken();
		} while (lastChar != EOF && lastChar != '\n' && lastChar != '\r');

		if(lastChar != EOF)
		{
			return getToken();
		}
		else
		{
			return tok_eof;
		}
	}
	
	int thisChar = lastChar;
	lastChar = getchar();
	return thisChar;
}