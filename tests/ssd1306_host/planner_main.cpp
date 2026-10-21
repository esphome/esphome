// Requires the isolated headers and flags from script/test_ssd1306_partial.py.
#ifdef SSD1306_STANDALONE_TEST

#include <gtest/gtest.h>
int main() { return standalone_test::run(); }

#endif  // SSD1306_STANDALONE_TEST
