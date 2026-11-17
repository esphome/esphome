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

static std::string logged(const char *cmd) {
  char buf[32];
  return loggable_command(cmd, buf, sizeof(buf));
}

static std::string hidden(const char *reply) {
  char buf[256];
  const size_t len = strlen(reply);
  memcpy(buf, reply, len + 1);
  const size_t new_len = hide_secret_args(buf, len);
  EXPECT_EQ(strlen(buf), new_len);
  return buf;
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

TEST(ModemReplyEnd, CutFirstLineIsSkipped) {
  // The tail of "...OK" from a longer line must not count as a result
  EXPECT_EQ(end_of("OK\r\nmore text\r\n", true), ReplyEnd::REPLY_END_NONE);
  EXPECT_EQ(end_of("OK\r\nmore text\r\n", false), ReplyEnd::REPLY_END_OK);
  EXPECT_EQ(end_of("tail\r\nOK\r\n", true), ReplyEnd::REPLY_END_OK);
}

TEST(ModemCollectReply, GrowsAndKeepsNewestBytes) {
  char buf[9];
  size_t len = 0;
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "abc", 3));
  EXPECT_STREQ(buf, "abc");
  // A UART reply comes back whole, starting with what was collected
  EXPECT_FALSE(collect_reply(buf, sizeof(buf), len, "abcdef", 6));
  EXPECT_STREQ(buf, "abcdef");
  // A CMUX frame is new data and is appended; the oldest bytes go
  EXPECT_TRUE(collect_reply(buf, sizeof(buf), len, "XYZ", 3));
  EXPECT_STREQ(buf, "bcdefXYZ");
  EXPECT_EQ(len, 8u);
  // Data longer than the buffer keeps its tail
  EXPECT_TRUE(collect_reply(buf, sizeof(buf), len, "0123456789", 10));
  EXPECT_STREQ(buf, "23456789");
}

TEST(ModemFlattenReply, OneLineWithoutFinalOk) {
  EXPECT_EQ(flat("\r\n+CSQ: 20,99\r\n\r\nOK\r\n"), "+CSQ: 20,99");
  EXPECT_EQ(flat("\r\nOK\r\n"), "");
  EXPECT_EQ(flat("\r\n+CME ERROR: SIM not inserted\r\n"), "+CME ERROR: SIM not inserted");
  EXPECT_EQ(flat("\r\n+COPS: 0,0,\"NOK\"\r\n"), "+COPS: 0,0,\"NOK\"");
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

TEST(ModemSecrets, CommandArgumentsHidden) {
  EXPECT_EQ(logged("AT+CPIN=1234"), "AT+CPIN=<hidden>");
  EXPECT_EQ(logged("at+cpin=1234"), "at+cpin=<hidden>");
  EXPECT_EQ(logged("AT+CPIN2=5678"), "AT+CPIN2=<hidden>");
  EXPECT_EQ(logged("at+clck=\"SC\",1,\"1234\""), "at+clck=<hidden>");
  EXPECT_EQ(logged("AT+CPWD=\"SC\",\"1111\",\"2222\""), "AT+CPWD=<hidden>");
  EXPECT_EQ(logged("ATE1;+CPIN=1234"), "ATE1;+CPIN=<hidden>");
  EXPECT_EQ(logged("AT+CPIN?"), "AT+CPIN?");
  EXPECT_EQ(logged("at+cpin?"), "at+cpin?");
  EXPECT_EQ(logged("AT+CSQ"), "AT+CSQ");
}

TEST(ModemSecrets, EchoedArgumentsDropped) {
  EXPECT_EQ(hidden("at+cpin=1234 OK"), "at+cpin= OK");
  EXPECT_EQ(hidden("AT+CPIN=1234"), "AT+CPIN=");
  EXPECT_EQ(hidden("at+clck=\"SC\",1,\"12 34\" +CME ERROR: incorrect password"),
            "at+clck= +CME ERROR: incorrect password");
  EXPECT_EQ(hidden("AT+CPIN=1 AT+cPwD=\"SC\",\"1\",\"2\" ERROR"), "AT+CPIN= AT+cPwD= ERROR");
  EXPECT_EQ(hidden("+CPIN: READY"), "+CPIN: READY");
  EXPECT_EQ(hidden("+CLCK: 0"), "+CLCK: 0");
  EXPECT_EQ(hidden(""), "");
}

}  // namespace esphome::modem::testing
