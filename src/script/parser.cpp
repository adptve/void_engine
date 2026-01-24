#include "parser.hpp"

namespace void_script {

// =============================================================================
// Parser Implementation
// =============================================================================

Parser::Parser(std::string_view source, std::string_view filename)
    : lexer_(source, filename) {
    advance();
}

Parser::Parser(Lexer lexer)
    : lexer_(std::move(lexer)) {
    advance();
}

std::unique_ptr<Program> Parser::parse_program() {
    auto program = std::make_unique<Program>();

    while (!is_at_end()) {
        if (auto stmt = parse_declaration()) {
            program->statements.push_back(std::move(stmt));
        }
    }

    return program;
}

StmtPtr Parser::parse_statement() {
    return parse_declaration();
}

ExprPtr Parser::parse_expression() {
    return parse_precedence(0);
}

bool Parser::check(TokenType type) const {
    return current_.type == type;
}

bool Parser::is_at_end() const {
    return current_.type == TokenType::Eof;
}

Token Parser::advance() {
    previous_ = current_;
    current_ = lexer_.next_token();

    while (current_.type == TokenType::Error) {
        error(current_, current_.string_value);
        current_ = lexer_.next_token();
    }

    return previous_;
}

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    error(current_, message);
    return current_;
}

void Parser::error(const std::string& message) {
    error(current_, message);
}

void Parser::error(const Token& token, const std::string& message) {
    if (panic_mode_) return;
    panic_mode_ = true;

    errors_.emplace_back(ScriptError::UnexpectedToken, message, token.location);
}

void Parser::synchronize() {
    panic_mode_ = false;

    while (!is_at_end()) {
        if (previous_.type == TokenType::Semicolon) return;

        switch (current_.type) {
            case TokenType::Class:
            case TokenType::Fn:
            case TokenType::Let:
            case TokenType::Const:
            case TokenType::For:
            case TokenType::If:
            case TokenType::While:
            case TokenType::Return:
            case TokenType::Import:
            case TokenType::Export:
                return;
            default:
                break;
        }

        advance();
    }
}

// =============================================================================
// Precedence
// =============================================================================

int Parser::get_precedence(TokenType type) {
    switch (type) {
        case TokenType::Assign:
        case TokenType::PlusAssign:
        case TokenType::MinusAssign:
        case TokenType::StarAssign:
        case TokenType::SlashAssign:
        case TokenType::PercentAssign:
            return static_cast<int>(Precedence::Assignment);

        case TokenType::Question:
            return static_cast<int>(Precedence::Ternary);

        case TokenType::QuestionQuestion:
            return static_cast<int>(Precedence::NullCoalesce);

        case TokenType::Or:
            return static_cast<int>(Precedence::Or);

        case TokenType::And:
            return static_cast<int>(Precedence::And);

        case TokenType::Pipe:
            return static_cast<int>(Precedence::BitwiseOr);

        case TokenType::Caret:
            return static_cast<int>(Precedence::BitwiseXor);

        case TokenType::Ampersand:
            return static_cast<int>(Precedence::BitwiseAnd);

        case TokenType::Equal:
        case TokenType::NotEqual:
            return static_cast<int>(Precedence::Equality);

        case TokenType::Less:
        case TokenType::LessEqual:
        case TokenType::Greater:
        case TokenType::GreaterEqual:
        case TokenType::Spaceship:
            return static_cast<int>(Precedence::Comparison);

        case TokenType::ShiftLeft:
        case TokenType::ShiftRight:
            return static_cast<int>(Precedence::Shift);

        case TokenType::DotDot:
            return static_cast<int>(Precedence::Range);

        case TokenType::Plus:
        case TokenType::Minus:
            return static_cast<int>(Precedence::Term);

        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:
            return static_cast<int>(Precedence::Factor);

        case TokenType::Power:
            return static_cast<int>(Precedence::Power);

        case TokenType::LeftParen:
        case TokenType::LeftBracket:
        case TokenType::Dot:
        case TokenType::QuestionDot:
            return static_cast<int>(Precedence::Call);

        default:
            return 0;
    }
}

