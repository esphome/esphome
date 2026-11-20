#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "esphome/components/modem/at_reply.h"

namespace esphome::modem::testing {

// Runs a raw reply through flatten_reply() as at_reply_() does
static std::string flat(const char *raw) {
  char buf[256];
  const size_t len = strlen(raw);
  memcpy(buf, raw, len + 1);
  return std::string(buf, flatten_reply(buf, len));
}

static ReplyEnd end_of(const char *reply, bool skip_first = false) {
  return reply_end(reply, strlen(reply), skip_first);
}

TEST(ModemReplyEnd, OnlyCompleteFinalLinesCount) {
  EXPECT_EQ(end_of("\r\n+CSQ: 20,99\r\n\r\nOK\r\n"), ReplyEnd::REPLY_END_OK);
  EXPECT_EQ(end_of("\r\nERROR\r\n"), ReplyEnd::REPLY_END_ERROR);
  EXPECT_EQ(end_of("\r\n+CME ERROR: SIM not inserted\r\n"), ReplyEnd::REPLY_END_ERROR);
  EXPECT_EQ(end_of("\r\n+CMS ERROR: 500\r\n"), ReplyEnd::REPLY_END_ERROR);
  // ERROR inside a payload line is not a result
  EXPECT_EQ(end_of("\r\n+CEER: PROTOCOL_ERROR\r\n"), ReplyEnd::REPLY_END_NONE);
  EXPECT_EQ(end_of("\r\n+CEER: PROTOCOL_ERROR\r\n\r\nOK\r\n"), ReplyEnd::REPLY_END_OK);
  // A line that has not ended yet
  EXPECT_EQ(end_of("\r\nOK"), ReplyEnd::REPLY_END_NONE);
  EXPECT_EQ(end_of("\r\n+CME ERROR: 10"), ReplyEnd::REPLY_END_NONE);
}

TEST(ModemReplyEnd, ErrorWithoutItsReason) {
  // esp_modem drops the CMUX frame ": SIM not inserted" because it holds no line end
  char buf[64];
  size_t len = 0;
  collect_reply(buf, sizeof(buf), len, "\r\n+CME ERROR", 12, false);
  EXPECT_EQ(reply_end(buf, len, false), ReplyEnd::REPLY_END_NONE);
  collect_reply(buf, sizeof(buf), len, "\r\n", 2, false);
  EXPECT_EQ(reply_end(buf, len, false), ReplyEnd::REPLY_END_ERROR);
  EXPECT_EQ(end_of("\r\n+CMS ERROR\r\n"), ReplyEnd::REPLY_END_ERROR);
}

TEST(ModemReplyEnd, CutFirstLineIsSkipped) {
  // The tail of "...OK" from a longer line must not count as a result
  EXPECT_EQ(end_of("OK\r\nmore text\r\n", true), ReplyEnd::REPLY_END_NONE);
  EXPECT_EQ(end_of("OK\r\nmore text\r\n", false), ReplyEnd::REPLY_END_OK);
  EXPECT_EQ(end_of("tail\r\nOK\r\n", true), ReplyEnd::REPLY_END_OK);
}

TEST(ModemCollectReply, UartReplyComesBackWhole) {
  char buf[9];
  size_t len = 0;
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "abc", 3, true));
  EXPECT_STREQ(buf, "abc");
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "abcdef", 6, true));
  EXPECT_STREQ(buf, "abcdef");
  // Data longer than the buffer keeps its tail
  EXPECT_TRUE(collect_reply(buf, sizeof(buf), len, "0123456789", 10, true));
  EXPECT_STREQ(buf, "23456789");
}

TEST(ModemCollectReply, GatheredCmuxFramesAreNotRepeated) {
  // With esp_modem's inflatable buffer, each CMUX frame comes with the ones before it
  char buf[64];
  size_t len = 0;
  const char *first = "\r\n+CSQ: 20,99\r\n";
  const char *both = "\r\n+CSQ: 20,99\r\n\r\nOK\r\n";
  collect_reply(buf, sizeof(buf), len, first, strlen(first), true);
  collect_reply(buf, sizeof(buf), len, both, strlen(both), true);
  EXPECT_STREQ(buf, both);
  EXPECT_EQ(reply_end(buf, len, false), ReplyEnd::REPLY_END_OK);
}

TEST(ModemCollectReply, ExactlyFullIsNotCut) {
  char buf[9];
  size_t len = 0;
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "OK\r\nabcd", 8, true));
  EXPECT_STREQ(buf, "OK\r\nabcd");
  len = 0;
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "OK\r\nabcd", 8, false));
  EXPECT_EQ(reply_end(buf, len, false), ReplyEnd::REPLY_END_OK);
  // Filling up by appending is not a cut either, one more byte is
  len = 0;
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "OK\r\n", 4, false));
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "abcd", 4, false));
  EXPECT_TRUE(collect_reply(buf, sizeof(buf), len, "e", 1, false));
  EXPECT_STREQ(buf, "K\r\nabcde");
}

