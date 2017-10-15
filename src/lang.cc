#include <cassert>
#include <cctype>
#include <string>
#include <vector>
#include <map>

#include "llvm/ADT/STLExtras.h"

// lexer

enum Token {
   tok_eof    = -1,

   tok_def    = -2,
   tok_extern = -3,

   tok_ident  = -4,
   tok_number = -5,
};

static std::string ident;
static double numval;

static int gettoken() {
   static int cursor = ' ';

   while (std::isspace(cursor))
      cursor = std::getchar();

   if (cursor == '#') {
      do
         cursor = std::getchar();
      while (cursor != EOF && cursor != '\n' && cursor != '\r');

      if (cursor != EOF)
         return gettoken();
   }

   if (std::isalpha(cursor)) {
      ident = cursor;
      while (std::isalnum((cursor = std::getchar())))
         ident += cursor;

      if (ident == "def")
         return tok_def;
      if (ident == "extern")
         return tok_extern;
      return tok_ident;
   }

   if (std::isdigit(cursor)) {
      std::string number;
      do {
         number += cursor;
         cursor = std::getchar();
      } while (std::isdigit(cursor) || cursor == '.');

      numval = std::strtod(number.c_str(), 0);
      return tok_number;
   }

   if (cursor == EOF)
      return tok_eof;

   int c = cursor;
   cursor = std::getchar();
   return c;
}

// abstract syntax tree

class ExprAST {
   public:
      virtual ~ExprAST() {}
};

class VariableExprAST : public ExprAST {
   std::string name;

   public:
   VariableExprAST(const std::string &name) : name(name) {}
};

class NumberExprAST : public ExprAST {
   double val;

   public:
   NumberExprAST(double val) : val(val) {}
};

class BinaryExprAST : public ExprAST {
   char op;
   std::unique_ptr<ExprAST> left, right;

   public:
   BinaryExprAST(
         char op, std::unique_ptr<ExprAST> left, std::unique_ptr<ExprAST> right)
      :
         op(op),
         left(std::move(left)),
         right(std::move(right)) {}
};

class CallExprAST : public ExprAST {
   std::string callee;
   std::vector<std::unique_ptr<ExprAST>> args;

   public:
   CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args)
      :
         callee(callee),
         args(std::move(args)) {}
};


class PrototypeAST {
   std::string name;
   std::vector<std::string> args;

   public:
   PrototypeAST(const std::string &name, std::vector<std::string> args)
      :
         name(name),
         args(std::move(args)) {}

   const std::string& get_name() const { return name; }
};

class FunctionAST {
   std::unique_ptr<PrototypeAST> proto;
   std::unique_ptr<ExprAST> body;

   public:
   FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body)
      :
         proto(std::move(proto)),
         body(std::move(body)) {}
};

// parser

static int token;
static int nexttoken() {
   return token = gettoken();
}

static std::map<char, int> precedence;

static int get_token_precedence() {
   if (!isascii(token))
      return -1;

   int pr = precedence[token];
   if (pr <= 0)
      return -1;
   return pr;
}

std::unique_ptr<ExprAST> err(const char* str) {
   fprintf(stderr, "error: %s\n", str);
   return nullptr;
}

std::unique_ptr<PrototypeAST> errproto(const char* str) {
   err(str);
   return nullptr;
}

static std::unique_ptr<ExprAST> parse_expr();

static std::unique_ptr<ExprAST> parse_number_expr() {
   assert(token == tok_number);
   auto result = llvm::make_unique<NumberExprAST>(numval);
   nexttoken();
   return std::move(result);
}

static std::unique_ptr<ExprAST> parse_paren_expr() {
   nexttoken();
   auto expr = parse_expr();
   if (!expr)
      return nullptr;
   if (token != ')')
      return err("expected ')'in expression");
   nexttoken();
   return expr;
}