bool Parser::is_right_associative(TokenType type) {
    return type == TokenType::Assign || type == TokenType::Power ||
           type == TokenType::Question;
}

// =============================================================================
// Expression Parsing
// =============================================================================

ExprPtr Parser::parse_precedence(int precedence) {
    ExprPtr left = parse_prefix();
    if (!left) return nullptr;

    while (precedence < get_precedence(current_.type)) {
        left = parse_infix(std::move(left), get_precedence(current_.type));
        if (!left) return nullptr;
    }

    return left;
}

ExprPtr Parser::parse_prefix() {
    switch (current_.type) {
        case TokenType::Integer:
        case TokenType::Float:
        case TokenType::String:
        case TokenType::True:
        case TokenType::False:
        case TokenType::Null:
            return parse_literal();

        case TokenType::Identifier:
            return parse_identifier();

        case TokenType::LeftParen:
            return parse_grouping();

        case TokenType::LeftBracket:
            return parse_array();

        case TokenType::LeftBrace:
            return parse_map();

        case TokenType::Minus:
        case TokenType::Not:
        case TokenType::Tilde:
        case TokenType::Increment:
        case TokenType::Decrement:
            return parse_unary();

        case TokenType::Fn:
            return parse_lambda();

        case TokenType::New:
            return parse_new();

        case TokenType::This:
            return parse_this();

        case TokenType::Super:
            return parse_super();

        case TokenType::Await:
            return parse_await();

        case TokenType::Yield:
            return parse_yield();

        default:
            error("Expected expression");
            return nullptr;
    }
}

ExprPtr Parser::parse_infix(ExprPtr left, int precedence) {
    switch (current_.type) {
        case TokenType::Plus:
        case TokenType::Minus:
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:
        case TokenType::Power:
        case TokenType::Equal:
        case TokenType::NotEqual:
        case TokenType::Less:
        case TokenType::LessEqual:
        case TokenType::Greater:
        case TokenType::GreaterEqual:
        case TokenType::Spaceship:
        case TokenType::And:
        case TokenType::Or:
        case TokenType::Ampersand:
        case TokenType::Pipe:
        case TokenType::Caret:
        case TokenType::ShiftLeft:
        case TokenType::ShiftRight:
        case TokenType::QuestionQuestion:
            return parse_binary(std::move(left));

        case TokenType::LeftParen:
            return parse_call(std::move(left));

        case TokenType::Dot:
        case TokenType::QuestionDot:
            return parse_member(std::move(left));

        case TokenType::LeftBracket:
            return parse_index(std::move(left));

        case TokenType::Assign:
        case TokenType::PlusAssign:
        case TokenType::MinusAssign:
        case TokenType::StarAssign:
        case TokenType::SlashAssign:
        case TokenType::PercentAssign:
            return parse_assignment(std::move(left));

        case TokenType::Question:
            return parse_ternary(std::move(left));

        default:
            return left;
    }
}

ExprPtr Parser::parse_literal() {
    Token tok = advance();
    Value value;

    switch (tok.type) {
        case TokenType::Integer:
            value = Value(tok.int_value);
            break;
        case TokenType::Float:
            value = Value(tok.float_value);
            break;
        case TokenType::String:
            value = Value(tok.string_value);
            break;
        case TokenType::True:
            value = Value(true);
            break;
        case TokenType::False:
            value = Value(false);
            break;
        case TokenType::Null:
            value = Value(nullptr);
            break;
        default:
            error("Expected literal");
            return nullptr;
    }

    return std::make_unique<LiteralExpr>(std::move(value));
}

ExprPtr Parser::parse_identifier() {
    Token tok = advance();
    return std::make_unique<IdentifierExpr>(std::string(tok.lexeme));
}

ExprPtr Parser::parse_grouping() {
    consume(TokenType::LeftParen, "Expected '('");
    ExprPtr expr = parse_expression();
    consume(TokenType::RightParen, "Expected ')'");
    return expr;
}