TEST(ModemCollectReply, CmuxFramesAreAppended) {
  char buf[9];
  size_t len = 0;
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "abc", 3, false));
  // A frame that starts like the text so far is still new data
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "abcd", 4, false));
  EXPECT_STREQ(buf, "abcabcd");
  // The oldest bytes go once the buffer is full
  EXPECT_TRUE(collect_reply(buf, sizeof(buf), len, "XY", 2, false));
  EXPECT_STREQ(buf, "bcabcdXY");
  EXPECT_EQ(len, 8u);
  EXPECT_TRUE(collect_reply(buf, sizeof(buf), len, "0123456789", 10, false));
  EXPECT_STREQ(buf, "23456789");
}

TEST(ModemFlattenReply, OneLineWithoutFinalOk) {
  EXPECT_EQ(flat("\r\n+CSQ: 20,99\r\n\r\nOK\r\n"), "+CSQ: 20,99");
  EXPECT_EQ(flat("\r\nOK\r\n"), "");
  EXPECT_EQ(flat("\r\n+CME ERROR: SIM not inserted\r\n"), "+CME ERROR: SIM not inserted");
  EXPECT_EQ(flat("\r\n+COPS: 0,0,\"NOK\"\r\n"), "+COPS: 0,0,\"NOK\"");
  // A line ending in OK is not the final result
  EXPECT_EQ(flat("\r\n+CSPN: NOK\r\n"), "+CSPN: NOK");
  EXPECT_EQ(flat(""), "");
}

TEST(ModemRegistration, QueryAnswerNotNotification) {
  // A one-field URC before the query answer is skipped
  EXPECT_EQ(registration_from_reply("+CEREG: 2 +CEREG: 0,1", "+CEREG:"), 1);
  EXPECT_EQ(registration_from_reply("+CEREG: 2,5,\"1A2B\",\"01A2B3C4\",7", "+CEREG:"), 5);
  EXPECT_EQ(registration_from_reply("+CREG: 0,3", "+CREG:"), 3);
  EXPECT_EQ(registration_from_reply("+CEREG: 1", "+CEREG:"), REGISTRATION_UNKNOWN);
  EXPECT_EQ(registration_from_reply("+CREG: 0,1", "+CEREG:"), REGISTRATION_UNKNOWN);
  EXPECT_EQ(registration_from_reply("", "+CEREG:"), REGISTRATION_UNKNOWN);
}

TEST(ModemRegistration, OtherRegisteredStates) {
  EXPECT_TRUE(is_registered(REGISTRATION_HOME));
  EXPECT_TRUE(is_registered(REGISTRATION_ROAMING));
  EXPECT_TRUE(is_registered(REGISTRATION_SMS_ONLY_HOME));
  EXPECT_TRUE(is_registered(REGISTRATION_SMS_ONLY_ROAMING));
  EXPECT_TRUE(is_registered(REGISTRATION_CSFB_NOT_PREFERRED_HOME));
  EXPECT_TRUE(is_registered(REGISTRATION_CSFB_NOT_PREFERRED_ROAMING));
  EXPECT_FALSE(is_registered(REGISTRATION_NOT_SEARCHING));
  EXPECT_FALSE(is_registered(REGISTRATION_SEARCHING));
  EXPECT_FALSE(is_registered(REGISTRATION_DENIED));
  EXPECT_FALSE(is_registered(REGISTRATION_REPORTED_UNKNOWN));
  EXPECT_FALSE(is_registered(REGISTRATION_EMERGENCY_ONLY));
  EXPECT_FALSE(is_registered(11));
  EXPECT_FALSE(is_registered(REGISTRATION_UNKNOWN));
}

TEST(ModemMergeRegistration, RegisteredOnEitherSideWins) {
  EXPECT_EQ(merge_registration(REGISTRATION_HOME, REGISTRATION_DENIED), REGISTRATION_HOME);
  EXPECT_EQ(merge_registration(REGISTRATION_DENIED, REGISTRATION_ROAMING), REGISTRATION_ROAMING);
  EXPECT_EQ(merge_registration(REGISTRATION_NO_ANSWER, REGISTRATION_HOME), REGISTRATION_HOME);
  EXPECT_EQ(merge_registration(REGISTRATION_UNSUPPORTED, REGISTRATION_ROAMING), REGISTRATION_ROAMING);
  EXPECT_EQ(merge_registration(REGISTRATION_UNSUPPORTED, REGISTRATION_CSFB_NOT_PREFERRED_ROAMING),
            REGISTRATION_CSFB_NOT_PREFERRED_ROAMING);
  EXPECT_EQ(merge_registration(REGISTRATION_SEARCHING, REGISTRATION_CSFB_NOT_PREFERRED_HOME),
            REGISTRATION_CSFB_NOT_PREFERRED_HOME);
  EXPECT_EQ(merge_registration(REGISTRATION_SMS_ONLY_HOME, REGISTRATION_DENIED), REGISTRATION_SMS_ONLY_HOME);
  // Emergency only is no registration, so a searching query still decides
  EXPECT_EQ(merge_registration(REGISTRATION_EMERGENCY_ONLY, REGISTRATION_SEARCHING), REGISTRATION_SEARCHING);
}