static std::unique_ptr<ExprAST> parse_ident_expr() {
   assert(token == tok_ident);
   nexttoken();

   if (token != '(')
      return llvm::make_unique<VariableExprAST>(ident);

   nexttoken();
   std::vector<std::unique_ptr<ExprAST>> args;
   while (token != ')') {
      if (auto arg = parse_expr())
         args.push_back(std::move(arg));
      else
         return nullptr;

      if (token == ')')
         break;

      if (token != ',')
         return err("expected ')' or ',' in argument list");

      nexttoken();
   }
   nexttoken();

   return llvm::make_unique<CallExprAST>(ident, std::move(args));
}

static std::unique_ptr<ExprAST> parse_primary_expr() {
   switch (token) {
      case tok_ident:
         return parse_ident_expr();
      case tok_number:
         return parse_number_expr();
      case '(':
         return parse_paren_expr();
      default:
         return err("unknown token when parsing an expression");
   }
}

static std::unique_ptr<ExprAST> parse_binary_op_right(
      int pr, std::unique_ptr<ExprAST> left) {
   int tok_prec = get_token_precedence();
   while (true) {
      if (tok_prec < pr)
         return left;

      int op = token;
      nexttoken();

      auto right = parse_primary_expr();
      if (!right)
         return nullptr;

      int next_tok_prec = get_token_precedence();
      if (tok_prec < next_tok_prec) {
         right = parse_binary_op_right(tok_prec+1, std::move(right));
         if (!right)
            return nullptr;
      }

      left = llvm::make_unique<BinaryExprAST>(op, std::move(left), std::move(right));
   }
}

static std::unique_ptr<ExprAST> parse_expr() {
   auto left = parse_primary_expr();
   if (!left)
      return nullptr;
   return parse_binary_op_right(0, std::move(left));
}

static std::unique_ptr<PrototypeAST> parse_prototype() {
   if (token != tok_ident)
      return errproto("expected function name in prototype");

   std::string name = ident;
   nexttoken();

   if (token != '(')
      return errproto("expected '(' in prototype");

   std::vector<std::string> args;
   while (nexttoken() == tok_ident)
      args.push_back(ident);
   if (token != ')')
      return errproto("expected ')' in prototype");

   nexttoken();
   return llvm::make_unique<PrototypeAST>(name, std::move(args));
}

static std::unique_ptr<FunctionAST> parse_def() {
   nexttoken();
   auto proto = parse_prototype();
   if (!proto)
      return nullptr;

   if (auto expr = parse_expr())
      return llvm::make_unique<FunctionAST>(std::move(proto), std::move(expr));
   return nullptr;
}

static std::unique_ptr<PrototypeAST> parse_extern() {
   nexttoken();
   return parse_prototype();
}

static std::unique_ptr<FunctionAST> parse_top_level_expr() {
   if (auto expr = parse_expr()) {
      auto proto = llvm::make_unique<PrototypeAST>(
            "__anon_expr", std::vector<std::string>());
      return llvm::make_unique<FunctionAST>(std::move(proto), std::move(expr));
   }
   return nullptr;
}

// mainloop

static void handle_def() {
   if (parse_def())
      fprintf(stderr, "parsed a def\n");
   else
      nexttoken();
}

static void handle_extern() {
   if (parse_extern())
      fprintf(stderr, "parsed an extern\n");
   else
      nexttoken();
}

static void handle_top_level_expr() {
   if (parse_top_level_expr())
      fprintf(stderr, "parsed a top-level expr\n");
   else
      nexttoken();
}

static void init() {
   precedence['<'] = 10;
   precedence['>'] = 10;
   precedence['+'] = 20;
   precedence['-'] = 20;
   precedence['*'] = 40;
   precedence['/'] = 40;
}

static void repl() {
   fprintf(stderr, "ready> ");
   nexttoken();
   while (true) {
      fprintf(stderr, "ready> ");
      switch (token) {
         case tok_eof:
            return;
         case ';':
            nexttoken();
            break;
         case tok_def:
            handle_def();
            break;
         case tok_extern:
            handle_extern();
            break;
         default:
            handle_top_level_expr();
            break;
      }
   }
}

int main(int argc, char* argv[]) {
   init(); repl();
}
