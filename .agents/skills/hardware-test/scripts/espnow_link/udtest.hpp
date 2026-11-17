#pragma once
#include "entest.hpp"
namespace entest {
inline Stream UP2P("p2p_seq", 0x1E, 0x5A);
inline Stream UBCR("bcrel_seq", 0x1E, 0x5B);
inline Stream UP2PF("p2pf_raw", 0xA5, 0x5B);
inline Stream UP2PT("p2pt_raw", 0xA5, 0x5C);
}  // namespace entest
