#include "parser.h"
#include "common.h"
#include <stdlib.h>

// Initialize parser
void parser_init(Parser* parser, Lexer* lexer) {
    parser->lexer = lexer;
    parser->had_error = false;
    parser->panic_mode = false;
    
    // Load first token
    parser->current = lexer_next_token(lexer);
    parser->previous = parser->current;
}

// Error reporting
void parser_error(Parser* parser, Token token, const char* message) {
    if (parser->panic_mode) return;
    
    parser->panic_mode = true;
    parser->had_error = true;
    
    fprintf(stderr, "%s[PARSER ERROR]%s Line %d, Column %d: %s\n",
            COLOR_RED, COLOR_RESET,
            token.line, token.column, message);
    
    if (token.kind == TK_EOF) {
        fprintf(stderr, " at end of file\n");
    } else if (token.kind == TK_ERROR) {
        fprintf(stderr, " at token: %s\n", token.start);
    } else {
        fprintf(stderr, " at '%.*s'\n", token.length, token.start);
    }
}

void parser_error_at_current(Parser* parser, const char* message) {
    parser_error(parser, parser->current, message);
}

void parser_error_at_previous(Parser* parser, const char* message) {
    parser_error(parser, parser->previous, message);
}

// Synchronize after error
void parser_synchronize(Parser* parser) {
    parser->panic_mode = false;
    
    while (parser->current.kind != TK_EOF) {
        if (parser->previous.kind == TK_SEMICOLON) return;
        
        switch (parser->current.kind) {
            case TK_FUNC:
            case TK_VAR:
            case TK_NET:
            case TK_CLOG:
            case TK_DOS:
            case TK_SEL:
            case TK_CONST:
            case TK_FOR:
            case TK_IF:
            case TK_WHILE:
            case TK_PRINT:
            case TK_RETURN:
            case TK_IMPORT:
            case TK_CLASS:
            case TK_STRUCT:
                return;
            default:
                break;
        }
        
        parser_advance(parser);
    }
}

// Token consumption
bool parser_match(Parser* parser, TokenKind kind) {
    if (!parser_check(parser, kind)) return false;
    parser_advance(parser);
    return true;
}

bool parser_check(Parser* parser, TokenKind kind) {
    return parser->current.kind == kind;
}

Token parser_consume(Parser* parser, TokenKind kind, const char* error_message) {
    if (parser_check(parser, kind)) {
        return parser_advance(parser);
    }
    
    parser_error_at_current(parser, error_message);
    Token error_token;
    error_token.kind = TK_ERROR;
    return error_token;
}

Token parser_advance(Parser* parser) {
    parser->previous = parser->current;
    
    if (!parser->panic_mode) {
        parser->current = lexer_next_token(parser->lexer);
    }
    
    return parser->previous;
}

// Parse primary expressions
ASTNode* parse_primary(Parser* parser) {
    if (parser_match(parser, TK_TRUE)) {
        return ast_new_bool(true, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_FALSE)) {
        return ast_new_bool(false, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_NULL)) {
        return ast_new_node(NODE_NULL, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_UNDEFINED)) {
        return ast_new_node(NODE_UNDEFINED, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_INT)) {
        return ast_new_int(parser->previous.value.int_val, 
                          parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_FLOAT)) {
        return ast_new_float(parser->previous.value.float_val, 
                            parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_STRING)) {
        return ast_new_string(parser->previous.value.str_val, 
                             parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_IDENT)) {
        char* ident = str_ncopy(parser->previous.start, parser->previous.length);
        return ast_new_identifier(ident, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_LPAREN)) {
        ASTNode* expr = parse_expression(parser);
        parser_consume(parser, TK_RPAREN, "Expect ')' after expression");
        return expr;
    }
    
    if (parser_match(parser, TK_LSQUARE)) {
        // Array literal
        ASTNode* list_node = ast_new_node(NODE_LIST, parser->previous.line, parser->previous.column);
        ASTNode* current = NULL;
        ASTNode** next_ptr = &list_node->left;
        
        if (!parser_check(parser, TK_RSQUARE)) {
            do {
                ASTNode* element = parse_expression(parser);
                *next_ptr = element;
                next_ptr = &element->right;
            } while (parser_match(parser, TK_COMMA));
        }
        
        parser_consume(parser, TK_RSQUARE, "Expect ']' after array literal");
        return list_node;
    }
    
    if (parser_match(parser, TK_LBRACE)) {
        // Map literal
        ASTNode* map_node = ast_new_node(NODE_MAP, parser->previous.line, parser->previous.column);
        ASTNode* current = NULL;
        ASTNode** next_ptr = &map_node->left;
        
        if (!parser_check(parser, TK_RBRACE)) {
            do {
                // Parse key-value pair
                if (parser_match(parser, TK_STRING) || parser_match(parser, TK_IDENT)) {
                    char* key = str_ncopy(parser->previous.start, parser->previous.length);
                    ASTNode* key_node = ast_new_string(key, parser->previous.line, parser->previous.column);
                    free(key);
                    
                    parser_consume(parser, TK_COLON, "Expect ':' after map key");
                    ASTNode* value_node = parse_expression(parser);
                    
                    // Create pair node
                    ASTNode* pair_node = ast_new_node(NODE_BINARY, 
                                                     parser->previous.line, parser->previous.column);
                    pair_node->left = key_node;
                    pair_node->right = value_node;
                    
                    *next_ptr = pair_node;
                    next_ptr = &pair_node->right;
                } else {
                    parser_error_at_current(parser, "Expect string or identifier as map key");
                }
            } while (parser_match(parser, TK_COMMA));
        }
        
        parser_consume(parser, TK_RBRACE, "Expect '}' after map literal");
        return map_node;
    }
    
    parser_error_at_current(parser, "Expect expression");
    return NULL;
}

