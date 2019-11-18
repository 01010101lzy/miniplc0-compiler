#include "analyser/analyser.h"
#include "instruction/instruction.h"
#include "tokenizer/tokenizer.h"

#include <sstream>
#include <vector>

#include "fmt/core.h"
#include "fmts.hpp"
#include "simple_vm.hpp"
std::ostream& operator<<(std::ostream& os,
                         miniplc0::CompilationError const& t) {
  os << fmt::format("{}", t);
  return os;
}
std::ostream& operator<<(std::ostream& os, miniplc0::Instruction const& t) {
  os << fmt::format("{}", t);
  return os;
}

// template <typename T>
// struct formatter<std::vector<T>> {
//   template <typename ParseContext>
//   constexpr auto parse(ParseContext& ctx) {
//     return ctx.begin();
//   }

//   template <typename FormatContext>
//   auto format(const std::vector<T>& p, FormatContext& ctx) {
//     auto res = format_to(ctx.out(), "[");
//     for (auto& i : p) format_to(ctx.out(), "{}, ", i);
//     return format_to(ctx.out(), "]");
//   }
// };
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
      // miniplc0::Instruction(miniplc0::Operation::STO, 0),
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
      // miniplc0::Instruction(miniplc0::Operation::STO, 0),
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
      // miniplc0::Instruction(miniplc0::Operation::STO, 0),
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
      // miniplc0::Instruction(miniplc0::Operation::STO, 0),
      miniplc0::Instruction(miniplc0::Operation::LIT, 1),
      // miniplc0::Instruction(miniplc0::Operation::STO, 1),
  };

  REQUIRE(result.first == expected);

  REQUIRE_FALSE(result.second.has_value());
}

TEST_CASE("Variables without initial values are initialized with 0") {
  std::string input =
      "begin\n"
      "  var test; \n"
      "  var test1 = 1; \n"
      "  test = 2; \n"
      "end";
  auto result = analyze(input);

  std::vector<miniplc0::Instruction> expected = {
      miniplc0::Instruction(miniplc0::Operation::LIT, 0),
      // miniplc0::Instruction(miniplc0::Operation::STO, 0),
      miniplc0::Instruction(miniplc0::Operation::LIT, 1),
      // miniplc0::Instruction(miniplc0::Operation::STO, 1),
      miniplc0::Instruction(miniplc0::Operation::LIT, 2),
      miniplc0::Instruction(miniplc0::Operation::STO, 0),
  };

  REQUIRE(result.first == expected);

  REQUIRE_FALSE(result.second.has_value());
}

TEST_CASE("Constants and variables act the same in programs") {
  std::string input =
      "begin\n"
      "  const a = 1; \n"
      "  var b = 2; \n"
      "  var c; \n"
      "  c = 3;\n"
      "  print(a+b+c); \n"
      "end";
  auto result = analyze(input);

  REQUIRE_FALSE(result.second.has_value());

  auto vm = miniplc0::VM(result.first);
  CAPTURE(result.first);

  auto vm_result = vm.Run();

  REQUIRE(vm_result == std::vector{6});
}

/* ======== Errors ======== */

TEST_CASE("ENoBegin: Main should has 'begin'") {
  std::string input =
      "  var test; \n"
      "end";
  auto result = analyze(input);

  REQUIRE(result.second.has_value());
  REQUIRE(result.second.value().GetCode() == miniplc0::ErrorCode::ErrNoBegin);
}

TEST_CASE("ENoBegin: Main should has 'end'") {
  std::string input =
      "begin \n"
      "  var test; \n"
      "";
  auto result = analyze(input);

  REQUIRE(result.second.has_value());
  REQUIRE(result.second.value().GetCode() == miniplc0::ErrorCode::ErrNoEnd);
}

TEST_CASE("EConstantNeedValue: Constants must be initialized") {
  std::string input =
      "begin \n"
      "  const test; \n"
      "end";
  auto result = analyze(input);

  REQUIRE(result.second.has_value());
  REQUIRE(result.second.value().GetCode() ==
          miniplc0::ErrorCode::ErrConstantNeedValue);
}

