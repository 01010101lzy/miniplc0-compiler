#include "tokenizer/tokenizer.h"

#include <cctype>
#include <sstream>
#include <string>

namespace miniplc0 {

std::pair<std::optional<Token>, std::optional<CompilationError>>
Tokenizer::NextToken() {
  if (!_initialized)
    readAll();
  if (_rdr.bad())
    return std::make_pair(
        std::optional<Token>(),
        std::make_optional<CompilationError>(0, 0, ErrorCode::ErrStreamError));
  if (isEOF())
    return std::make_pair(
        std::optional<Token>(),
        std::make_optional<CompilationError>(0, 0, ErrorCode::ErrEOF));
  auto p = nextToken();
  if (p.second.has_value())
    return std::make_pair(p.first, p.second);
  auto err = checkToken(p.first.value());
  if (err.has_value())
    return std::make_pair(p.first, err.value());
  return std::make_pair(p.first, std::optional<CompilationError>());
}

std::pair<std::vector<Token>, std::optional<CompilationError>>
Tokenizer::AllTokens() {
  std::vector<Token> result;
  while (true) {
    auto p = NextToken();
    if (p.second.has_value()) {
      if (p.second.value().GetCode() == ErrorCode::ErrEOF)
        return std::make_pair(result, std::optional<CompilationError>());
      else
        return std::make_pair(std::vector<Token>(), p.second);
    }
    result.emplace_back(p.first.value());
  }
}

// 注意：这里的返回值中 Token 和 CompilationError 只能返回一个，不能同时返回。
//
// this function is rearranged by Rynco.
//
std::pair<std::optional<Token>, std::optional<CompilationError>>
Tokenizer::nextToken() {
  // read in a character
  auto current_char = nextChar();

  // 已经读到了文件尾
  if (!current_char.has_value())
    // 返回一个空的token，和编译错误ErrEOF：遇到了文件尾
    return {std::optional<Token>(),
            std::make_optional<CompilationError>(0, 0, ErrEOF)};

  std::pair<int64_t, int64_t> pos;
  std::stringstream ss;

  // check which category this character is in.
  auto cur = current_char.value();
  if (miniplc0::isspace(cur)) {
    // skip_spaces
    while (current_char.has_value() && miniplc0::isspace(current_char.value()))
      current_char = nextChar();
    unreadLast();

    // We are sure the next char is not space, so we can call this function
    // again. This is not infinitely recursive!
    return nextToken();
  } else if (miniplc0::isalpha(cur)) {
    // lex_ident
    pos = currentPos();

    // while isalnum(cur)
    while (current_char.has_value() &&
           (miniplc0::isalpha(current_char.value()) ||
            miniplc0::isdigit(current_char.value()))) {
      ss << current_char.value();
      current_char = nextChar();
    }

    // the last char is not alnum. unread.
    unreadLast();

    auto s = ss.str();
    TokenType typ;

    if (s == "begin") {
      typ = TokenType::BEGIN;
    } else if (s == "end") {
      typ = TokenType::END;
    } else if (s == "var") {
      typ = TokenType::VAR;
    } else if (s == "const") {
      typ = TokenType::CONST;
    } else if (s == "print") {
      typ = TokenType::PRINT;
    } else {
      typ = TokenType::IDENTIFIER;
    }

    return {std::make_optional<Token>(typ, s, pos, nextPos()),
            std::optional<CompilationError>()};

  } else if (miniplc0::isdigit(cur)) {
    // lex_uint
    pos = currentPos();

    while (current_char.has_value() && miniplc0::isdigit(current_char.value())) {
      ss << current_char.value();
      current_char = nextChar();
    }

    unreadLast();

    auto s = ss.str();

    auto ui = std::stoll(s);
    if (errno == 0) {
      return {std::make_optional<Token>(TokenType::UNSIGNED_INTEGER, ss.str(),
                                        pos, nextPos()),
              std::optional<CompilationError>()};
    } else if (errno == ERANGE) {
      return {std::optional<Token>(), std::make_optional<CompilationError>(
                                          pos, ErrorCode::ErrIntegerOverflow)};
    } else {
      // Unknown error
      return {std::optional<Token>(), std::make_optional<CompilationError>(
                                          pos, ErrorCode::ErrInvalidInput)};
    }

  } else {
    // other chars
    switch (cur) {
      case '+':
        return {std::make_optional<Token>(TokenType::PLUS_SIGN, '+', pos,
                                          nextPos()),
                std::optional<CompilationError>()};
      case '-':
        return {std::make_optional<Token>(TokenType::MINUS_SIGN, '-', pos,
                                          nextPos()),
                std::optional<CompilationError>()};
      case '*':
        return {std::make_optional<Token>(TokenType::MULTIPLICATION_SIGN, '*',
                                          pos, nextPos()),
                std::optional<CompilationError>()};
      case '/':
        return {std::make_optional<Token>(TokenType::DIVISION_SIGN, '/', pos,
                                          nextPos()),
                std::optional<CompilationError>()};
      case '=':
        return {std::make_optional<Token>(TokenType::EQUAL_SIGN, '=', pos,
                                          nextPos()),
                std::optional<CompilationError>()};
      case '(':
        return {std::make_optional<Token>(TokenType::LEFT_BRACKET, '(', pos,
                                          nextPos()),
                std::optional<CompilationError>()};
      case ')':
        return {std::make_optional<Token>(TokenType::RIGHT_BRACKET, ')', pos,
                                          nextPos()),
                std::optional<CompilationError>()};
      case ';':
        return {std::make_optional<Token>(TokenType::SEMICOLON, ';', pos,
                                          nextPos()),
                std::optional<CompilationError>()};
      default:
        return std::make_pair(std::optional<Token>(),
                              std::make_optional<CompilationError>(
                                  pos, ErrorCode::ErrInvalidInput));
    }
  }
}

std::optional<CompilationError> Tokenizer::checkToken(const Token& t) {
  switch (t.GetType()) {
    case IDENTIFIER: {
      auto val = t.GetValueString();
      if (miniplc0::isdigit(val[0]))
        return std::make_optional<CompilationError>(
            t.GetStartPos().first, t.GetStartPos().second,
            ErrorCode::ErrInvalidIdentifier);
      break;
    }
    default:
      break;
  }
  return {};
}

void Tokenizer::readAll() {
  if (_initialized)
    return;
  for (std::string tp; std::getline(_rdr, tp);)
    _lines_buffer.emplace_back(std::move(tp + "\n"));
  _initialized = true;
  _ptr = std::make_pair<int64_t, int64_t>(0, 0);
  return;
}

// Note: We allow this function to return a postion which is out of bound
// according to the design like std::vector::end().
std::pair<uint64_t, uint64_t> Tokenizer::nextPos() {
  if (_ptr.first >= _lines_buffer.size())
    DieAndPrint("advance after EOF");
  if (_ptr.second == _lines_buffer[_ptr.first].size() - 1)
    return std::make_pair(_ptr.first + 1, 0);
  else
    return std::make_pair(_ptr.first, _ptr.second + 1);
}

std::pair<uint64_t, uint64_t> Tokenizer::currentPos() {
  return _ptr;
}

std::pair<uint64_t, uint64_t> Tokenizer::previousPos() {
  if (_ptr.first == 0 && _ptr.second == 0)
    DieAndPrint("previous position from beginning");
  if (_ptr.second == 0)
    return std::make_pair(_ptr.first - 1,
                          _lines_buffer[_ptr.first - 1].size() - 1);
  else
    return std::make_pair(_ptr.first, _ptr.second - 1);
}

std::optional<char> Tokenizer::nextChar() {
  if (isEOF())
    return {};  // EOF
  auto result = _lines_buffer[_ptr.first][_ptr.second];
  _ptr = nextPos();
  return result;
}

bool Tokenizer::isEOF() {
  return _ptr.first >= _lines_buffer.size();
}

// Note: Is it evil to unread a buffer?
void Tokenizer::unreadLast() {
  _ptr = previousPos();
}
}  // namespace miniplc0