// Parse function call
ASTNode* parse_call(Parser* parser, ASTNode* callee) {
    ASTNode* args = NULL;
    ASTNode** next_arg = &args;
    
    if (!parser_check(parser, TK_RPAREN)) {
        do {
            ASTNode* arg = parse_expression(parser);
            *next_arg = arg;
            next_arg = &arg->right;
        } while (parser_match(parser, TK_COMMA));
    }
    
    parser_consume(parser, TK_RPAREN, "Expect ')' after arguments");
    
    return ast_new_function_call(callee, args, parser->previous.line, parser->previous.column);
}

// Parse postfix expressions
ASTNode* parse_postfix(Parser* parser) {
    ASTNode* expr = parse_primary(parser);
    
    while (true) {
        if (parser_match(parser, TK_LPAREN)) {
            expr = parse_call(parser, expr);
        } else if (parser_match(parser, TK_PERIOD)) {
            Token name = parser_consume(parser, TK_IDENT, "Expect property name after '.'");
            char* prop_name = str_ncopy(name.start, name.length);
            ASTNode* prop_node = ast_new_identifier(prop_name, name.line, name.column);
            free(prop_name);
            
            ASTNode* access_node = ast_new_node(NODE_MEMBER_ACCESS, 
                                               parser->previous.line, parser->previous.column);
            access_node->left = expr;
            access_node->right = prop_node;
            expr = access_node;
        } else if (parser_match(parser, TK_LSQUARE)) {
            ASTNode* index = parse_expression(parser);
            parser_consume(parser, TK_RSQUARE, "Expect ']' after index");
            
            ASTNode* access_node = ast_new_node(NODE_ARRAY_ACCESS, 
                                               parser->previous.line, parser->previous.column);
            access_node->left = expr;
            access_node->right = index;
            expr = access_node;
        } else {
            break;
        }
    }
    
    return expr;
}

// Parse unary expressions
ASTNode* parse_unary(Parser* parser) {
    if (parser_match(parser, TK_MINUS) || parser_match(parser, TK_NOT) || 
        parser_match(parser, TK_BIT_NOT) || parser_match(parser, TK_INCREMENT) ||
        parser_match(parser, TK_DECREMENT)) {
        Token op = parser->previous;
        ASTNode* right = parse_unary(parser);
        
        ASTNode* node = ast_new_unary(NODE_UNARY, right, op.line, op.column);
        node->op_type = op.kind;
        return node;
    }
    
    return parse_postfix(parser);
}

// Parse multiplicative expressions
ASTNode* parse_multiplication(Parser* parser) {
    ASTNode* expr = parse_unary(parser);
    
    while (parser_match(parser, TK_MULT) || parser_match(parser, TK_DIV) || 
           parser_match(parser, TK_MOD) || parser_match(parser, TK_POW)) {
        Token op = parser->previous;
        ASTNode* right = parse_unary(parser);
        
        ASTNode* node = ast_new_binary(NODE_BINARY, expr, right, op.line, op.column);
        node->op_type = op.kind;
        expr = node;
    }
    
    return expr;
}

