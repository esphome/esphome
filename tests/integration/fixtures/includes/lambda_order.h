#pragma once
#include <string>

static constexpr int HEADER_CONSTANT = 42;

inline std::string header_text() { return "from-header"; }

// Names the global by its id, so the header must come after the id declarations.
inline int header_reads_global() { return header_value->value() * 2; }
