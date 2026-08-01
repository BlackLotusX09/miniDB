#include "parser/lexer.h"
#include<cassert>
#include <iostream>
#include "parser/token.h"

using namespace std;



int main(){
    Lexer lex("SELECT name FROM students WHERE age >= 18;");
    Token t;
    while((t = lex.NextToken()).type != TokenType::END_OF_FILE){
        cout<<t.value<<endl;
    }
}
