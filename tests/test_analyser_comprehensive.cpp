#include "analyser/analyser.h"
#include "instruction/instruction.h"
#include "tokenizer/tokenizer.h"

#include <sstream>
#include <vector>
#include <random>

#include "fmt/core.h"
#include "fmts.hpp"
#include "simple_vm.hpp"

template <typename T>
std::ostream& operator<<(std::ostream& os, std::optional<T> const& t) {
  if (t.has_value())
    os << fmt::format("Some({})", t.value());
  else
    os << "None";
  return os;
}

#include "catch2/catch.hpp"
auto engine = std::minstd_rand0();

// Generator function Fn(std::stringstream&, bool) -> ()

void generate_spaces(std::stringstream& ss, bool invalid = false) {
  const int space_max = 2;
  int space_cnt = std::uniform_int_distribution<>(1, space_max)(engine);
  for (int i = 0; i < space_cnt; i++) {
    if (!invalid) {
      bool use_lf = std::uniform_int_distribution<>(0, 50)(engine) < 3;
      if (use_lf)
        ss << '\n';
      else
        ss << ' ';
    } else {
      int use = rand() % 3;
      switch (use) {
        case 1:
          ss << '\n';
          break;
        case 2:
          ss << ' ';
          break;
        case 3:
          ss << '\t';
          break;
        default:
          break;
      }
    }
  }
}

// Comprehensive tests of the analyzer
void generate_uint(std::stringstream& ss, bool invalid) {
  if (!invalid) {
    int32_t x = std::uniform_int_distribution<int64_t>(0, INT32_MAX)(engine);
    ss << fmt::format("{}", x);
  } else {
    int64_t x = std::uniform_int_distribution<int64_t>((int64_t)INT32_MAX + 1,
                                                       INT64_MAX)(engine);
    ss << fmt::format("{}", x);
  }
}

// int64_t __counter = 0;
// int64_t get_counter() { return __counter++; }

std::string generate_unique_ident(bool invalid = false) {
  // auto engine = std::minstd_rand0);
  auto distributor = std::uniform_int_distribution<int>(0, 10 + 26 + 26 - 1);
  int len = std::uniform_int_distribution<>(5, 32)(engine);
  std::string ss;

  if (invalid) {
    char start = std::uniform_int_distribution<char>('0', '9')(engine);
    ss.push_back(start);
  } else {
    char start = std::uniform_int_distribution<char>('a', 'z')(engine);
    bool caps = std::uniform_int_distribution<char>(0, 1)(engine);
    if (caps) start = start + ('A' - 'a');
    ss.push_back(start);
  }

  for (int i = 1; i < len; i++) {
    int c = distributor(engine);
    if (c < 10) {
      ss.push_back((char)(c + '0'));
    } else if (c < 36) {
      ss.push_back((char)(c - 10 + 'a'));
    } else {
      ss.push_back((char)(c - 36 + 'A'));
    }
  }
  return ss;
}

void generate_kw_begin(std::stringstream& ss, bool invalid) {
  if (!invalid)
    ss << "begin";
  else {
    // we just delegate that to identifier.
    ss << generate_unique_ident();
  }
}
void generate_kw_end(std::stringstream& ss, bool invalid) {
  if (!invalid)
    ss << "end";
  else {
    // we just delegate that to identifier.
    ss << generate_unique_ident();
  }
}
void generate_kw_const(std::stringstream& ss, bool invalid) {
  if (!invalid)
    ss << "const";
  else {
    // we just delegate that to identifier.
    ss << generate_unique_ident();
  }
}
void generate_kw_var(std::stringstream& ss, bool invalid) {
  if (!invalid)
    ss << "var";
  else {
    // we just delegate that to identifier.
    ss << generate_unique_ident();
  }
}
void generate_kw_print(std::stringstream& ss, bool invalid) {
  if (!invalid)
    ss << "print";
  else {
    // we just delegate that to identifier.
    ss << generate_unique_ident();
  }
}

void generate_const_decl(std::stringstream& ss, bool invalid,
                         std::string& name) {
  int invalid_place =
      invalid ? std::uniform_int_distribution<>(1, 7)(engine) : 0;
  int sign = std::uniform_int_distribution<>(0, 2)(engine);

  generate_kw_const(ss, invalid_place == 1);
  generate_spaces(ss);
  ss << name;
  generate_spaces(ss);
  ss << '=';
  generate_spaces(ss);

  if (sign == 1)
    ss << '+';
  else if (sign == 2)
    ss << '-';
  generate_uint(ss, invalid_place == 2);
  ss << ';';
  generate_spaces(ss);
}

