#include "analyser/analyser.h"
#include "instruction/instruction.h"
#include "tokenizer/tokenizer.h"

#include <sstream>
#include <vector>

#include "fmt/core.h"
#include "fmts.hpp"
std::ostream& operator<<(std::ostream& os,
                         miniplc0::CompilationError const& t) {
  os << fmt::format("{}", t);
  return os;
}
std::ostream& operator<<(std::ostream& os, miniplc0::Instruction const& t) {
  os << fmt::format("{}", t);
  return os;
}
#include "catch2/catch.hpp"

std::pair<std::vector<miniplc0::Instruction>,
          std::optional<miniplc0::CompilationError>>
analyze(std::string& input) {
  std::stringstream ss(input);
  miniplc0::Tokenizer lexer(ss);
  auto tokens = lexer.AllTokens();
  miniplc0::Analyser parser(tokens.first);
  return parser.Analyse();
}

TEST_CASE("Basic analyzing program") {
  std::string input = "begin end";
  auto result = analyze(input);
  REQUIRE(result.first.size() == 0);
  REQUIRE_FALSE(result.second.has_value());
}

TEST_CASE("Constant storing") {
  std::string input =
      "begin\n"
      "  const test = 1; \n"
      "end";
  auto result = analyze(input);

  std::vector<miniplc0::Instruction> expected = {
      miniplc0::Instruction(miniplc0::Operation::LIT, 1),
      miniplc0::Instruction(miniplc0::Operation::STO, 0),
  };

  REQUIRE(result.first == expected);

  REQUIRE_FALSE(result.second.has_value());
}

TEST_CASE("No assigns to constants") {
  std::string input =
      "begin\n"
      "  const test = 1; \n"
      "  test = 2;\n"
      "end";
  auto result = analyze(input);

  // std::vector<miniplc0::Instruction> expected = {
  //     miniplc0::Instruction(miniplc0::Operation::LIT, 1),
  //     miniplc0::Instruction(miniplc0::Operation::STO, 0),
  // };

  // spotted in tests - remember positions start at zero!
  auto expected = miniplc0::CompilationError(
      2, 6, miniplc0::ErrorCode::ErrAssignToConstant);

  REQUIRE(result.second.value() == expected);
}

TEST_CASE("Variable storing") {
  std::string input =
      "begin\n"
      "  var test = 1; \n"
      "end";
  auto result = analyze(input);

  std::vector<miniplc0::Instruction> expected = {
      miniplc0::Instruction(miniplc0::Operation::LIT, 1),
      miniplc0::Instruction(miniplc0::Operation::STO, 0),
  };

  REQUIRE(result.first == expected);

  REQUIRE_FALSE(result.second.has_value());
}

TEST_CASE("Variables allow assignments") {
  std::string input =
      "begin\n"
      "  var test = 1; \n"
      "  test = 2; \n"
      "end";
  auto result = analyze(input);

  std::vector<miniplc0::Instruction> expected = {
      miniplc0::Instruction(miniplc0::Operation::LIT, 1),
      miniplc0::Instruction(miniplc0::Operation::STO, 0),
      miniplc0::Instruction(miniplc0::Operation::LIT, 2),
      miniplc0::Instruction(miniplc0::Operation::STO, 0),
  };

  REQUIRE(result.first == expected);

  REQUIRE_FALSE(result.second.has_value());
}

TEST_CASE("Variables are stored according to declaration order") {
  std::string input =
      "begin\n"
      "  var test0 = 0; \n"
      "  var test1 = 1; \n"
      "end";
  auto result = analyze(input);

  std::vector<miniplc0::Instruction> expected = {
      miniplc0::Instruction(miniplc0::Operation::LIT, 0),
      miniplc0::Instruction(miniplc0::Operation::STO, 0),
      miniplc0::Instruction(miniplc0::Operation::LIT, 1),
      miniplc0::Instruction(miniplc0::Operation::STO, 1),
  };

  REQUIRE(result.first == expected);

  REQUIRE_FALSE(result.second.has_value());
}
TEST_CASE("Variables without initial values have no instruction") {
  std::string input =
      "begin\n"
      "  var test; \n"
      "  var test1 = 1; \n"
      "  test = 2; \n"
      "end";
  auto result = analyze(input);

  std::vector<miniplc0::Instruction> expected = {
      miniplc0::Instruction(miniplc0::Operation::LIT, 1),
      miniplc0::Instruction(miniplc0::Operation::STO, 1),
      miniplc0::Instruction(miniplc0::Operation::LIT, 2),
      miniplc0::Instruction(miniplc0::Operation::STO, 0),
  };

  REQUIRE(result.first == expected);

  REQUIRE_FALSE(result.second.has_value());
}