ExprPtr Parser::parse_array() {
    consume(TokenType::LeftBracket, "Expected '['");

    std::vector<ExprPtr> elements;

    if (!check(TokenType::RightBracket)) {
        do {
            elements.push_back(parse_expression());
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightBracket, "Expected ']'");
    return std::make_unique<ArrayExpr>(std::move(elements));
}

ExprPtr Parser::parse_map() {
    consume(TokenType::LeftBrace, "Expected '{'");

    std::vector<MapExpr::Entry> entries;

    if (!check(TokenType::RightBrace)) {
        do {
            ExprPtr key;
            if (check(TokenType::Identifier)) {
                // Shorthand: { name } becomes { name: name }
                Token tok = advance();
                key = std::make_unique<LiteralExpr>(Value(std::string(tok.lexeme)));

                if (!check(TokenType::Colon)) {
                    ExprPtr value = std::make_unique<IdentifierExpr>(std::string(tok.lexeme));
                    entries.push_back({std::move(key), std::move(value)});
                    continue;
                }
            } else if (check(TokenType::LeftBracket)) {
                // Computed key: { [expr]: value }
                advance();
                key = parse_expression();
                consume(TokenType::RightBracket, "Expected ']'");
            } else {
                key = parse_expression();
            }

            consume(TokenType::Colon, "Expected ':' after map key");
            ExprPtr value = parse_expression();
            entries.push_back({std::move(key), std::move(value)});
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightBrace, "Expected '}'");
    return std::make_unique<MapExpr>(std::move(entries));
}

ExprPtr Parser::parse_unary() {
    Token op = advance();
    ExprPtr operand = parse_precedence(static_cast<int>(Precedence::Unary));
    return std::make_unique<UnaryExpr>(op.type, std::move(operand), true);
}

ExprPtr Parser::parse_binary(ExprPtr left) {
    Token op = advance();
    int prec = get_precedence(op.type);
    if (is_right_associative(op.type)) --prec;

    ExprPtr right = parse_precedence(prec);
    return std::make_unique<BinaryExpr>(op.type, std::move(left), std::move(right));
}

ExprPtr Parser::parse_call(ExprPtr callee) {
    consume(TokenType::LeftParen, "Expected '('");

    std::vector<ExprPtr> args;
    if (!check(TokenType::RightParen)) {
        do {
            if (args.size() >= 255) {
                error("Too many arguments");
            }
            args.push_back(parse_expression());
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "Expected ')'");
    return std::make_unique<CallExpr>(std::move(callee), std::move(args));
}

ExprPtr Parser::parse_member(ExprPtr object) {
    bool optional = current_.type == TokenType::QuestionDot;
    advance();  // . or ?.

    Token name = consume(TokenType::Identifier, "Expected property name");
    return std::make_unique<MemberExpr>(std::move(object), std::string(name.lexeme), optional);
}

ExprPtr Parser::parse_index(ExprPtr object) {
    consume(TokenType::LeftBracket, "Expected '['");
    ExprPtr index = parse_expression();
    consume(TokenType::RightBracket, "Expected ']'");
    return std::make_unique<IndexExpr>(std::move(object), std::move(index));
}

ExprPtr Parser::parse_assignment(ExprPtr target) {
    Token op = advance();
    ExprPtr value = parse_precedence(static_cast<int>(Precedence::Assignment) - 1);
    return std::make_unique<AssignExpr>(op.type, std::move(target), std::move(value));
}

ExprPtr Parser::parse_ternary(ExprPtr condition) {
    consume(TokenType::Question, "Expected '?'");
    ExprPtr then_expr = parse_expression();
    consume(TokenType::Colon, "Expected ':'");
    ExprPtr else_expr = parse_expression();
    return std::make_unique<TernaryExpr>(std::move(condition), std::move(then_expr),
                                          std::move(else_expr));
}

ExprPtr Parser::parse_lambda() {
    consume(TokenType::Fn, "Expected 'fn'");
    consume(TokenType::LeftParen, "Expected '('");

    std::vector<LambdaExpr::Parameter> params;
    if (!check(TokenType::RightParen)) {
        do {
            LambdaExpr::Parameter param;
            Token name = consume(TokenType::Identifier, "Expected parameter name");
            param.name = std::string(name.lexeme);
            param.type = parse_type_annotation();

            if (match(TokenType::Assign)) {
                param.default_value = parse_expression();
            }

            params.push_back(std::move(param));
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "Expected ')'");

    std::optional<std::string> return_type;
    if (match(TokenType::Arrow)) {
        return_type = parse_type_annotation();
    }

    StmtPtr body;
    if (match(TokenType::FatArrow)) {
        // Expression body
        ExprPtr expr = parse_expression();
        body = std::make_unique<ReturnStatement>(std::move(expr));
    } else {
        body = parse_block_statement();
    }

    auto lambda = std::make_unique<LambdaExpr>(std::move(params), std::move(body));
    lambda->return_type = std::move(return_type);
    return lambda;
}

ExprPtr Parser::parse_new() {
    consume(TokenType::New, "Expected 'new'");
    ExprPtr class_expr = parse_precedence(static_cast<int>(Precedence::Call));

    std::vector<ExprPtr> args;
    if (match(TokenType::LeftParen)) {
        if (!check(TokenType::RightParen)) {
            do {
                args.push_back(parse_expression());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RightParen, "Expected ')'");
    }

    return std::make_unique<NewExpr>(std::move(class_expr), std::move(args));
}

ExprPtr Parser::parse_this() {
    consume(TokenType::This, "Expected 'this'");
    return std::make_unique<ThisExpr>();
}

ExprPtr Parser::parse_super() {
    consume(TokenType::Super, "Expected 'super'");
    auto expr = std::make_unique<SuperExpr>();
    if (match(TokenType::Dot)) {
        Token method = consume(TokenType::Identifier, "Expected method name");
        expr->method = std::string(method.lexeme);
    }
    return expr;
}

ExprPtr Parser::parse_await() {
    consume(TokenType::Await, "Expected 'await'");
    ExprPtr operand = parse_precedence(static_cast<int>(Precedence::Unary));
    return std::make_unique<AwaitExpr>(std::move(operand));
}

ExprPtr Parser::parse_yield() {
    consume(TokenType::Yield, "Expected 'yield'");
    bool delegate = match(TokenType::Star);
    ExprPtr value = parse_expression();
    return std::make_unique<YieldExpr>(std::move(value), delegate);
}

// =============================================================================
// Statement Parsing
// =============================================================================

StmtPtr Parser::parse_declaration() {
    try {
        if (match(TokenType::Let) || match(TokenType::Const) || match(TokenType::Var)) {
            return parse_var_declaration();
        }
        if (match(TokenType::Fn)) {
            return parse_function_declaration();
        }
        if (match(TokenType::Class)) {
            return parse_class_declaration();
        }
        if (match(TokenType::Import)) {
            return parse_import_declaration();
        }
        if (match(TokenType::Export)) {
            return parse_export_declaration();
        }
        if (match(TokenType::Module)) {
            return parse_module_declaration();
        }

        return parse_simple_statement();
    } catch (const ScriptException& e) {
        synchronize();
        return nullptr;
    }
}

StmtPtr Parser::parse_var_declaration() {
    bool is_const = previous_.type == TokenType::Const;

    Token name = consume(TokenType::Identifier, "Expected variable name");
    auto type = parse_type_annotation();

    ExprPtr initializer;
    if (match(TokenType::Assign)) {
        initializer = parse_expression();
    } else if (is_const) {
        error("Const declaration must have initializer");
    }

    consume(TokenType::Semicolon, "Expected ';' after variable declaration");

    auto decl = std::make_unique<VarDecl>(std::string(name.lexeme), std::move(initializer), is_const);
    decl->type = std::move(type);
    return decl;
}

StmtPtr Parser::parse_function_declaration() {
    Token name = consume(TokenType::Identifier, "Expected function name");
    auto params = parse_parameters();
    auto return_type = parse_type_annotation();
    auto body = parse_block_statement();

    auto decl = std::make_unique<FunctionDecl>(std::string(name.lexeme), std::move(params),
                                                 std::move(body));
    decl->return_type = std::move(return_type);
    return decl;
}

StmtPtr Parser::parse_class_declaration() {
    Token name = consume(TokenType::Identifier, "Expected class name");
    auto decl = std::make_unique<ClassDecl>(std::string(name.lexeme));

    // Superclass
    if (match(TokenType::Colon)) {
        Token super = consume(TokenType::Identifier, "Expected superclass name");
        decl->superclass = std::string(super.lexeme);
    }

    consume(TokenType::LeftBrace, "Expected '{'");

    while (!check(TokenType::RightBrace) && !is_at_end()) {
        bool is_pub = match(TokenType::Pub);
        bool is_static = match(TokenType::Static);

        if (check(TokenType::Fn)) {
            advance();
            auto func = parse_function_declaration();
            ClassDecl::Method method;
            method.func = std::unique_ptr<FunctionDecl>(
                static_cast<FunctionDecl*>(func.release()));
            method.is_pub = is_pub;
            method.is_static = is_static;
            decl->methods.push_back(std::move(method));
        } else {
            // Member variable
            Token member_name = consume(TokenType::Identifier, "Expected member name");
            ClassDecl::Member member;
            member.name = std::string(member_name.lexeme);
            member.type = parse_type_annotation();
            member.is_pub = is_pub;
            member.is_static = is_static;

            if (match(TokenType::Assign)) {
                member.default_value = parse_expression();
            }

            consume(TokenType::Semicolon, "Expected ';'");
            decl->members.push_back(std::move(member));
        }
    }

    consume(TokenType::RightBrace, "Expected '}'");
    return decl;
}

StmtPtr Parser::parse_import_declaration() {
    auto decl = std::make_unique<ImportDecl>();

    if (match(TokenType::Star)) {
        consume(TokenType::As, "Expected 'as'");
        Token alias = consume(TokenType::Identifier, "Expected alias");
        decl->alias = std::string(alias.lexeme);
        decl->import_all = true;
    } else if (match(TokenType::LeftBrace)) {
        do {
            ImportDecl::ImportItem item;
            Token name = consume(TokenType::Identifier, "Expected import name");
            item.name = std::string(name.lexeme);

            if (match(TokenType::As)) {
                Token alias = consume(TokenType::Identifier, "Expected alias");
                item.alias = std::string(alias.lexeme);
            }

            decl->items.push_back(std::move(item));
        } while (match(TokenType::Comma));

        consume(TokenType::RightBrace, "Expected '}'");
    } else {
        Token name = consume(TokenType::Identifier, "Expected module name");
        decl->module_path = std::string(name.lexeme);
    }

    consume(TokenType::From, "Expected 'from'");
    Token path = consume(TokenType::String, "Expected module path");
    decl->module_path = path.string_value;

    consume(TokenType::Semicolon, "Expected ';'");
    return decl;
}

StmtPtr Parser::parse_export_declaration() {
    auto decl = std::make_unique<ExportDecl>();
    decl->declaration = parse_declaration();
    return decl;
}

StmtPtr Parser::parse_module_declaration() {
    Token name = consume(TokenType::Identifier, "Expected module name");
    consume(TokenType::LeftBrace, "Expected '{'");

    std::vector<StmtPtr> statements;
    while (!check(TokenType::RightBrace) && !is_at_end()) {
        if (auto stmt = parse_declaration()) {
            statements.push_back(std::move(stmt));
        }
    }

    consume(TokenType::RightBrace, "Expected '}'");

    return std::make_unique<ModuleDecl>(std::string(name.lexeme), std::move(statements));
}

StmtPtr Parser::parse_simple_statement() {
    if (match(TokenType::LeftBrace)) {
        return parse_block_statement();
    }
    if (match(TokenType::If)) {
        return parse_if_statement();
    }
    if (match(TokenType::While)) {
        return parse_while_statement();
    }
    if (match(TokenType::For)) {
        return parse_for_statement();
    }
    if (match(TokenType::Return)) {
        return parse_return_statement();
    }
    if (match(TokenType::Break)) {
        return parse_break_statement();
    }
    if (match(TokenType::Continue)) {
        return parse_continue_statement();
    }
    if (match(TokenType::Match)) {
        return parse_match_statement();
    }
    if (match(TokenType::Try)) {
        return parse_try_statement();
    }
    if (match(TokenType::Throw)) {
        return parse_throw_statement();
    }

    return parse_expression_statement();
}

StmtPtr Parser::parse_expression_statement() {
    ExprPtr expr = parse_expression();
    consume(TokenType::Semicolon, "Expected ';' after expression");
    return std::make_unique<ExprStatement>(std::move(expr));
}

StmtPtr Parser::parse_block_statement() {
    std::vector<StmtPtr> statements;

    while (!check(TokenType::RightBrace) && !is_at_end()) {
        if (auto stmt = parse_declaration()) {
            statements.push_back(std::move(stmt));
        }
    }

    consume(TokenType::RightBrace, "Expected '}'");
    return std::make_unique<BlockStatement>(std::move(statements));
}

StmtPtr Parser::parse_if_statement() {
    consume(TokenType::LeftParen, "Expected '(' after 'if'");
    ExprPtr condition = parse_expression();
    consume(TokenType::RightParen, "Expected ')' after condition");

    StmtPtr then_branch = parse_simple_statement();
    StmtPtr else_branch;

    if (match(TokenType::Else)) {
        else_branch = parse_simple_statement();
    }

    return std::make_unique<IfStatement>(std::move(condition), std::move(then_branch),
                                          std::move(else_branch));
}

StmtPtr Parser::parse_while_statement() {
    consume(TokenType::LeftParen, "Expected '(' after 'while'");
    ExprPtr condition = parse_expression();
    consume(TokenType::RightParen, "Expected ')' after condition");

    StmtPtr body = parse_simple_statement();

    return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
}

StmtPtr Parser::parse_for_statement() {
    consume(TokenType::LeftParen, "Expected '(' after 'for'");

    // Check for for-each
    if (check(TokenType::Identifier)) {
        Token var = advance();
        if (match(TokenType::In)) {
            ExprPtr iterable = parse_expression();
            consume(TokenType::RightParen, "Expected ')'");
            StmtPtr body = parse_simple_statement();
            return std::make_unique<ForEachStatement>(std::string(var.lexeme),
                                                        std::move(iterable), std::move(body));
        }
        // Put token back (not a for-each)
        // This is a simplification; proper implementation would use lookahead
    }

    // Regular for loop
    StmtPtr initializer;
    if (!match(TokenType::Semicolon)) {
        if (match(TokenType::Let) || match(TokenType::Var)) {
            initializer = parse_var_declaration();
        } else {
            initializer = parse_expression_statement();
        }
    }

    ExprPtr condition;
    if (!check(TokenType::Semicolon)) {
        condition = parse_expression();
    }
    consume(TokenType::Semicolon, "Expected ';' after loop condition");

    ExprPtr increment;
    if (!check(TokenType::RightParen)) {
        increment = parse_expression();
    }
    consume(TokenType::RightParen, "Expected ')' after for clauses");

    StmtPtr body = parse_simple_statement();

    return std::make_unique<ForStatement>(std::move(initializer), std::move(condition),
                                           std::move(increment), std::move(body));
}

StmtPtr Parser::parse_return_statement() {
    ExprPtr value;
    if (!check(TokenType::Semicolon)) {
        value = parse_expression();
    }
    consume(TokenType::Semicolon, "Expected ';' after return");
    return std::make_unique<ReturnStatement>(std::move(value));
}

StmtPtr Parser::parse_break_statement() {
    auto stmt = std::make_unique<BreakStatement>();
    if (check(TokenType::Identifier)) {
        stmt->label = std::string(advance().lexeme);
    }
    consume(TokenType::Semicolon, "Expected ';' after break");
    return stmt;
}

StmtPtr Parser::parse_continue_statement() {
    auto stmt = std::make_unique<ContinueStatement>();
    if (check(TokenType::Identifier)) {
        stmt->label = std::string(advance().lexeme);
    }
    consume(TokenType::Semicolon, "Expected ';' after continue");
    return stmt;
}

StmtPtr Parser::parse_match_statement() {
    consume(TokenType::LeftParen, "Expected '(' after 'match'");
    ExprPtr subject = parse_expression();
    consume(TokenType::RightParen, "Expected ')'");

    consume(TokenType::LeftBrace, "Expected '{'");

    std::vector<MatchStatement::Arm> arms;
    while (!check(TokenType::RightBrace) && !is_at_end()) {
        MatchStatement::Arm arm;
        arm.pattern = parse_expression();

        if (match(TokenType::If)) {
            arm.guard = parse_expression();
        }

        consume(TokenType::FatArrow, "Expected '=>'");
        arm.body = parse_simple_statement();
        arms.push_back(std::move(arm));
    }

    consume(TokenType::RightBrace, "Expected '}'");
    return std::make_unique<MatchStatement>(std::move(subject), std::move(arms));
}

StmtPtr Parser::parse_try_statement() {
    consume(TokenType::LeftBrace, "Expected '{'");
    StmtPtr try_block = parse_block_statement();

    std::vector<TryCatchStatement::CatchClause> catches;
    while (match(TokenType::Catch)) {
        TryCatchStatement::CatchClause clause;
        consume(TokenType::LeftParen, "Expected '('");
        Token var = consume(TokenType::Identifier, "Expected variable name");
        clause.variable = std::string(var.lexeme);
        clause.type = parse_type_annotation();
        consume(TokenType::RightParen, "Expected ')'");
        consume(TokenType::LeftBrace, "Expected '{'");
        clause.body = parse_block_statement();
        catches.push_back(std::move(clause));
    }

    StmtPtr finally_block;
    if (match(TokenType::Finally)) {
        consume(TokenType::LeftBrace, "Expected '{'");
        finally_block = parse_block_statement();
    }

    return std::make_unique<TryCatchStatement>(std::move(try_block), std::move(catches),
                                                 std::move(finally_block));
}

StmtPtr Parser::parse_throw_statement() {
    ExprPtr value = parse_expression();
    consume(TokenType::Semicolon, "Expected ';' after throw");
    return std::make_unique<ThrowStatement>(std::move(value));
}

std::vector<FunctionDecl::Parameter> Parser::parse_parameters() {
    consume(TokenType::LeftParen, "Expected '('");

    std::vector<FunctionDecl::Parameter> params;
    if (!check(TokenType::RightParen)) {
        do {
            if (params.size() >= 255) {
                error("Too many parameters");
            }

            FunctionDecl::Parameter param;

            if (match(TokenType::DotDotDot)) {
                param.is_variadic = true;
            }

            Token name = consume(TokenType::Identifier, "Expected parameter name");
            param.name = std::string(name.lexeme);
            param.type = parse_type_annotation();

            if (match(TokenType::Assign)) {
                param.default_value = parse_expression();
            }

            params.push_back(std::move(param));
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "Expected ')'");
    return params;
}

std::vector<ExprPtr> Parser::parse_arguments() {
    std::vector<ExprPtr> args;

    if (!check(TokenType::RightParen)) {
        do {
            if (args.size() >= 255) {
                error("Too many arguments");
            }
            args.push_back(parse_expression());
        } while (match(TokenType::Comma));
    }

    return args;
}

std::optional<std::string> Parser::parse_type_annotation() {
    if (!match(TokenType::Colon)) {
        return std::nullopt;
    }

    Token type = consume(TokenType::Identifier, "Expected type name");
    std::string result(type.lexeme);

    // Generic types like Array<T>
    if (match(TokenType::Less)) {
        result += "<";
        do {
            Token generic = consume(TokenType::Identifier, "Expected type");
            result += std::string(generic.lexeme);
            if (check(TokenType::Comma)) {
                result += ", ";
            }
        } while (match(TokenType::Comma));
        consume(TokenType::Greater, "Expected '>'");
        result += ">";
    }

    return result;
}

} // namespace void_script
