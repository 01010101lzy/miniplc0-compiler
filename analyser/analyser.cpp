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

bool expect(const std::optional<Token>& t, const TokenType& tt) {
  return (t.has_value() && t.value().GetType() == tt);
}

bool seq(bool a, void (Analyser::*b)(void), Analyser* c) {
  (c->*b)();
  return a;
}

std::optional<CompilationError> Analyser::analyseProgram() {
  if (!(expect(nextToken(), TokenType::BEGIN)))
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrNoBegin);

  auto err = analyseMain();
  if (err.has_value()) return err;

  if (!(expect(nextToken(), TokenType::END)))
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrNoEnd);
  return {};
}

std::optional<CompilationError> Analyser::analyseMain() {
  auto err = analyseConstantDeclaration();
  if (err.has_value()) return err;

  err = analyseVariableDeclaration();
  if (err.has_value()) return err;

  err = analyseStatementSequence();
  if (err.has_value()) return err;

  return {};
}

std::optional<CompilationError> Analyser::analyseConstantDeclaration() {
  while (true) {
    if (!(expect(nextToken(), TokenType::CONST)
              ? true
              : seq(false, &Analyser::unreadToken, this))) {
      return {};
    }

    if (!(seq(expect(nextToken(), TokenType::IDENTIFIER),
              &Analyser::unreadToken, this)))
      return std::make_optional<CompilationError>(_current_pos,
                                                  ErrorCode::ErrNeedIdentifier);

    auto next = nextToken();
    if (isDeclared(next.value().GetValueString()))
      return std::make_optional<CompilationError>(
          _current_pos, ErrorCode::ErrDuplicateDeclaration);
    addConstant(next.value());

    if (!(expect(nextToken(), TokenType::EQUAL_SIGN)))
      return std::make_optional<CompilationError>(
          _current_pos, ErrorCode::ErrConstantNeedValue);

    int32_t val;
    auto err = analyseConstantExpression(val);
    if (err.has_value()) return err;

    if (!(expect(nextToken(), TokenType::SEMICOLON)))
      return std::make_optional<CompilationError>(_current_pos,
                                                  ErrorCode::ErrNoSemicolon);

    _instructions.emplace_back(Operation::LIT, val);
  }
  return {};
}

std::optional<CompilationError> Analyser::analyseVariableDeclaration() {
  while (1) {
    if (!(expect(nextToken(), TokenType::VAR)
              ? true
              : seq(false, &Analyser::unreadToken, this)))
      return {};

    if (!(seq(expect(nextToken(), TokenType::IDENTIFIER),
              &Analyser::unreadToken, this)))
      return std::make_optional<CompilationError>(_current_pos,
                                                  ErrorCode::ErrNeedIdentifier);

    auto next = nextToken().value();
    if (isDeclared(next.GetValueString()))
      return {
          CompilationError(_current_pos, ErrorCode::ErrDuplicateDeclaration)};

    if (!(expect(nextToken(), TokenType::EQUAL_SIGN)
              ? true
              : seq(false, &Analyser::unreadToken, this))) {
      addUninitializedVariable(next);

      _instructions.emplace_back(Operation::LIT, 0);

    } else {
      auto err = analyseExpression();
      if (err.has_value()) return err;

      addVariable(next);
    }

    if (!(expect(nextToken(), TokenType::SEMICOLON))) {
      return {CompilationError(_current_pos, ErrorCode::ErrNoSemicolon)};
    }
  }
  return {};
}

std::optional<CompilationError> Analyser::analyseStatementSequence() {
  while (true) {
    if ((expect(nextToken(), TokenType::SEMICOLON)
             ? true
             : seq(false, &Analyser::unreadToken, this))) {
    } else if ((seq(expect(nextToken(), TokenType::IDENTIFIER),
                    &Analyser::unreadToken, this))) {
      auto e = analyseAssignmentStatement();
      if (e.has_value()) return e;
    } else if ((seq(expect(nextToken(), TokenType::PRINT),
                    &Analyser::unreadToken, this))) {
      auto e = analyseOutputStatement();
      if (e.has_value()) return e;
    } else {
      return {};
    }
  }
}