// Parse additive expressions
ASTNode* parse_addition(Parser* parser) {
    ASTNode* expr = parse_multiplication(parser);
    
    while (parser_match(parser, TK_PLUS) || parser_match(parser, TK_MINUS) ||
           parser_match(parser, TK_CONCAT)) {
        Token op = parser->previous;
        ASTNode* right = parse_multiplication(parser);
        
        ASTNode* node = ast_new_binary(NODE_BINARY, expr, right, op.line, op.column);
        node->op_type = op.kind;
        expr = node;
    }
    
    return expr;
}

// Parse comparison expressions
ASTNode* parse_comparison(Parser* parser) {
    ASTNode* expr = parse_addition(parser);
    
    while (parser_match(parser, TK_GT) || parser_match(parser, TK_GTE) ||
           parser_match(parser, TK_LT) || parser_match(parser, TK_LTE) ||
           parser_match(parser, TK_EQ) || parser_match(parser, TK_NEQ) ||
           parser_match(parser, TK_IS) || parser_match(parser, TK_ISNOT) ||
           parser_match(parser, TK_IN)) {
        Token op = parser->previous;
        ASTNode* right = parse_addition(parser);
        
        ASTNode* node = ast_new_binary(NODE_BINARY, expr, right, op.line, op.column);
        node->op_type = op.kind;
        expr = node;
    }
    
    return expr;
}

// Parse logical AND
ASTNode* parse_and(Parser* parser) {
    ASTNode* expr = parse_comparison(parser);
    
    while (parser_match(parser, TK_AND)) {
        Token op = parser->previous;
        ASTNode* right = parse_comparison(parser);
        
        ASTNode* node = ast_new_binary(NODE_BINARY, expr, right, op.line, op.column);
        node->op_type = op.kind;
        expr = node;
    }
    
    return expr;
}

// Parse logical OR
ASTNode* parse_or(Parser* parser) {
    ASTNode* expr = parse_and(parser);
    
    while (parser_match(parser, TK_OR)) {
        Token op = parser->previous;
        ASTNode* right = parse_and(parser);
        
        ASTNode* node = ast_new_binary(NODE_BINARY, expr, right, op.line, op.column);
        node->op_type = op.kind;
        expr = node;
    }
    
    return expr;
}

// Parse ternary conditional
ASTNode* parse_ternary(Parser* parser) {
    ASTNode* expr = parse_or(parser);
    
    if (parser_match(parser, TK_QUESTION)) {
        ASTNode* then_branch = parse_expression(parser);
        parser_consume(parser, TK_COLON, "Expect ':' in ternary operator");
        ASTNode* else_branch = parse_ternary(parser);
        
        ASTNode* node = ast_new_node(NODE_TERNARY, parser->previous.line, parser->previous.column);
        node->left = expr;          // Condition
        node->right = then_branch;  // Then branch
        node->third = else_branch;  // Else branch
        expr = node;
    }
    
    return expr;
}

// Parse assignment
ASTNode* parse_assignment(Parser* parser) {
    ASTNode* expr = parse_ternary(parser);
    
    if (parser_match(parser, TK_ASSIGN) || 
        parser_match(parser, TK_PLUS_ASSIGN) ||
        parser_match(parser, TK_MINUS_ASSIGN) ||
        parser_match(parser, TK_MULT_ASSIGN) ||
        parser_match(parser, TK_DIV_ASSIGN) ||
        parser_match(parser, TK_MOD_ASSIGN) ||
        parser_match(parser, TK_POW_ASSIGN) ||
        parser_match(parser, TK_CONCAT_ASSIGN)) {
        
        Token op = parser->previous;
        ASTNode* value = parse_assignment(parser);
        
        if (op.kind == TK_ASSIGN) {
            ASTNode* node = ast_new_assignment(expr, value, op.line, op.column);
            expr = node;
        } else {
            ASTNode* node = ast_new_node(NODE_COMPOUND_ASSIGN, op.line, op.column);
            node->left = expr;
            node->right = value;
            node->op_type = op.kind;
            expr = node;
        }
    }
    
    return expr;
}

// Main expression parser
ASTNode* parse_expression(Parser* parser) {
    return parse_assignment(parser);
}

// Parse variable declaration
ASTNode* parse_var_declaration(Parser* parser) {
    Token var_type = parser->previous; // Should be VAR, NET, CLOG, DOS, SEL, CONST, GLOBAL
    
    Token name = parser_consume(parser, TK_IDENT, "Expect variable name");
    char* var_name = str_ncopy(name.start, name.length);
    
    ASTNode* initializer = NULL;
    if (parser_match(parser, TK_ASSIGN)) {
        initializer = parse_expression(parser);
    }
    
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after variable declaration");
    
    return ast_new_var_decl(var_name, initializer, var_type.kind, name.line, name.column);
}

