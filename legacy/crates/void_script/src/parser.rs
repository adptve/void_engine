//! Parser for VoidScript
//!
//! Converts a stream of tokens into an AST.

use thiserror::Error;

use crate::lexer::{Token, TokenKind};
use crate::ast::{Expr, Stmt, Program, BinaryOp, UnaryOp};
use crate::value::Value;

/// Parse errors
#[derive(Debug, Error)]
pub enum ParseError {
    #[error("Unexpected token: {0}")]
    UnexpectedToken(String),

    #[error("Expected {expected}, found {found}")]
    Expected { expected: String, found: String },

    #[error("Unexpected end of input")]
    UnexpectedEof,

    #[error("Invalid assignment target")]
    InvalidAssignment,

    #[error("Too many arguments (max 255)")]
    TooManyArguments,

    #[error("Too many parameters (max 255)")]
    TooManyParameters,
}

/// Parser for VoidScript
pub struct Parser {
    tokens: Vec<Token>,
    current: usize,
}

impl Parser {
    /// Create a new parser
    pub fn new(tokens: Vec<Token>) -> Self {
        Self { tokens, current: 0 }
    }

    /// Parse the entire program
    pub fn parse(&mut self) -> Result<Program, ParseError> {
        let mut statements = Vec::new();

        while !self.is_at_end() {
            statements.push(self.declaration()?);
        }

        Ok(Program::new(statements))
    }

    /// Parse a single expression (for REPL)
    pub fn parse_expression(&mut self) -> Result<Expr, ParseError> {
        self.expression()
    }

    // === Declaration parsing ===

    fn declaration(&mut self) -> Result<Stmt, ParseError> {
        if self.match_token(&[TokenKind::Let]) {
            self.var_declaration()
        } else if self.match_token(&[TokenKind::Fn]) {
            self.function_declaration()
        } else {
            self.statement()
        }
    }

    fn var_declaration(&mut self) -> Result<Stmt, ParseError> {
        let name = self.consume_ident("Expected variable name")?;

        self.consume(&TokenKind::Eq, "Expected '=' after variable name")?;
        let value = self.expression()?;
        self.consume(&TokenKind::Semicolon, "Expected ';' after variable declaration")?;

        Ok(Stmt::Let { name, value })
    }

    fn function_declaration(&mut self) -> Result<Stmt, ParseError> {
        let name = self.consume_ident("Expected function name")?;

        self.consume(&TokenKind::LParen, "Expected '(' after function name")?;

        let mut params = Vec::new();
        if !self.check(&TokenKind::RParen) {
            loop {
                if params.len() >= 255 {
                    return Err(ParseError::TooManyParameters);
                }
                params.push(self.consume_ident("Expected parameter name")?);
                if !self.match_token(&[TokenKind::Comma]) {
                    break;
                }
            }
        }

        self.consume(&TokenKind::RParen, "Expected ')' after parameters")?;
        self.consume(&TokenKind::LBrace, "Expected '{' before function body")?;

        let body = self.block_statements()?;

        Ok(Stmt::Function { name, params, body })
    }

    // === Statement parsing ===

    fn statement(&mut self) -> Result<Stmt, ParseError> {
        if self.match_token(&[TokenKind::If]) {
            self.if_statement()
        } else if self.match_token(&[TokenKind::While]) {
            self.while_statement()
        } else if self.match_token(&[TokenKind::For]) {
            self.for_statement()
        } else if self.match_token(&[TokenKind::Return]) {
            self.return_statement()
        } else if self.match_token(&[TokenKind::Break]) {
            self.consume(&TokenKind::Semicolon, "Expected ';' after 'break'")?;
            Ok(Stmt::Break)
        } else if self.match_token(&[TokenKind::Continue]) {
            self.consume(&TokenKind::Semicolon, "Expected ';' after 'continue'")?;
            Ok(Stmt::Continue)
        } else if self.match_token(&[TokenKind::LBrace]) {
            Ok(Stmt::Block(self.block_statements()?))
        } else {
            self.expression_statement()
        }
    }

