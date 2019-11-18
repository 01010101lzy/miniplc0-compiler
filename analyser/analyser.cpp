#include "analyser.h"

#include <climits>

namespace miniplc0 {
std::pair<std::vector<Instruction>, std::optional<CompilationError>>
Analyser::Analyse() {
  auto err = analyseProgram();
  if (err.has_value())
    return std::make_pair(std::vector<Instruction>(), err);
  else
    return std::make_pair(_instructions, std::optional<CompilationError>());
}

inline bool Analyser::expectToken(const TokenType& t) {
  auto x = nextToken();
  return (x.has_value() && x.value().GetType() == t);
}
inline bool Analyser::peekExpectToken(const TokenType& t) {
  auto x = nextToken();
  unreadToken();
  return (x.has_value() && x.value().GetType() == t);
}
inline bool Analyser::tryExpectToken(const TokenType& t) {
  auto x = nextToken();
  if (x.has_value() && x.value().GetType() == t) {
    return true;
  } else {
    unreadToken();
    return false;
  }
}

// <程序> ::= 'begin'<主过程>'end'
std::optional<CompilationError> Analyser::analyseProgram() {
  // 示例函数，示例如何调用子程序

  // 'begin'
  if (!expectToken(TokenType::BEGIN))
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrNoBegin);

  // <主过程>
  auto err = analyseMain();
  if (err.has_value()) return err;

  // 'end'
  if (!expectToken(TokenType::END))
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrNoEnd);
  return {};
}

// <主过程> ::= <常量声明><变量声明><语句序列>
// 需要补全
std::optional<CompilationError> Analyser::analyseMain() {
  // 完全可以参照 <程序> 编写

  // <常量声明>
  auto err = analyseConstantDeclaration();
  if (err.has_value()) return err;

  // <变量声明>
  err = analyseVariableDeclaration();
  if (err.has_value()) return err;

  // <语句序列>
  err = analyseStatementSequence();
  if (err.has_value()) return err;

  return {};
}

// <常量声明> ::= {<常量声明语句>}
// <常量声明语句> ::= 'const'<标识符>'='<常表达式>';'
std::optional<CompilationError> Analyser::analyseConstantDeclaration() {
  // 示例函数，示例如何分析常量声明

  // 常量声明语句可能有 0 或无数个
  while (true) {
    // 预读一个 token，不然不知道是否应该用 <常量声明> 推导
    // 如果是 const 那么说明应该推导 <常量声明> 否则直接返回
    if (!tryExpectToken(TokenType::CONST)) {
      return {};
    }

    // <常量声明语句>
    if (!peekExpectToken(TokenType::IDENTIFIER))
      return std::make_optional<CompilationError>(_current_pos,
                                                  ErrorCode::ErrNeedIdentifier);

    auto next = nextToken();
    if (isDeclared(next.value().GetValueString()))
      return std::make_optional<CompilationError>(
          _current_pos, ErrorCode::ErrDuplicateDeclaration);
    addConstant(next.value());

    // '='
    if (!expectToken(TokenType::EQUAL_SIGN))
      return std::make_optional<CompilationError>(
          _current_pos, ErrorCode::ErrConstantNeedValue);

    // <常表达式>
    int32_t val;
    auto err = analyseConstantExpression(val);
    if (err.has_value()) return err;

    // ';'
    if (!expectToken(TokenType::SEMICOLON))
      return std::make_optional<CompilationError>(_current_pos,
                                                  ErrorCode::ErrNoSemicolon);
    // 生成一次 LIT 指令加载常量
    _instructions.emplace_back(Operation::LIT, val);
    // _instructions.emplace_back(Operation::STO,
    //                            getIndex(next.value().GetValueString()));
  }
  return {};
}

// <变量声明> ::= {<变量声明语句>}
// <变量声明语句> ::= 'var'<标识符>['='<表达式>]';'
// 需要补全
std::optional<CompilationError> Analyser::analyseVariableDeclaration() {
  // 变量声明语句可能有一个或者多个
  while (1) {
    if (!tryExpectToken(TokenType::VAR)) return {};

    // <常量声明语句>
    if (!peekExpectToken(TokenType::IDENTIFIER))
      return std::make_optional<CompilationError>(_current_pos,
                                                  ErrorCode::ErrNeedIdentifier);

    auto next = nextToken().value();
    if (isDeclared(next.GetValueString()))
      return {
          CompilationError(_current_pos, ErrorCode::ErrDuplicateDeclaration)};

    if (!tryExpectToken(TokenType::EQUAL_SIGN)) {
      addUninitializedVariable(next);
      // variables are created in sequence
      _instructions.emplace_back(Operation::LIT, 0);
      // _instructions.emplace_back(Operation::STO,
      //                            getIndex(next.GetValueString()));
    } else {
      // '<表达式>'

      // int32_t val;
      auto err = analyseExpression();
      if (err.has_value()) return err;

      addVariable(next);
      // _instructions.emplace_back(Operation::STO,
      //  getIndex(next.GetValueString()));
    }

    // ';'
    if (!expectToken(TokenType::SEMICOLON)) {
      return {CompilationError(_current_pos, ErrorCode::ErrNoSemicolon)};
    }
  }
  return {};
}

