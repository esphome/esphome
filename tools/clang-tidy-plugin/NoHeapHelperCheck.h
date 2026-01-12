#pragma once

#include "clang-tidy/ClangTidyCheck.h"

namespace esphome_tidy {

/// Clang-tidy check that flags heap-allocating helper functions.
///
/// These helpers return std::string, causing heap allocations that fragment
/// memory on long-running embedded devices. Each has a stack-based alternative.
///
/// Flagged functions:
/// - esphome::format_hex() -> use format_hex_to() with stack buffer
/// - esphome::format_mac_address_pretty() -> use format_mac_addr_upper()
/// - esphome::str_truncate() -> unused, remove
/// - esphome::str_upper_case() -> unused, remove
/// - esphome::str_snake_case() -> unused, remove
/// - std::to_string() -> use snprintf or format helpers
class NoHeapHelperCheck : public clang::tidy::ClangTidyCheck {
 public:
  using ClangTidyCheck::ClangTidyCheck;

  void registerMatchers(clang::ast_matchers::MatchFinder *Finder) override;
  void check(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;
};

}  // namespace esphome_tidy