TEST_CASE("ENeedIdentifier: Assignments need identifiers") {
  std::string input =
      "begin \n"
      "  var = 4; \n"
      "end";
  auto result = analyze(input);

  REQUIRE(result.second.has_value());
  REQUIRE(result.second.value().GetCode() ==
          miniplc0::ErrorCode::ErrNeedIdentifier);
}

TEST_CASE(
    "ENeedIdentifier: Assignments need identifiers, not other token types") {
  // this test is essensially the same as the last one. Anything other than
  // <token> is reported in this case.
  std::string input =
      "begin \n"
      "  var 1 = 4; \n"
      "end";
  auto result = analyze(input);

  REQUIRE(result.second.has_value());
  REQUIRE(result.second.value().GetCode() ==
          miniplc0::ErrorCode::ErrNeedIdentifier);
}

TEST_CASE("ENotDeclared: Variable cannot be used without declaration") {
  SECTION("As LValue") {
    std::string input =
        "begin\n"
        "  test = 1; \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrNotDeclared);
  }

  SECTION("As RValue") {
    std::string input =
        "begin\n"
        "  var test1 = test; \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrNotDeclared);
  }
}

TEST_CASE("ENotInitialized: Uninitialized variable cannot be used") {
  std::string input =
      "begin\n"
      "  var test; \n"
      "  var test1 = test; \n"
      "end";
  auto result = analyze(input);

  REQUIRE(result.second.has_value());
  REQUIRE(result.second.value().GetCode() ==
          miniplc0::ErrorCode::ErrNotInitialized);
}

TEST_CASE("EAssignToConstant: Const cannot be assigned") {
  std::string input =
      "begin\n"
      "  const test = 1; \n"
      "  test = 1; \n"
      "end";
  auto result = analyze(input);

  REQUIRE(result.second.has_value());
  REQUIRE(result.second.value().GetCode() ==
          miniplc0::ErrorCode::ErrAssignToConstant);
}

TEST_CASE("EDuplicateDeclaration: Variables are declared only once") {
  SECTION("Crashing consts with consts") {
    std::string input =
        "begin\n"
        "  const test = 1; \n"
        "  const test = 1; \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrDuplicateDeclaration);
  }
  SECTION("Crashing consts with vars") {
    std::string input =
        "begin\n"
        "  const test = 1; \n"
        "  var test = 1; \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrDuplicateDeclaration);
  }
  SECTION("Crashing vars with vars") {
    std::string input =
        "begin\n"
        "  var test = 1; \n"
        "  var test = 1; \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrDuplicateDeclaration);
  }
}

TEST_CASE("ENeedSemicolon: Semicolons are needed at every statement") {
  SECTION("Semicolon in const declaration") {
    std::string input =
        "begin\n"
        "  const test = 1 \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrNoSemicolon);
  }

  SECTION("Semicolon in var declaration") {
    std::string input =
        "begin\n"
        "  var test = 1 \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrNoSemicolon);
  }

  SECTION("Semicolon in expression") {
    std::string input =
        "begin\n"
        "  var test; \n"
        "  test = 1 \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrNoSemicolon);
  }

  SECTION("Semicolon in print statement") {
    std::string input =
        "begin\n"
        "  print(1) \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrNoSemicolon);
  }
}

TEST_CASE("EIncompleteExpression: When parameters don't match") {
  SECTION("After a addition operator") {
    std::string input =
        "begin\n"
        "  var test = 1 + ; \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrIncompleteExpression);
  }
  SECTION("After a mutiplication operator") {
    std::string input =
        "begin\n"
        "  var test = 1 * ; \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrIncompleteExpression);
  }
  SECTION("With parentheses") {
    std::string input =
        "begin\n"
        "  var test = (1 + 1; \n"
        "end";
    auto result = analyze(input);

    REQUIRE(result.second.has_value());
    REQUIRE(result.second.value().GetCode() ==
            miniplc0::ErrorCode::ErrIncompleteExpression);
  }
}