// <语句序列> ::= {<语句>}
// <语句> :: = <赋值语句> | <输出语句> | <空语句>
// <赋值语句> :: = <标识符>'='<表达式>';'
// <输出语句> :: = 'print' '(' <表达式> ')' ';'
// <空语句> :: = ';'
// 需要补全
std::optional<CompilationError> Analyser::analyseStatementSequence() {
  while (true) {
    if (tryExpectToken(TokenType::SEMICOLON)) {
      // noop
    } else if (peekExpectToken(TokenType::IDENTIFIER)) {
      // Assignment
      auto e = analyseAssignmentStatement();
      if (e.has_value()) return e;
    } else if (peekExpectToken(TokenType::PRINT)) {
      auto e = analyseOutputStatement();
      if (e.has_value()) return e;
    } else {
      return {};
    }
  }
}

// <常表达式> ::= [<符号>]<无符号整数>
// 需要补全
std::optional<CompilationError> Analyser::analyseConstantExpression(
    int32_t& out) {
  // out 是常表达式的结果
  // 这里你要分析常表达式并且计算结果
  // 注意以下均为常表达式
  // +1 -1 1
  // 同时要注意是否溢出
  bool neg = false;
  if (tryExpectToken(TokenType::PLUS_SIGN))
    neg = false;
  else if (tryExpectToken(TokenType::MINUS_SIGN))
    neg = true;
  if (!peekExpectToken(TokenType::UNSIGNED_INTEGER))
    return {CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};
  auto n = nextToken();
  // we are sure that this value does not overflow in tokenizer
  int32_t k;
  try {
    k = std::any_cast<int32_t>(n.value().GetValue());
  } catch (std::bad_any_cast&) {
    return {CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};
  }
  if (neg) k = -k;
  out = k;
  return {};
}

// <表达式> ::= <项>{<加法型运算符><项>}
std::optional<CompilationError> Analyser::analyseExpression() {
  // <项>
  auto err = analyseItem();
  if (err.has_value()) return err;

  // {<加法型运算符><项>}
  while (true) {
    // 预读
    bool plus;
    if (tryExpectToken(TokenType::PLUS_SIGN)) {
      plus = true;
    } else if (tryExpectToken(TokenType::MINUS_SIGN)) {
      plus = false;
    } else {
      return {};
    }

    err = analyseItem();
    if (err.has_value()) return err;

    if (plus)
      _instructions.emplace_back(Operation::ADD, 0);
    else
      _instructions.emplace_back(Operation::SUB, 0);
  }
  return {};
}

// <赋值语句> ::= <标识符>'='<表达式>';'
// 需要补全
std::optional<CompilationError> Analyser::analyseAssignmentStatement() {
  auto next = nextToken();

  // assert next.has_value()
  auto ident = next.value().GetValueString();
  if (!isDeclared(ident))
    return {CompilationError(_current_pos, ErrorCode::ErrNotDeclared)};
  if (isConstant(ident))
    return {CompilationError(_current_pos, ErrorCode::ErrAssignToConstant)};

  if (!expectToken(TokenType::EQUAL_SIGN))
    return {CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};

  auto err = analyseExpression();
  if (err.has_value()) return err;

  if (!isInitializedVariable(ident)) makeInitialized(next.value());
  if (!expectToken(TokenType::SEMICOLON))
    return {CompilationError(_current_pos, ErrorCode::ErrNoSemicolon)};

  _instructions.emplace_back(Operation::STO, getIndex(ident));
  // 这里除了语法分析以外还要留意
  // 标识符声明过吗？
  // 标识符是常量吗？
  // 需要生成指令吗？
  return {};
}

// <输出语句> ::= 'print' '(' <表达式> ')' ';'
std::optional<CompilationError> Analyser::analyseOutputStatement() {
  // 如果之前 <语句序列> 的实现正确，这里第一个 next 一定是 TokenType::PRINT
  auto next = nextToken();

  // '('
  next = nextToken();
  if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrInvalidPrint);

  // <表达式>
  auto err = analyseExpression();
  if (err.has_value()) return err;

  // ')'
  next = nextToken();
  if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrInvalidPrint);

  // ';'
  next = nextToken();
  if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrNoSemicolon);

  // 生成相应的指令 WRT
  _instructions.emplace_back(Operation::WRT, 0);
  return {};
}

