#include <sstream>
#include <vector>

#include "catch2/catch.hpp"
#include "fmt/core.h"
#include "tokenizer/tokenizer.h"

// 下面是示例如何书写测试用例
TEST_CASE("Test keywords") {
  std::string input = "begin end var const print";
  std::stringstream ss(input);
  miniplc0::Tokenizer lexer(ss);
  auto result = lexer.AllTokens();

  std::vector<miniplc0::Token> expected = {};

  if (result.second.has_value()) {
    FAIL("Error introduced in lexing keywords");
  } else {
    REQUIRE(result.first == expected);
  }
}
