#include "cc.h"

Token* preprocess(Token* tokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    cur = cur->next = token;
  }

  return head.next;
}