// <项> :: = <因子>{ <乘法型运算符><因子> }
// 需要补全
std::optional<CompilationError> Analyser::analyseItem() {
  // <项>
  auto err = analyseFactor();
  if (err.has_value()) return err;

  // {<加法型运算符><项>}
  while (true) {
    // 预读
    bool mult;
    if (tryExpectToken(TokenType::MULTIPLICATION_SIGN)) {
      mult = true;
    } else if (tryExpectToken(TokenType::DIVISION_SIGN)) {
      mult = false;
    } else {
      return {};
    }

    err = analyseFactor();
    if (err.has_value()) return err;

    if (mult)
      _instructions.emplace_back(Operation::MUL, 0);
    else
      _instructions.emplace_back(Operation::DIV, 0);
  }
  return {};
}

// <因子> ::= [<符号>]( <标识符> | <无符号整数> | '('<表达式>')' )
// 需要补全
std::optional<CompilationError> Analyser::analyseFactor() {
  // [<符号>]
  auto next = nextToken();
  auto prefix = 1;
  if (!next.has_value())
    return std::make_optional<CompilationError>(
        _current_pos, ErrorCode::ErrIncompleteExpression);
  if (next.value().GetType() == TokenType::PLUS_SIGN)
    prefix = 1;
  else if (next.value().GetType() == TokenType::MINUS_SIGN) {
    prefix = -1;
    _instructions.emplace_back(Operation::LIT, 0);
  } else
    unreadToken();

  if (peekExpectToken(TokenType::IDENTIFIER)) {
    // identifier
    auto next = nextToken().value();
    auto ident = next.GetValueString();
    if (!isDeclared(ident))
      return {CompilationError(_current_pos, ErrorCode::ErrNotDeclared)};
    if (!isInitializedVariable(ident) && !isConstant(ident))
      return {CompilationError(_current_pos, ErrorCode::ErrNotInitialized)};

    _instructions.emplace_back(Operation::LOD, getIndex(ident));

  } else if (peekExpectToken(TokenType::UNSIGNED_INTEGER)) {
    auto next = nextToken().value();
    // we are sure this will not overflow
    int32_t val = std::any_cast<int32_t>(next.GetValue());
    _instructions.emplace_back(Operation::LIT, val);

  } else if (tryExpectToken(TokenType::LEFT_BRACKET)) {
    analyseExpression();
    if (!expectToken(TokenType::RIGHT_BRACKET))
      return {
          CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};
  } else {
    return {CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};
  }

  // 取负
  if (prefix == -1) _instructions.emplace_back(Operation::SUB, 0);
  return {};
}

std::optional<Token> Analyser::nextToken() {
  if (_offset == _tokens.size()) return {};
  // 考虑到 _tokens[0..._offset-1] 已经被分析过了
  // 所以我们选择 _tokens[0..._offset-1] 的 EndPos 作为当前位置
  _current_pos = _tokens[_offset].GetEndPos();
  return _tokens[_offset++];
}

void Analyser::unreadToken() {
  if (_offset == 0) DieAndPrint("analyser unreads token from the begining.");
  _current_pos = _tokens[_offset - 1].GetEndPos();
  _offset--;
}

void Analyser::_add(const Token& tk, std::map<std::string, int32_t>& mp) {
  if (tk.GetType() != TokenType::IDENTIFIER)
    DieAndPrint("only identifier can be added to the table.");
  mp[tk.GetValueString()] = _nextTokenIndex;
  _nextTokenIndex++;
}

void Analyser::addVariable(const Token& tk) { _add(tk, _vars); }

void Analyser::addConstant(const Token& tk) { _add(tk, _consts); }

void Analyser::makeInitialized(const Token& var) {
  makeInitialized(var.GetValueString());
}
void Analyser::makeInitialized(const std::string& var_name) {
  auto var = _uninitialized_vars.find(var_name);
  if (var == _uninitialized_vars.end())
    DieAndPrint("Variable not found in uninitialized area. bad bad");
  auto item = _uninitialized_vars.extract(var);
  _vars.insert(std::move(item));
}

void Analyser::addUninitializedVariable(const Token& tk) {
  _add(tk, _uninitialized_vars);
}

int32_t Analyser::getIndex(const std::string& s) {
  if (_uninitialized_vars.find(s) != _uninitialized_vars.end())
    return _uninitialized_vars[s];
  else if (_vars.find(s) != _vars.end())
    return _vars[s];
  else
    return _consts[s];
}

bool Analyser::isDeclared(const std::string& s) {
  return isConstant(s) || isUninitializedVariable(s) ||
         isInitializedVariable(s);
}

bool Analyser::isUninitializedVariable(const std::string& s) {
  return _uninitialized_vars.find(s) != _uninitialized_vars.end();
}
bool Analyser::isInitializedVariable(const std::string& s) {
  return _vars.find(s) != _vars.end();
}

bool Analyser::isConstant(const std::string& s) {
  return _consts.find(s) != _consts.end();
}
}  // namespace miniplc0
