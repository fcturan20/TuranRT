#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
/*
bool tesl_isEmptyChar(char c) { return c < 33 && c != '\n' && c != 0; }
// In version 0.0.2, all of the token features will be used
struct tesl_token {
  unsigned int startIndx, endIndx;
  enum class type {
    name,
    Operator,     // operators like '!', '.', ',', '[', ']', '(', ')', '+', '-', '*', '/'
    num,          // Numeric
    Struct,       // struct keyword
    commentLine,  // C-like comment
    startList,    // (
    endList,      // )
    startDef,     // {
    endDef,       // }
    output,       // shader-stage based output (rt for fragment etc)
    include,      // Including a previously defined struct
    as,           // typedef in TESL
    endStatement  // Statement finish
  };
  type tokenType;
};
struct tesl_proccessingCode {
  const char*             c;
  uint32_t                cIndx, codeLen;
  const char*             finalCode;
  std::vector<tesl_token> tokens;
};

void tesl_skipEmptySpace(tesl_proccessingCode& code) {
  // Skip comment lines
  if (code.c[code.cIndx] == '/' && code.c[code.cIndx + 1] == '/') {
    tesl_token token;
    token.startIndx = code.cIndx;
    while (code.c[++code.cIndx] != '\n' && code.cIndx < code.codeLen) {
      continue;
    }
    token.endIndx = code.cIndx + 1;
    code.tokens.push_back(token);
  }
  // Skip empty characters
  while (tesl_isEmptyChar(code.c[code.cIndx++])) {
  }
}

bool tesl_handleOperator(tesl_proccessingCode& code) {
  tesl_token tok;
  bool       isFound = false;
  tok.startIndx      = code.cIndx;
  tok.endIndx        = code.cIndx + 1;
  tok.tokenType      = tesl_token::type::Operator;
  switch (code.c[code.cIndx]) {
    case '!':
    case '.':
    case ',':
    case '[':
    case ']': isFound = true;
  }
  
  if (isFound) {
    code.tokens.push_back(tok);
    code.cIndx = tok.endIndx;
    return true;
  }
  return false;
}
bool tesl_handleNum(tesl_proccessingCode& code) {
  tesl_token tok;
  tok.startIndx = code.cIndx;
  while (code.c[code.cIndx]) {
  }
}
bool tesl_handleKeyword(tesl_proccessingCode& code) {}
bool tesl_handleEndStatement(tesl_proccessingCode& code) {}

unsigned char tesl_handlePreprocessor(tesl_proccessingCode& code) {}
void          tesl_getNextToken(tesl_proccessingCode& code) {
  tesl_skipEmptySpace(code);
  if (tesl_handleOperator(code)) {
  } else if (tesl_handleNum(code)) {
  } else if (tesl_handleKeyword(code)) {
  } else if (tesl_handleEndStatement(code)) {
  }
}

struct teShader* compileShader(const char* code) {
  tesl_proccessingCode processingCode;
  processingCode.codeLen   = strlen(code);
  processingCode.c         = code;
  processingCode.cIndx     = 0;
  processingCode.finalCode = new char[processingCode.codeLen]{'\0'};

  // Lexer
  while (processingCode.cIndx < processingCode.codeLen) {
    tesl_getNextToken(processingCode);
  }

  // Parser
  for (uint32_t i = 0; i < processingCode.tokens.size(); i++) {
    tesl_token& t = processingCode.tokens[i];
  }
}*/

//struct tePipeline* createPipeline(unsigned int shaderCount, struct teShader* const* shaders) {}