std::optional<CompilationError> Analyser::analyseConstantExpression(
    int32_t& out) {
  bool neg = false;
  if ((expect(nextToken(), TokenType::PLUS_SIGN)
           ? true
           : seq(false, &Analyser::unreadToken, this)))
    neg = false;
  else if ((expect(nextToken(), TokenType::MINUS_SIGN)
                ? true
                : seq(false, &Analyser::unreadToken, this)))
    neg = true;
  if (!(seq(expect(nextToken(), TokenType::UNSIGNED_INTEGER),
            &Analyser::unreadToken, this)))
    return {CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};
  auto n = nextToken();

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

std::optional<CompilationError> Analyser::analyseExpression() {
  auto err = analyseItem();
  if (err.has_value()) return err;

  while (true) {
    bool plus;
    if ((expect(nextToken(), TokenType::PLUS_SIGN)
             ? true
             : seq(false, &Analyser::unreadToken, this))) {
      plus = true;
    } else if ((expect(nextToken(), TokenType::MINUS_SIGN)
                    ? true
                    : seq(false, &Analyser::unreadToken, this))) {
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

std::optional<CompilationError> Analyser::analyseAssignmentStatement() {
  auto next = nextToken();

  auto ident = next.value().GetValueString();
  if (!isDeclared(ident))
    return {CompilationError(_current_pos, ErrorCode::ErrNotDeclared)};
  if (isConstant(ident))
    return {CompilationError(_current_pos, ErrorCode::ErrAssignToConstant)};

  if (!(expect(nextToken(), TokenType::EQUAL_SIGN)))
    return {CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};

  auto err = analyseExpression();
  if (err.has_value()) return err;

  if (!isInitializedVariable(ident)) makeInitialized(next.value());
  if (!(expect(nextToken(), TokenType::SEMICOLON)))
    return {CompilationError(_current_pos, ErrorCode::ErrNoSemicolon)};

  _instructions.emplace_back(Operation::STO, getIndex(ident));

  return {};
}

std::optional<CompilationError> Analyser::analyseOutputStatement() {
  auto next = nextToken();

  next = nextToken();
  if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrInvalidPrint);

  auto err = analyseExpression();
  if (err.has_value()) return err;

  next = nextToken();
  if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrInvalidPrint);

  next = nextToken();
  if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
    return std::make_optional<CompilationError>(_current_pos,
                                                ErrorCode::ErrNoSemicolon);

  _instructions.emplace_back(Operation::WRT, 0);
  return {};
}

std::optional<CompilationError> Analyser::analyseItem() {
  auto err = analyseFactor();
  if (err.has_value()) return err;

  while (true) {
    bool mult;
    if ((expect(nextToken(), TokenType::MULTIPLICATION_SIGN)
             ? true
             : seq(false, &Analyser::unreadToken, this))) {
      mult = true;
    } else if ((expect(nextToken(), TokenType::DIVISION_SIGN)
                    ? true
                    : seq(false, &Analyser::unreadToken, this))) {
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

std::optional<CompilationError> Analyser::analyseFactor() {
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

  if ((seq(expect(nextToken(), TokenType::IDENTIFIER), &Analyser::unreadToken,
           this))) {
    auto next = nextToken().value();
    auto ident = next.GetValueString();
    if (!isDeclared(ident))
      return {CompilationError(_current_pos, ErrorCode::ErrNotDeclared)};
    if (!isInitializedVariable(ident) && !isConstant(ident))
      return {CompilationError(_current_pos, ErrorCode::ErrNotInitialized)};

    _instructions.emplace_back(Operation::LOD, getIndex(ident));

  } else if ((seq(expect(nextToken(), TokenType::UNSIGNED_INTEGER),
                  &Analyser::unreadToken, this))) {
    auto next = nextToken().value();

    int32_t val = std::any_cast<int32_t>(next.GetValue()) + 1;
    _instructions.emplace_back(Operation::LIT, val);

  } else if ((expect(nextToken(), TokenType::LEFT_BRACKET)
                  ? true
                  : seq(false, &Analyser::unreadToken, this))) {
    analyseExpression();
    if (!(expect(nextToken(), TokenType::RIGHT_BRACKET)))
      return {
          CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};
  } else {
    return {CompilationError(_current_pos, ErrorCode::ErrIncompleteExpression)};
  }

  if (prefix == -1) _instructions.emplace_back(Operation::SUB, 0);
  return {};
}

std::optional<Token> Analyser::nextToken() {
  if (_offset == _tokens.size()) return {};

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
