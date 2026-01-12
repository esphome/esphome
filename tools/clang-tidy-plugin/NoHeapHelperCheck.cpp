#include "NoHeapHelperCheck.h"

#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace esphome_tidy {

void NoHeapHelperCheck::registerMatchers(MatchFinder *Finder) {
  // Match std::to_string calls
  Finder->addMatcher(callExpr(callee(functionDecl(hasName("::std::to_string")))).bind("std_to_string"), this);

  // Match esphome::format_hex (all overloads that return std::string)
  // Note: format_hex_to() returns char* and is the preferred alternative
  Finder->addMatcher(callExpr(callee(functionDecl(hasName("format_hex"),
                                                  returns(hasDeclaration(namedDecl(hasName("::std::basic_string")))))))
                         .bind("format_hex"),
                     this);

  // Match esphome::format_mac_address_pretty
  Finder->addMatcher(callExpr(callee(functionDecl(hasName("format_mac_address_pretty")))).bind("format_mac"), this);

  // Match esphome::str_truncate
  Finder->addMatcher(callExpr(callee(functionDecl(hasName("str_truncate")))).bind("str_truncate"), this);

  // Match esphome::str_upper_case
  Finder->addMatcher(callExpr(callee(functionDecl(hasName("str_upper_case")))).bind("str_upper_case"), this);

  // Match esphome::str_snake_case
  Finder->addMatcher(callExpr(callee(functionDecl(hasName("str_snake_case")))).bind("str_snake_case"), this);
}

void NoHeapHelperCheck::check(const MatchFinder::MatchResult &Result) {
  // std::to_string
  if (const auto *Call = Result.Nodes.getNodeAs<clang::CallExpr>("std_to_string")) {
    diag(Call->getBeginLoc(), "std::to_string() allocates heap memory; "
                              "use snprintf() or stack-based formatting instead");
    return;
  }

  // format_hex
  if (const auto *Call = Result.Nodes.getNodeAs<clang::CallExpr>("format_hex")) {
    diag(Call->getBeginLoc(), "format_hex() allocates heap memory; "
                              "use format_hex_to() with a stack buffer instead");
    return;
  }

  // format_mac_address_pretty
  if (const auto *Call = Result.Nodes.getNodeAs<clang::CallExpr>("format_mac")) {
    diag(Call->getBeginLoc(), "format_mac_address_pretty() allocates heap memory; "
                              "use format_mac_addr_upper() with a stack buffer instead");
    return;
  }

  // str_truncate
  if (const auto *Call = Result.Nodes.getNodeAs<clang::CallExpr>("str_truncate")) {
    diag(Call->getBeginLoc(), "str_truncate() allocates heap memory; "
                              "this function is unused and should be removed");
    return;
  }

  // str_upper_case
  if (const auto *Call = Result.Nodes.getNodeAs<clang::CallExpr>("str_upper_case")) {
    diag(Call->getBeginLoc(), "str_upper_case() allocates heap memory; "
                              "this function is unused and should be removed");
    return;
  }

  // str_snake_case
  if (const auto *Call = Result.Nodes.getNodeAs<clang::CallExpr>("str_snake_case")) {
    diag(Call->getBeginLoc(), "str_snake_case() allocates heap memory; "
                              "this function is unused and should be removed");
    return;
  }
}

}  // namespace esphome_tidy
