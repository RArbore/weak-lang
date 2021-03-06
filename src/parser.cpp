// This file is part of weak-lang.
// weak-lang is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
// weak-lang is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
// You should have received a copy of the GNU Affero General Public License
// along with weak-lang. If not, see <https://www.gnu.org/licenses/>.

#include "parser.hpp"

Parser::Parser(std::vector<Token> input): tokens(input) {}

std::vector<Stmt*> Parser::parse() {
    while (cur_index < tokens.size() && tokens.at(cur_index).type != END) {
        decls.push_back(declaration());
    }
    return decls;
}

std::string Parser::as_dot() {
    std::string dot;
    dot += "digraph AST { \n";
    dot += "\n";
    dot += "splines=\"FALSE\"; \n";
    for(auto decl : decls) {
        dot += decl->to_string().second;
    }
    dot += "\n}\n";
    return dot;
}

bool Parser::match(TokenType type) {
    if (tokens.at(cur_index).type == type) {
        cur_index++;
        return true;
    }
    return false;
}

bool Parser::match(TokenType type1, TokenType type2) {
    return match(type1) || match(type2);
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for(auto t : types) {
        if(match(t)) {
            return true;
        }
    }
    return false;
}

bool Parser::currently_at(TokenType type) {
    return tokens.at(cur_index).type == type;
}

