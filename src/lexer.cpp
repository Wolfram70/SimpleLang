#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>

#include "../include/lexExtern.h"

std::string identifierStr;
double numVal;

SourceLocation curLoc;
SourceLocation lexLoc = {1, 0};

extern int advance() {
  int lastChar = file.get();

  if (lastChar == '\n' || lastChar == '\r') {
    lexLoc.line++;
    lexLoc.col = 0;
  } else {
    lexLoc.col++;
  }
  return lastChar;
}

extern int getToken() {
  static char lastChar = ' ';

  while (isspace(lastChar)) {
    lastChar = advance();
  }

  if (isalpha(lastChar)) {
    identifierStr.erase();
    identifierStr.push_back(lastChar);

    while (isalnum(lastChar = advance())) {
      identifierStr.push_back(lastChar);
    }

    if (identifierStr == "def" || identifierStr == "DEF" ||
        identifierStr == "define") {
      return tok_def;
    } else if (identifierStr == "extern" || identifierStr == "EXTERN") {
      return tok_extern;
    } else if (identifierStr == "if" || identifierStr == "IF") {
      return tok_if;
    } else if (identifierStr == "then" || identifierStr == "THEN") {
      return tok_then;
    } else if (identifierStr == "else" || identifierStr == "ELSE") {
      return tok_else;
    } else if (identifierStr == "for" || identifierStr == "FOR") {
      return tok_for;
    } else if (identifierStr == "when" || identifierStr == "WHEN") {
      return tok_when;
    } else if (identifierStr == "inc" || identifierStr == "INC") {
      return tok_inc;
    } else if (identifierStr == "do" || identifierStr == "DO") {
      return tok_do;
    } else if (identifierStr == "binary") {
      return tok_binary;
    } else if (identifierStr == "unary") {
      return tok_unary;
    } else if (identifierStr == "var" || identifierStr == "VAR") {
      return tok_var;
    } else if (identifierStr == "in" || identifierStr == "IN") {
      return tok_in;
    } else {
      return tok_identifier;
    }
  } else if (isdigit(lastChar) || lastChar == '.') {
    std::string numStr;
    do {
      numStr.push_back(lastChar);
      lastChar = advance();
    } while (isdigit(lastChar) || lastChar == '.');

    numVal = strtod(numStr.c_str(), 0);
    return tok_number;
  } else if (lastChar == '#') {
    do {
      lastChar = getToken();
    } while (lastChar != EOF && lastChar != '\n' && lastChar != '\r');

    if (lastChar != EOF) {
      return getToken();
    } else {
      return tok_eof;
    }
  }

  int thisChar = lastChar;
  lastChar = advance();
  return thisChar;
}
