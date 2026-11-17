#pragma once
#include <string>
#include <cstdio>
// 800 chars: 8 digit n, '|', then letters from n and the slot
static std::string make_txt(uint32_t n, int slot) {
  std::string s(800, ' ');
  char head[16];
  snprintf(head, sizeof(head), "%08u|", (unsigned) n);
  for (int i = 0; i < 9; i++)
    s[i] = head[i];
  for (int i = 9; i < 800; i++)
    s[i] = 'A' + (n * 7 + slot * 3 + i) % 26;
  return s;
}