    fn if_statement(&mut self) -> Result<Stmt, ParseError> {
        let condition = self.expression()?;
        self.consume(&TokenKind::LBrace, "Expected '{' after if condition")?;
        let then_branch = Box::new(Stmt::Block(self.block_statements()?));

        let else_branch = if self.match_token(&[TokenKind::Else]) {
            if self.match_token(&[TokenKind::If]) {
                Some(Box::new(self.if_statement()?))
            } else {
                self.consume(&TokenKind::LBrace, "Expected '{' after 'else'")?;
                Some(Box::new(Stmt::Block(self.block_statements()?)))
            }
        } else {
            None
        };

        Ok(Stmt::If {
            condition,
            then_branch,
            else_branch,
        })
    }

    fn while_statement(&mut self) -> Result<Stmt, ParseError> {
        let condition = self.expression()?;
        self.consume(&TokenKind::LBrace, "Expected '{' after while condition")?;
        let body = Box::new(Stmt::Block(self.block_statements()?));

        Ok(Stmt::While { condition, body })
    }

    fn for_statement(&mut self) -> Result<Stmt, ParseError> {
        let variable = self.consume_ident("Expected variable name")?;
        self.consume(&TokenKind::In, "Expected 'in' after variable")?;
        let iterable = self.expression()?;
        self.consume(&TokenKind::LBrace, "Expected '{' after iterable")?;
        let body = Box::new(Stmt::Block(self.block_statements()?));

        Ok(Stmt::For {
            variable,
            iterable,
            body,
        })
    }

    fn return_statement(&mut self) -> Result<Stmt, ParseError> {
        let value = if !self.check(&TokenKind::Semicolon) {
            Some(self.expression()?)
        } else {
            None
        };
        self.consume(&TokenKind::Semicolon, "Expected ';' after return value")?;
        Ok(Stmt::Return(value))
    }

    fn block_statements(&mut self) -> Result<Vec<Stmt>, ParseError> {
        let mut statements = Vec::new();

        while !self.check(&TokenKind::RBrace) && !self.is_at_end() {
            statements.push(self.declaration()?);
        }

        self.consume(&TokenKind::RBrace, "Expected '}' after block")?;
        Ok(statements)
    }

    fn expression_statement(&mut self) -> Result<Stmt, ParseError> {
        let expr = self.expression()?;

        // Check for assignment
        if self.match_token(&[TokenKind::Eq]) {
            let value = self.expression()?;
            self.consume(&TokenKind::Semicolon, "Expected ';' after assignment")?;

            match expr {
                Expr::Ident(name) => Ok(Stmt::Assign { name, value }),
                _ => Err(ParseError::InvalidAssignment),
            }
        } else {
            self.consume(&TokenKind::Semicolon, "Expected ';' after expression")?;
            Ok(Stmt::Expr(expr))
        }
    }

    // === Expression parsing (Pratt parser) ===

    fn expression(&mut self) -> Result<Expr, ParseError> {
        self.parse_precedence(1)
    }

    fn parse_precedence(&mut self, min_precedence: u8) -> Result<Expr, ParseError> {
        let mut left = self.unary()?;

        while let Some(op) = self.peek_binary_op() {
            let precedence = op.precedence();
            if precedence < min_precedence {
                break;
            }

            self.advance();
            let right = self.parse_precedence(precedence + 1)?;
            left = Expr::Binary {
                left: Box::new(left),
                op,
                right: Box::new(right),
            };
        }

        // Handle ternary
        if self.check_ternary() {
            left = self.ternary(left)?;
        }

        Ok(left)
    }

    fn check_ternary(&self) -> bool {
        // We don't have a '?' token, so ternary would need different syntax
        // For now, skip ternary support
        false
    }

    fn ternary(&mut self, _condition: Expr) -> Result<Expr, ParseError> {
        // Ternary would need '?' and ':' tokens
        // Not implemented in current lexer
        Err(ParseError::UnexpectedToken("Ternary not supported".to_string()))
    }

