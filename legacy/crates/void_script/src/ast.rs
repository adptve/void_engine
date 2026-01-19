//! Abstract Syntax Tree for VoidScript
//!
//! Defines the AST nodes for expressions and statements.

use crate::value::Value;

/// A complete program
#[derive(Debug, Clone)]
pub struct Program {
    pub statements: Vec<Stmt>,
}

impl Program {
    pub fn new(statements: Vec<Stmt>) -> Self {
        Self { statements }
    }
}

/// Statement types
#[derive(Debug, Clone)]
pub enum Stmt {
    /// Expression statement
    Expr(Expr),

    /// Variable declaration: let x = expr;
    Let {
        name: String,
        value: Expr,
    },

    /// Assignment: x = expr;
    Assign {
        name: String,
        value: Expr,
    },

    /// Block: { statements }
    Block(Vec<Stmt>),

    /// If statement: if cond { } else { }
    If {
        condition: Expr,
        then_branch: Box<Stmt>,
        else_branch: Option<Box<Stmt>>,
    },

    /// While loop: while cond { }
    While {
        condition: Expr,
        body: Box<Stmt>,
    },

    /// For loop: for x in iter { }
    For {
        variable: String,
        iterable: Expr,
        body: Box<Stmt>,
    },

    /// Function declaration: fn name(params) { }
    Function {
        name: String,
        params: Vec<String>,
        body: Vec<Stmt>,
    },

    /// Return statement: return expr;
    Return(Option<Expr>),

    /// Break statement
    Break,

    /// Continue statement
    Continue,
}

/// Expression types
#[derive(Debug, Clone)]
pub enum Expr {
    /// Literal value
    Literal(Value),

    /// Variable reference
    Ident(String),

    /// Binary operation: left op right
    Binary {
        left: Box<Expr>,
        op: BinaryOp,
        right: Box<Expr>,
    },

    /// Unary operation: op expr
    Unary {
        op: UnaryOp,
        expr: Box<Expr>,
    },

    /// Function call: name(args)
    Call {
        callee: Box<Expr>,
        args: Vec<Expr>,
    },

    /// Index access: expr[index]
    Index {
        object: Box<Expr>,
        index: Box<Expr>,
    },

    /// Member access: expr.member
    Member {
        object: Box<Expr>,
        member: String,
    },

    /// Array literal: [a, b, c]
    Array(Vec<Expr>),

    /// Object literal: { key: value }
    Object(Vec<(String, Expr)>),

    /// Ternary: cond ? then : else
    Ternary {
        condition: Box<Expr>,
        then_expr: Box<Expr>,
        else_expr: Box<Expr>,
    },

    /// Lambda: |params| expr or |params| { body }
    Lambda {
        params: Vec<String>,
        body: Box<Expr>,
    },
}

/// Binary operators
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BinaryOp {
    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    // Comparison
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,

    // Logical
    And,
    Or,
}

impl BinaryOp {
    /// Get operator precedence (higher = binds tighter)
    pub fn precedence(&self) -> u8 {
        match self {
            Self::Or => 1,
            Self::And => 2,
            Self::Eq | Self::Ne => 3,
            Self::Lt | Self::Le | Self::Gt | Self::Ge => 4,
            Self::Add | Self::Sub => 5,
            Self::Mul | Self::Div | Self::Mod => 6,
        }
    }

    /// Get operator symbol
    pub fn symbol(&self) -> &'static str {
        match self {
            Self::Add => "+",
            Self::Sub => "-",
            Self::Mul => "*",
            Self::Div => "/",
            Self::Mod => "%",
            Self::Eq => "==",
            Self::Ne => "!=",
            Self::Lt => "<",
            Self::Le => "<=",
            Self::Gt => ">",
            Self::Ge => ">=",
            Self::And => "&&",
            Self::Or => "||",
        }
    }
}

/// Unary operators
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UnaryOp {
    /// Negation: -x
    Neg,
    /// Logical not: !x
    Not,
}

impl UnaryOp {
    pub fn symbol(&self) -> &'static str {
        match self {
            Self::Neg => "-",
            Self::Not => "!",
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_binary_op_precedence() {
        assert!(BinaryOp::Mul.precedence() > BinaryOp::Add.precedence());
        assert!(BinaryOp::Add.precedence() > BinaryOp::Eq.precedence());
        assert!(BinaryOp::And.precedence() > BinaryOp::Or.precedence());
    }
}