TEST(ModemMergeRegistration, SearchingBeatsDenied) {
  EXPECT_EQ(merge_registration(REGISTRATION_SEARCHING, REGISTRATION_DENIED), REGISTRATION_SEARCHING);
  EXPECT_EQ(merge_registration(REGISTRATION_DENIED, REGISTRATION_SEARCHING), REGISTRATION_SEARCHING);
}

TEST(ModemMergeRegistration, DefiniteAnswerBeatsTimeout) {
  EXPECT_EQ(merge_registration(REGISTRATION_DENIED, REGISTRATION_NO_ANSWER), REGISTRATION_DENIED);
  EXPECT_EQ(merge_registration(REGISTRATION_NO_ANSWER, REGISTRATION_SEARCHING), REGISTRATION_SEARCHING);
  EXPECT_EQ(merge_registration(REGISTRATION_NO_ANSWER, REGISTRATION_NO_ANSWER), REGISTRATION_NO_ANSWER);
  // Any reported <stat> outweighs a timeout, so the fallbacks still count it
  EXPECT_EQ(merge_registration(REGISTRATION_NOT_SEARCHING, REGISTRATION_NO_ANSWER), REGISTRATION_NOT_SEARCHING);
  EXPECT_EQ(merge_registration(REGISTRATION_NO_ANSWER, REGISTRATION_NOT_SEARCHING), REGISTRATION_NOT_SEARCHING);
  EXPECT_EQ(merge_registration(REGISTRATION_REPORTED_UNKNOWN, REGISTRATION_NO_ANSWER), REGISTRATION_REPORTED_UNKNOWN);
  EXPECT_EQ(merge_registration(REGISTRATION_NO_ANSWER, REGISTRATION_REPORTED_UNKNOWN), REGISTRATION_REPORTED_UNKNOWN);
  // Only a timeout and no <stat> at all leave the state open
  EXPECT_EQ(merge_registration(REGISTRATION_UNSUPPORTED, REGISTRATION_NO_ANSWER), REGISTRATION_NO_ANSWER);
  EXPECT_EQ(merge_registration(REGISTRATION_NO_ANSWER, REGISTRATION_UNKNOWN), REGISTRATION_NO_ANSWER);
}

TEST(ModemMergeRegistration, UnsupportedOnlyWhenBothAre) {
  EXPECT_EQ(merge_registration(REGISTRATION_UNSUPPORTED, REGISTRATION_UNSUPPORTED), REGISTRATION_UNSUPPORTED);
  EXPECT_EQ(merge_registration(REGISTRATION_UNSUPPORTED, REGISTRATION_NOT_SEARCHING), REGISTRATION_NOT_SEARCHING);
  EXPECT_EQ(merge_registration(REGISTRATION_UNKNOWN, REGISTRATION_UNSUPPORTED), REGISTRATION_UNKNOWN);
}

TEST(ModemMergeRegistration, UnknownFallback) {
  // The LTE value goes first, then the 2G and 3G one, then unknown
  EXPECT_EQ(merge_registration(REGISTRATION_NOT_SEARCHING, REGISTRATION_REPORTED_UNKNOWN), REGISTRATION_NOT_SEARCHING);
  EXPECT_EQ(merge_registration(REGISTRATION_UNKNOWN, REGISTRATION_REPORTED_UNKNOWN), REGISTRATION_REPORTED_UNKNOWN);
  EXPECT_EQ(merge_registration(REGISTRATION_UNKNOWN, REGISTRATION_UNKNOWN), REGISTRATION_UNKNOWN);
}

TEST(ModemUnsupported, OnlyWhenTheErrorSaysSo) {
  EXPECT_TRUE(reply_names_unsupported("+CME ERROR: operation not supported"));
  EXPECT_TRUE(reply_names_unsupported("+CME ERROR: Operation Not Supported"));
  EXPECT_TRUE(reply_names_unsupported("+CME ERROR: 4"));
  EXPECT_FALSE(reply_names_unsupported("+CME ERROR: 40"));
  EXPECT_FALSE(reply_names_unsupported("+CME ERROR: SIM busy"));
  EXPECT_FALSE(reply_names_unsupported("ERROR"));
  EXPECT_FALSE(reply_names_unsupported(""));
}

TEST(ModemSimStatus, CutBeforeTheNextLine) {
  char ready[] = "+CPIN: READY";
  EXPECT_STREQ(sim_status(ready), "READY");
  char with_urc[] = "+CPIN: SIM PIN  +CGEV: ME PDN DEACT 1";
  EXPECT_STREQ(sim_status(with_urc), "SIM PIN");
  char echoed[] = "AT+CPIN?";
  EXPECT_EQ(sim_status(echoed), nullptr);
  char empty[] = "+CPIN:";
  EXPECT_STREQ(sim_status(empty), "");
}

}  // namespace esphome::modem::testing