    fn unary(&mut self) -> Result<Expr, ParseError> {
        if self.match_token(&[TokenKind::Bang]) {
            let expr = self.unary()?;
            Ok(Expr::Unary {
                op: UnaryOp::Not,
                expr: Box::new(expr),
            })
        } else if self.match_token(&[TokenKind::Minus]) {
            let expr = self.unary()?;
            Ok(Expr::Unary {
                op: UnaryOp::Neg,
                expr: Box::new(expr),
            })
        } else {
            self.call()
        }
    }

    fn call(&mut self) -> Result<Expr, ParseError> {
        let mut expr = self.primary()?;

        loop {
            if self.match_token(&[TokenKind::LParen]) {
                expr = self.finish_call(expr)?;
            } else if self.match_token(&[TokenKind::Dot]) {
                let member = self.consume_ident("Expected property name after '.'")?;
                expr = Expr::Member {
                    object: Box::new(expr),
                    member,
                };
            } else if self.match_token(&[TokenKind::LBracket]) {
                let index = self.expression()?;
                self.consume(&TokenKind::RBracket, "Expected ']' after index")?;
                expr = Expr::Index {
                    object: Box::new(expr),
                    index: Box::new(index),
                };
            } else {
                break;
            }
        }

        Ok(expr)
    }

    fn finish_call(&mut self, callee: Expr) -> Result<Expr, ParseError> {
        let mut args = Vec::new();

        if !self.check(&TokenKind::RParen) {
            loop {
                if args.len() >= 255 {
                    return Err(ParseError::TooManyArguments);
                }
                args.push(self.expression()?);
                if !self.match_token(&[TokenKind::Comma]) {
                    break;
                }
            }
        }

        self.consume(&TokenKind::RParen, "Expected ')' after arguments")?;

        Ok(Expr::Call {
            callee: Box::new(callee),
            args,
        })
    }

    fn primary(&mut self) -> Result<Expr, ParseError> {
        // Literals
        if let Some(token) = self.advance_if(|k| matches!(k,
            TokenKind::Int(_) | TokenKind::Float(_) |
            TokenKind::String(_) | TokenKind::Bool(_) | TokenKind::Null
        )) {
            let value = match token.kind {
                TokenKind::Int(n) => Value::Int(n),
                TokenKind::Float(f) => Value::Float(f),
                TokenKind::String(s) => Value::String(s),
                TokenKind::Bool(b) => Value::Bool(b),
                TokenKind::Null => Value::Null,
                _ => unreachable!(),
            };
            return Ok(Expr::Literal(value));
        }

        // Identifier
        if let Some(token) = self.advance_if(|k| matches!(k, TokenKind::Ident(_))) {
            if let TokenKind::Ident(name) = token.kind {
                return Ok(Expr::Ident(name));
            }
        }

        // Grouped expression
        if self.match_token(&[TokenKind::LParen]) {
            let expr = self.expression()?;
            self.consume(&TokenKind::RParen, "Expected ')' after expression")?;
            return Ok(expr);
        }

        // Array literal
        if self.match_token(&[TokenKind::LBracket]) {
            return self.array_literal();
        }

        // Object literal
        if self.match_token(&[TokenKind::LBrace]) {
            return self.object_literal();
        }

        // Lambda: |params| expr
        if self.match_token(&[TokenKind::Or]) {
            return self.lambda();
        }

        Err(ParseError::UnexpectedToken(format!(
            "Unexpected token: {:?}",
            self.peek()
        )))
    }

    fn array_literal(&mut self) -> Result<Expr, ParseError> {
        let mut elements = Vec::new();

        if !self.check(&TokenKind::RBracket) {
            loop {
                elements.push(self.expression()?);
                if !self.match_token(&[TokenKind::Comma]) {
                    break;
                }
                // Allow trailing comma
                if self.check(&TokenKind::RBracket) {
                    break;
                }
            }
        }

        self.consume(&TokenKind::RBracket, "Expected ']' after array elements")?;
        Ok(Expr::Array(elements))
    }

