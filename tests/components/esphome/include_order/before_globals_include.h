#pragma once

// Workaround for the linter to not complain about #define being used for
// constants
constexpr auto defined_first = 2;

#define DEFINED_FIRST defined_first

struct CustomGlobalType {
  int value;
};