void generate_var_decl(std::stringstream& ss, bool invalid, std::string& name) {
  int invalid_place =
      invalid ? std::uniform_int_distribution<>(1, 7)(engine) : 0;
  int sign = std::uniform_int_distribution<>(0, 2)(engine);

  generate_kw_var(ss, invalid_place == 1);
  generate_spaces(ss);
  ss << name;
  generate_spaces(ss);
  ss << '=';
  generate_spaces(ss);
  if (sign == 1)
    ss << '+';
  else if (sign == 2)
    ss << '-';
  generate_uint(ss, invalid_place == 2);
  ss << ';';
  generate_spaces(ss);
}

void generate_u_var_decl(std::stringstream& ss, bool invalid,
                         std::string& name) {
  int invalid_place =
      invalid ? std::uniform_int_distribution<>(1, 7)(engine) : 0;
  int sign = std::uniform_int_distribution<>(0, 2)(engine);

  generate_kw_var(ss, invalid_place == 1);
  generate_spaces(ss);
  ss << name;
  generate_spaces(ss);
  ss << ';';
  generate_spaces(ss);
}

void generate_var_assignment(std::stringstream& ss, bool invalid,
                             std::string& asn_name,
                             std::vector<std::string>& var_list) {}

void generate_main(std::stringstream& ss, bool invalid = false) {
  int invalid_place =
      invalid ? std::uniform_int_distribution<>(1, 7)(engine) : 0;

  int const_cnt = std::uniform_int_distribution<>(0, 15)(engine);
  int u_var_cnt = std::uniform_int_distribution<>(0, 15)(engine);
  int var_cnt = std::uniform_int_distribution<>(0, 15)(engine);

  // generate variable names
  std::vector<std::string> const_names;
  int const_inv = invalid_place == 1 ? std::uniform_int_distribution<>(
                                           0, const_cnt - 1)(engine)
                                     : -1;
  for (int i = 0; i < const_cnt; i++) {
    const_names.push_back(generate_unique_ident(const_inv == i));
  }

  std::vector<std::string> var_names;
  int var_inv = invalid_place == 2
                    ? std::uniform_int_distribution<>(0, var_cnt - 1)(engine)
                    : -1;
  for (int i = 0; i < var_cnt; i++) {
    const_names.push_back(generate_unique_ident(var_inv == i));
  }

  std::vector<std::string> u_var_names;
  int u_var_inv = invalid_place == 3 ? std::uniform_int_distribution<>(
                                           0, u_var_cnt - 1)(engine)
                                     : -1;
  for (int i = 0; i < u_var_cnt; i++) {
    u_var_names.push_back(generate_unique_ident(u_var_inv == i));
  }

  for (auto& const_ : const_names) {
    generate_const_decl(ss, invalid_place == 4, const_);
    generate_spaces(ss);
  }

  for (auto& var_ : var_names) {
    generate_var_decl(ss, invalid_place == 5, var_);
    generate_spaces(ss);
  }

  for (auto& var_ : u_var_names) {
    generate_u_var_decl(ss, invalid_place == 6, var_);
    generate_spaces(ss);
  }
}

void generate_program(std::stringstream& ss, bool invalid = false) {
  int invalid_place =
      invalid ? std::uniform_int_distribution<>(1, 3)(engine) : 0;

  generate_kw_begin(ss, invalid_place == 1);
  generate_spaces(ss);
  generate_main(ss, invalid_place == 2);
  generate_spaces(ss);
  generate_kw_end(ss, invalid_place == 3);
}

TEST_CASE("Random passing program") {
  const int test_cnt = 500;
  for (int i = 0; i < test_cnt; i++) {
    SECTION(fmt::format("Test run {}/{}", i, test_cnt)) {
      std::stringstream ss;
      generate_program(ss, false);
      CAPTURE(ss.str());
      miniplc0::Tokenizer lexer(ss);
      auto tokens = lexer.AllTokens();
      miniplc0::Analyser parser(tokens.first);
      auto result = parser.Analyse();
      CAPTURE(result.second);
      REQUIRE_FALSE(result.second.has_value());
    }
  }
}