    fn object_literal(&mut self) -> Result<Expr, ParseError> {
        let mut pairs = Vec::new();

        if !self.check(&TokenKind::RBrace) {
            loop {
                let key = self.consume_ident("Expected property name")?;
                self.consume(&TokenKind::Colon, "Expected ':' after property name")?;
                let value = self.expression()?;
                pairs.push((key, value));

                if !self.match_token(&[TokenKind::Comma]) {
                    break;
                }
                // Allow trailing comma
                if self.check(&TokenKind::RBrace) {
                    break;
                }
            }
        }

        self.consume(&TokenKind::RBrace, "Expected '}' after object")?;
        Ok(Expr::Object(pairs))
    }

    fn lambda(&mut self) -> Result<Expr, ParseError> {
        // Already consumed the first |
        let mut params = Vec::new();

        if !self.check(&TokenKind::Or) {
            loop {
                params.push(self.consume_ident("Expected parameter name")?);
                if !self.match_token(&[TokenKind::Comma]) {
                    break;
                }
            }
        }

        self.consume(&TokenKind::Or, "Expected '|' after lambda parameters")?;

        let body = self.expression()?;

        Ok(Expr::Lambda {
            params,
            body: Box::new(body),
        })
    }

    // === Helper methods ===

    fn peek_binary_op(&self) -> Option<BinaryOp> {
        let token = self.peek()?;
        match &token.kind {
            TokenKind::Plus => Some(BinaryOp::Add),
            TokenKind::Minus => Some(BinaryOp::Sub),
            TokenKind::Star => Some(BinaryOp::Mul),
            TokenKind::Slash => Some(BinaryOp::Div),
            TokenKind::Percent => Some(BinaryOp::Mod),
            TokenKind::EqEq => Some(BinaryOp::Eq),
            TokenKind::BangEq => Some(BinaryOp::Ne),
            TokenKind::Lt => Some(BinaryOp::Lt),
            TokenKind::LtEq => Some(BinaryOp::Le),
            TokenKind::Gt => Some(BinaryOp::Gt),
            TokenKind::GtEq => Some(BinaryOp::Ge),
            TokenKind::And => Some(BinaryOp::And),
            TokenKind::Or => Some(BinaryOp::Or),
            _ => None,
        }
    }

    fn peek(&self) -> Option<&Token> {
        self.tokens.get(self.current)
    }

    fn is_at_end(&self) -> bool {
        self.peek()
            .map(|t| matches!(t.kind, TokenKind::Eof))
            .unwrap_or(true)
    }

    fn advance(&mut self) -> Option<Token> {
        if !self.is_at_end() {
            self.current += 1;
            self.tokens.get(self.current - 1).cloned()
        } else {
            None
        }
    }

    fn advance_if<F>(&mut self, predicate: F) -> Option<Token>
    where
        F: FnOnce(&TokenKind) -> bool,
    {
        if let Some(token) = self.peek() {
            if predicate(&token.kind) {
                return self.advance();
            }
        }
        None
    }

    fn check(&self, kind: &TokenKind) -> bool {
        self.peek()
            .map(|t| std::mem::discriminant(&t.kind) == std::mem::discriminant(kind))
            .unwrap_or(false)
    }

    fn match_token(&mut self, kinds: &[TokenKind]) -> bool {
        for kind in kinds {
            if self.check(kind) {
                self.advance();
                return true;
            }
        }
        false
    }

    fn consume(&mut self, kind: &TokenKind, message: &str) -> Result<Token, ParseError> {
        if self.check(kind) {
            self.advance().ok_or(ParseError::UnexpectedEof)
        } else {
            Err(ParseError::Expected {
                expected: message.to_string(),
                found: format!("{:?}", self.peek().map(|t| &t.kind)),
            })
        }
    }

