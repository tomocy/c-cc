#include "cc.h"

Token* preprocess(Token* tokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    if (!equal_to_token(token, "#")) {
      cur = cur->next = token;
      continue;
    }

    if (token->is_bol) {
      // null directive
      if (token->next->is_bol) {
        continue;
      }
    }

    error_token(token, "invalid preprocessor directive");
  }

  return head.next;
}