bool Parser::currently_at(std::initializer_list<TokenType> types) {
    for(auto t : types) {
        if (currently_at(t)) {
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, std::string message) {
    if (tokens.at(cur_index).type == type) return tokens.at(cur_index++);
    throw std::runtime_error(create_error(tokens.at(cur_index), message));
}

Token Parser::consume(std::initializer_list<TokenType> types, std::string message) {
    for(auto type : types) {
        if(tokens.at(cur_index).type == type) {
            return tokens.at(cur_index++);
        }
    }
    throw std::runtime_error(create_error(tokens.at(cur_index), message));
}

std::string Parser::create_error(Token token, std::string message) {
    return message + " but instead found: \"" + token.lexeme + "\", at line " + std::to_string(token.line + 1) + " and column " + std::to_string(token.col + 1) + ", this token has type " + print_token_type(token.type);
}

Stmt* Parser::declaration() {
    if (match(FUNCTION)) return funDeclaration();
    if (match(OPERATOR)) return opDeclaration();
    if (match(LET)) return varDeclaration();
    return statement();
}

Stmt* Parser::funDeclaration() {
    Token name = consume(IDENTIFIER, "Expected function name");

    consume(LEFT_PAREN, "Expected '(' after function name");
    std::vector<Token> params;
    while (tokens.at(cur_index).type != END && tokens.at(cur_index).type != RIGHT_PAREN) {
        if (params.size() == 0) {
            params.push_back(consume(IDENTIFIER, "Expected parameter name in function declaration"));
        }
        else if (params.size() < MAX_ARGS) {
            consume(COMMA, "Expected comma in function declaration");
            params.push_back(consume(IDENTIFIER, "Expected parameter name in function declaration"));
        }
        else {
            throw std::runtime_error(create_error(name, "Too many arguments for function (max " + std::to_string(MAX_ARGS) + ")"));
        }
    }
    consume(RIGHT_PAREN, "Expected ')' after parameters of function");

    consume(LEFT_BRACE, "Expected '{' before function body");
    std::vector<Stmt*> body = block();
    return new FuncDecl(name, params, body);
}

Stmt* Parser::opDeclaration() {
    Token name = consume(IDENTIFIER, "Expected operator name");

    consume(LEFT_PAREN, "Expected '(' after operator name");
    Token left = consume(IDENTIFIER, "Exepcted left parameter name in operator declaration");
    consume(COMMA, "Exepcted ',' after left operator parameter");
    Token right = consume(IDENTIFIER, "Expected right parameter name in operator declaration");
    consume(RIGHT_PAREN, "Expected ')' after operator parameters list");

    consume(LEFT_BRACE, "Expected '{' before operator body");
    std::vector<Stmt*> body = block();
    return new OpDecl(name, left, right, body);
}

Stmt* Parser::varDeclaration() {
    Token name = consume(IDENTIFIER, "Expected variable name");

    Expr* initialize;
    if (match(EQUALS)) initialize = expression();
    else initialize = new Nil();

    consume(SEMI, "Expect semi-colon after variable declaration");
    return new VarDecl(name, initialize);
}

Stmt* Parser::statement() {
    if (match(IF)) return ifStatement();
    if (match(PRINT)) return printStatement();
    if (match(RETURN)) return returnStatement();
    if (match(WHILE)) return whileStatement();
    if (match(ASSERT)) return assertStatement(); 
    else return exprStatement();
}

std::vector<Stmt*> Parser::block() {
    std::vector<Stmt*> contents;
    while(tokens.at(cur_index).type != END && tokens.at(cur_index).type != RIGHT_BRACE) {
        contents.push_back(declaration());
    }

    if(tokens.at(cur_index).type == END) {
        create_error(tokens.at(cur_index), "Expected '}' after block");
    }

    consume(RIGHT_BRACE, "Expected '}' after block");
    return contents;
}

Stmt* Parser::exprStatement() {
    Expr* exp = expression();
    consume(SEMI, "Expected ';' after expression");
    Stmt* stmt = new ExprStmt(exp);
    return stmt;
}

Stmt* Parser::ifStatement() {
    Token ifToken = tokens.at(cur_index - 1);
    consume(LEFT_PAREN, "Expected '(' before condition");
    Expr* condition = expression();
    consume(RIGHT_PAREN, "Expected ')' after condition");

    consume(LEFT_BRACE, "Expected '{' before contents of if statement");
    std::vector<Stmt*> body = block();
    return new If(ifToken, condition, body);
}

Stmt* Parser::printStatement() {
    Stmt* stmt = new Print(tokens.at(cur_index - 1), expression());
    consume(SEMI, "Expected ';' after print statement");
    return stmt;
}

Stmt* Parser::whileStatement() {
    Token whileToken = tokens.at(cur_index - 1);
    consume(LEFT_PAREN, "Expected '(' before condition");
    Expr* condition = expression();
    consume(RIGHT_PAREN, "Expected ')' after condition");

    consume(LEFT_BRACE, "Expected '{' before contents of while loop");
    std::vector<Stmt*> body = block();
    return new While(whileToken, condition, body);
}

Stmt* Parser::returnStatement() {
    Expr* expr;
    if (cur_index < tokens.size() && tokens.at(cur_index).type != SEMI) expr = expression();
    else expr = new Nil();
    Stmt* stmt = new Return(tokens.at(cur_index - 1), expr);
    consume(SEMI, "Expected ';' after return statement");
    return stmt;
}

Stmt* Parser::assertStatement() {
    Expr* expr = expression(); 
    Stmt* stmt = new Assert(tokens.at(cur_index - 1), expr);
    consume(SEMI, "Expected ';' after assert statement"); 
    return stmt; 
}

Expr* Parser::expression() {
    return assignment();
}

Expr* Parser::assignment() {
    if(tokens.at(cur_index).type == IDENTIFIER && cur_index < tokens.size() - 1 && tokens.at(cur_index + 1).type == EQUALS) {
        Token id = consume(IDENTIFIER, "Expected identifier");
        consume(EQUALS, "Expected '=' after identifier");
        Expr* right = assignment();
        return new Assign(id, right);
    } else if (tokens.at(cur_index).type == IDENTIFIER && cur_index < tokens.size() - 1 && tokens.at(cur_index + 1).type == LEFT_BRACK) {
        size_t dummy_index = cur_index;
        while (dummy_index < tokens.size() && tokens.at(dummy_index).type != RIGHT_BRACK) dummy_index++;
        if (dummy_index >= tokens.size() || (dummy_index < tokens.size() - 1 && tokens.at(dummy_index + 1).type != EQUALS)) return operation();
        Token id = consume(IDENTIFIER, "Expected identifier");
        Token left_b = consume(LEFT_BRACK, "Unreachable");
        Expr* first_dim = expression();
        std::vector<Expr*> args;
        args.push_back(first_dim);
        while(cur_index < tokens.size() && tokens.at(cur_index).type != RIGHT_BRACK) {
            if (args.size() < MAX_ARGS) {
                consume(COMMA, "Expected comma in array indexing");
                Expr* arg = expression();
                args.push_back(arg);
            }
            else {
                throw std::runtime_error(create_error(left_b, "Too many arguments for array indexing (max " + std::to_string(MAX_ARGS) + ")"));
            }
        }
        consume(RIGHT_BRACK, "Expected ']' after indices");
        consume(EQUALS, "Expected '=' after identifier");
        Expr* right = assignment();
        return new Assign(id, args, right);
    } else {
        return operation();
    }
}

Expr* Parser::operation() {
    Expr* exp = logicOr();
    while(match(IDENTIFIER)) {
        Token id = tokens.at(cur_index-1);
        if (id.type == EQUALS) throw std::runtime_error(create_error(id, "Can't parse = as a binary operator"));
        Expr* right = logicOr();
        exp = new Binary(exp, id, right);
    }
    return exp;
}

Expr* Parser::logicOr() {
    Expr* exp = logicAnd();
    while (currently_at(OR)) {
        Token t = consume(OR, "Expected 'O'");
        Expr* next = logicAnd();
        exp = new Binary(exp, t, next);
    }
    return exp;
}

Expr* Parser::logicAnd() {
    Expr* exp = equality();
    while (currently_at(AND)) {
        Token a = consume(AND, "Expected 'A'");
        Expr* next = equality();
        exp = new Binary(exp, a, next);
    }
    return exp;
}

Expr* Parser::equality() {
    Expr* exp = comparison();
    while (currently_at({EQUALS_EQUALS, EXCLA_EQUALS})) {
        Token t = consume({EQUALS_EQUALS, EXCLA_EQUALS}, "Expected '==' or '!='");
        Expr* next = comparison();
        exp = new Binary(exp, t, next);
    }
    return exp; 
}

Expr* Parser::comparison() {
    Expr* exp = term();
    while(currently_at({GREATER_EQUALS, GREATER, LESSER_EQUALS, LESSER})) {
        Token comp = consume({GREATER_EQUALS, GREATER, LESSER_EQUALS, LESSER}, "Expected '>=', '>', '<=', '<'");
        Expr* next = term();
        exp = new Binary(exp, comp, next);
    }
    return exp;
}

Expr* Parser::term() {
    Expr* left = factor();
    while(currently_at({MINUS, PLUS})) {
        Token op = consume({MINUS, PLUS}, "Expected '-' or '+'");
        Expr* fac = factor();
        left = new Binary(left, op, fac);
    }
    return left;
}

Expr* Parser::factor() {
    Expr* left = unary();
    while(currently_at({SLASH, STAR, AT, AS_SHAPE, EXP})) {
        Token op = consume({SLASH, STAR, AT, AS_SHAPE, EXP}, "Expected '/', '*', '@', 'sa', '^'");
        Expr* un = unary();
        left = new Binary(left, op, un);
    }
    return left;
}

Expr* Parser::unary() {
    switch(tokens.at(cur_index).type) {
        case EXCLA:
        case MINUS:
        case SHAPE: {
            Token t = tokens.at(cur_index);
            cur_index += 1;
            Expr* next = unary();
            return new Unary(t, next);
        }
        default:
            return arrAccess();
    }
}

Expr* Parser::arrAccess() {
    Expr* id = function();
    if(match(LEFT_BRACK)) {
        Token left_b = tokens.at(cur_index - 1);
        Expr* first_dim = expression();
        std::vector<Expr*> args;
        args.push_back(first_dim);
        while(cur_index < tokens.size() && tokens.at(cur_index).type != RIGHT_BRACK) {
            if (args.size() < MAX_ARGS) {
                consume(COMMA, "Expected comma in array indexing");
                Expr* arg = expression();
                args.push_back(arg);
            }
            else {
                throw std::runtime_error(create_error(left_b, "Too many arguments for array indexing (max " + std::to_string(MAX_ARGS) + ")"));
            }
        }
        consume(RIGHT_BRACK, "Expected ']' after indices");
        return new ArrAccess(id, left_b, args);
    }
    return id;
}

Expr* Parser::function() {
    if(tokens.at(cur_index).type == IDENTIFIER && cur_index < tokens.size() - 1 && tokens.at(cur_index+1).type == LEFT_PAREN) {
        Token name = consume(IDENTIFIER, "");
        Token left_p = consume(LEFT_PAREN, "");
        std::vector<Expr*> args;
        while(cur_index < tokens.size() && tokens.at(cur_index).type != RIGHT_PAREN) {
            if (args.size() == 0) {
                Expr* arg = expression();
                args.push_back(arg);
            }
            else if (args.size() < MAX_ARGS) {
                consume(COMMA, "Expected comma in function call");
                Expr* arg = expression();
                args.push_back(arg);
            }
            else {
                throw std::runtime_error(create_error(name, "Too many arguments for function (max " + std::to_string(MAX_ARGS) + ")"));
            }
        }
        consume(RIGHT_PAREN, "Expected ')' after function arguments");
        return new Func(name, left_p, args);
    } else {
        return primary();
    }
}

Expr* Parser::primary() {
    if(match(TRUE)) return new Literal(tokens.at(cur_index-1), true);
    if(match(FALSE)) return new Literal(tokens.at(cur_index-1), false);
    if(match(NIL)) return new Nil();
    if(match(NUMBER)) return new Literal(tokens.at(cur_index-1), tokens.at(cur_index-1).literal_double);
    if(match(STRING)) return new Literal(tokens.at(cur_index-1), tokens.at(cur_index-1).literal_string);
    if(match(IDENTIFIER)) return new Var(tokens.at(cur_index-1));
    if(match(LEFT_PAREN)) {
        Expr* exp = expression();
        consume(RIGHT_PAREN, "Expected ')' after expression");
        return exp;
    }
    if(match(LEFT_BRACK)) {
        std::vector<Expr*> array_vals;
	Token left_brack_token = tokens.at(cur_index-1);
        while(tokens.at(cur_index).type != END && tokens.at(cur_index).type != RIGHT_BRACK) {
            if(array_vals.size() > 0) {
                consume(COMMA, "Expected ',' between values in array");
            }
	    Expr* expr = expression();
	    array_vals.push_back(expr);
        }
        consume(RIGHT_BRACK, "Expected ']' after array declaration");
	return new Literal(left_brack_token, array_vals);
    }
    throw std::runtime_error(create_error(tokens.at(cur_index), "Expected primary"));
}