    fn consume_ident(&mut self, message: &str) -> Result<String, ParseError> {
        if let Some(token) = self.peek() {
            if let TokenKind::Ident(name) = &token.kind {
                let name = name.clone();
                self.advance();
                return Ok(name);
            }
        }
        Err(ParseError::Expected {
            expected: message.to_string(),
            found: format!("{:?}", self.peek().map(|t| &t.kind)),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::Lexer;

    fn parse(source: &str) -> Result<Program, ParseError> {
        let tokens = Lexer::new(source).tokenize().unwrap();
        Parser::new(tokens).parse()
    }

    fn parse_expr(source: &str) -> Result<Expr, ParseError> {
        let tokens = Lexer::new(source).tokenize().unwrap();
        Parser::new(tokens).parse_expression()
    }

    #[test]
    fn test_parse_literal() {
        let expr = parse_expr("42").unwrap();
        assert!(matches!(expr, Expr::Literal(Value::Int(42))));
    }

    #[test]
    fn test_parse_binary() {
        let expr = parse_expr("1 + 2 * 3").unwrap();
        // Should be: Add(1, Mul(2, 3))
        if let Expr::Binary { op, .. } = expr {
            assert_eq!(op, BinaryOp::Add);
        } else {
            panic!("Expected binary expression");
        }
    }

    #[test]
    fn test_parse_let() {
        let program = parse("let x = 10;").unwrap();
        assert_eq!(program.statements.len(), 1);
        assert!(matches!(&program.statements[0], Stmt::Let { name, .. } if name == "x"));
    }

    #[test]
    fn test_parse_function() {
        let program = parse("fn add(a, b) { return a + b; }").unwrap();
        assert_eq!(program.statements.len(), 1);
        if let Stmt::Function { name, params, body } = &program.statements[0] {
            assert_eq!(name, "add");
            assert_eq!(params, &["a", "b"]);
            assert_eq!(body.len(), 1);
        } else {
            panic!("Expected function");
        }
    }

    #[test]
    fn test_parse_if() {
        let program = parse("if true { x = 1; }").unwrap();
        assert_eq!(program.statements.len(), 1);
        assert!(matches!(&program.statements[0], Stmt::If { .. }));
    }

    #[test]
    fn test_parse_while() {
        let program = parse("while x > 0 { x = x - 1; }").unwrap();
        assert_eq!(program.statements.len(), 1);
        assert!(matches!(&program.statements[0], Stmt::While { .. }));
    }

    #[test]
    fn test_parse_for() {
        let program = parse("for i in items { print(i); }").unwrap();
        assert_eq!(program.statements.len(), 1);
        assert!(matches!(&program.statements[0], Stmt::For { variable, .. } if variable == "i"));
    }

    #[test]
    fn test_parse_array() {
        let expr = parse_expr("[1, 2, 3]").unwrap();
        if let Expr::Array(elements) = expr {
            assert_eq!(elements.len(), 3);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_parse_object() {
        let expr = parse_expr("{ x: 1, y: 2 }").unwrap();
        if let Expr::Object(pairs) = expr {
            assert_eq!(pairs.len(), 2);
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_parse_call() {
        let expr = parse_expr("foo(1, 2)").unwrap();
        if let Expr::Call { args, .. } = expr {
            assert_eq!(args.len(), 2);
        } else {
            panic!("Expected call");
        }
    }

    #[test]
    fn test_parse_member() {
        let expr = parse_expr("obj.field").unwrap();
        if let Expr::Member { member, .. } = expr {
            assert_eq!(member, "field");
        } else {
            panic!("Expected member access");
        }
    }

    #[test]
    fn test_parse_index() {
        let expr = parse_expr("arr[0]").unwrap();
        assert!(matches!(expr, Expr::Index { .. }));
    }

    #[test]
    fn test_parse_unary() {
        let expr = parse_expr("-5").unwrap();
        if let Expr::Unary { op, .. } = expr {
            assert_eq!(op, UnaryOp::Neg);
        } else {
            panic!("Expected unary");
        }
    }
}