// Parse print statement
ASTNode* parse_print_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* value = parse_expression(parser);
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after print statement");
    
    return ast_new_print(value, line, column);
}

// Parse input statement
ASTNode* parse_input_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    const char* prompt = NULL;
    if (parser_match(parser, TK_STRING)) {
        prompt = parser->previous.value.str_val;
    }
    
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after input statement");
    
    return ast_new_input(prompt, line, column);
}

// Parse if statement
ASTNode* parse_if_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_LSQUARE, "Expect '[' after 'if'");
    ASTNode* condition = parse_expression(parser);
    parser_consume(parser, TK_RSQUARE, "Expect ']' after condition");
    
    ASTNode* then_branch = parse_statement(parser);
    ASTNode* else_branch = NULL;
    
    if (parser_match(parser, TK_ELSE)) {
        else_branch = parse_statement(parser);
    } else if (parser_match(parser, TK_ELIF)) {
        // Handle elif chain
        else_branch = parse_if_statement(parser);
    }
    
    return ast_new_if(condition, then_branch, else_branch, line, column);
}

// Parse while statement
ASTNode* parse_while_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_LSQUARE, "Expect '[' after 'while'");
    ASTNode* condition = parse_expression(parser);
    parser_consume(parser, TK_RSQUARE, "Expect ']' after condition");
    
    ASTNode* body = parse_statement(parser);
    
    return ast_new_while(condition, body, line, column);
}

// Parse for statement
ASTNode* parse_for_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_LSQUARE, "Expect '[' after 'for'");
    
    // Parse initialization
    ASTNode* init = NULL;
    if (!parser_match(parser, TK_SEMICOLON)) {
        if (parser_match(parser, TK_VAR) || parser_match(parser, TK_NET) || 
            parser_match(parser, TK_CLOG) || parser_match(parser, TK_DOS) ||
            parser_match(parser, TK_SEL) || parser_match(parser, TK_CONST)) {
            init = parse_var_declaration(parser);
            // var_declaration already consumes semicolon
        } else {
            init = parse_expression(parser);
            parser_consume(parser, TK_SEMICOLON, "Expect ';' after loop initialization");
        }
    }
    
    // Parse condition
    ASTNode* condition = NULL;
    if (!parser_check(parser, TK_SEMICOLON)) {
        condition = parse_expression(parser);
    }
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after loop condition");
    
    // Parse increment
    ASTNode* increment = NULL;
    if (!parser_check(parser, TK_RSQUARE)) {
        increment = parse_expression(parser);
    }
    parser_consume(parser, TK_RSQUARE, "Expect ']' after for clause");
    
    ASTNode* body = parse_statement(parser);
    
    return ast_new_for(init, condition, increment, body, line, column);
}

// Parse return statement
ASTNode* parse_return_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* value = NULL;
    if (!parser_check(parser, TK_SEMICOLON)) {
        value = parse_expression(parser);
    }
    
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after return value");
    
    return ast_new_return(value, line, column);
}

// Parse break statement
ASTNode* parse_break_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after break");
    
    return ast_new_node(NODE_BREAK, line, column);
}

// Parse continue statement
ASTNode* parse_continue_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after continue");
    
    return ast_new_node(NODE_CONTINUE, line, column);
}

// Parse import statement
ASTNode* parse_import_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    // Parse what to import
    char** modules = NULL;
    int module_count = 0;
    char* from_module = NULL;
    
    if (parser_match(parser, TK_STRING)) {
        // Single module import: import "module"
        module_count = 1;
        modules = malloc(sizeof(char*));
        modules[0] = str_copy(parser->previous.value.str_val);
        
        if (parser_match(parser, TK_FROM)) {
            parser_consume(parser, TK_STRING, "Expect module name after 'from'");
            from_module = str_copy(parser->previous.value.str_val);
        }
    } else {
        // Selective import: import func1, func2 from "module"
        do {
            Token module = parser_consume(parser, TK_IDENT, "Expect identifier in import list");
            module_count++;
            modules = realloc(modules, module_count * sizeof(char*));
            modules[module_count - 1] = str_ncopy(module.start, module.length);
        } while (parser_match(parser, TK_COMMA));
        
        parser_consume(parser, TK_FROM, "Expect 'from' after import list");
        parser_consume(parser, TK_STRING, "Expect module name after 'from'");
        from_module = str_copy(parser->previous.value.str_val);
    }
    
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after import statement");
    
    return ast_new_import(modules, module_count, from_module, line, column);
}

