#include <cctype>
#include <string>

// lexer

enum Token {
   tok_eof    = -1,

   tok_def    = -2,
   tok_extern = -3,

   tok_ident  = -4,
   tok_number = -5,
};

static std::string ident;
static double value;

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

      value = std::strtod(number.c_str(), 0);
      return tok_number;
   }

   if (cursor == EOF)
      return tok_eof;

   int c = cursor;
   cursor = std::getchar();
   return c;
}

int main(int argc, char* argv[]) {
   return 0;
}