// Parse function declaration
ASTNode* parse_function_declaration(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    Token name = parser_consume(parser, TK_IDENT, "Expect function name");
    char* func_name = str_ncopy(name.start, name.length);
    
    // Parse parameters
    parser_consume(parser, TK_LPAREN, "Expect '(' after function name");
    
    ASTNode* params = NULL;
    ASTNode** next_param = &params;
    
    if (!parser_check(parser, TK_RPAREN)) {
        do {
            Token param = parser_consume(parser, TK_IDENT, "Expect parameter name");
            char* param_name = str_ncopy(param.start, param.length);
            ASTNode* param_node = ast_new_identifier(param_name, param.line, param.column);
            free(param_name);
            
            *next_param = param_node;
            next_param = &param_node->right;
        } while (parser_match(parser, TK_COMMA));
    }
    
    parser_consume(parser, TK_RPAREN, "Expect ')' after parameters");
    
    // Parse function body
    ASTNode* body = parse_block(parser);
    
    return ast_new_function(func_name, params, body, line, column);
}

// Parse block statement
ASTNode* parse_block(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_LBRACE, "Expect '{' before block");
    
    ASTNode* block_node = ast_new_node(NODE_BLOCK, line, column);
    ASTNode* current = NULL;
    ASTNode** next_stmt = &block_node->left;
    
    while (!parser_check(parser, TK_RBRACE) && !parser_check(parser, TK_EOF)) {
        ASTNode* stmt = parse_statement(parser);
        *next_stmt = stmt;
        next_stmt = &stmt->right;
    }
    
    parser_consume(parser, TK_RBRACE, "Expect '}' after block");
    
    return block_node;
}

// Parse expression statement
ASTNode* parse_expression_statement(Parser* parser) {
    ASTNode* expr = parse_expression(parser);
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after expression");
    return expr;
}

// Parse pass statement (do nothing)
ASTNode* parse_pass_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_SEMICOLON, "Expect ';' after pass");
    
    return ast_new_node(NODE_PASS, line, column);
}

// Parse a single statement
ASTNode* parse_statement(Parser* parser) {
    if (parser_match(parser, TK_PRINT)) {
        return parse_print_statement(parser);
    }
    
    if (parser_match(parser, TK_INPUT)) {
        return parse_input_statement(parser);
    }
    
    if (parser_match(parser, TK_IF)) {
        return parse_if_statement(parser);
    }
    
    if (parser_match(parser, TK_WHILE)) {
        return parse_while_statement(parser);
    }
    
    if (parser_match(parser, TK_FOR)) {
        return parse_for_statement(parser);
    }
    
    if (parser_match(parser, TK_RETURN)) {
        return parse_return_statement(parser);
    }
    
    if (parser_match(parser, TK_BREAK)) {
        return parse_break_statement(parser);
    }
    
    if (parser_match(parser, TK_CONTINUE)) {
        return parse_continue_statement(parser);
    }
    
    if (parser_match(parser, TK_IMPORT)) {
        return parse_import_statement(parser);
    }
    
    if (parser_match(parser, TK_FUNC)) {
        return parse_function_declaration(parser);
    }
    
    if (parser_match(parser, TK_PASS)) {
        return parse_pass_statement(parser);
    }
    
    if (parser_match(parser, TK_LBRACE)) {
        return parse_block(parser);
    }
    
    // Variable declarations
    if (parser_match(parser, TK_VAR) || parser_match(parser, TK_NET) ||
        parser_match(parser, TK_CLOG) || parser_match(parser, TK_DOS) ||
        parser_match(parser, TK_SEL) || parser_match(parser, TK_CONST) ||
        parser_match(parser, TK_GLOBAL)) {
        return parse_var_declaration(parser);
    }
    
    // Expression statement as fallback
    return parse_expression_statement(parser);
}

// Parse entire program
ASTNode* parse_program(Parser* parser) {
    ASTNode* program_node = ast_new_node(NODE_PROGRAM, 1, 1);
    ASTNode* current = NULL;
    ASTNode** next_stmt = &program_node->left;
    
    while (!parser_check(parser, TK_EOF)) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            *next_stmt = stmt;
            next_stmt = &stmt->right;
        }
        
        if (parser->panic_mode) {
            parser_synchronize(parser);
        }
    }
    
    return program_node;
}
