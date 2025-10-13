#pragma once

#ifndef _UTILS_H
#define _UTILS_H

#include "esphome/core/log.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <cstring>
#include "types.h"
#include<time.h>
#include <map>
#include <set>
//#include <Arduino.h>

//----------------------------------------------------------------------------------
//  Function Declareration
//----------------------------------------------------------------------------------
void dumpHex(uint8_t* data, int len, bool newLine = true);
void dumpInt(uint8_t* data, int len, bool newLine = true);
unsigned char* safeButUnsafeVectorPtr(std::vector<unsigned char>& v);
std::string str_snprintf(const char* fmt, size_t len, ...);
//std::string to_string(int value);
char format_hex_pretty_char(uint8_t v);
std::string format_hex_pretty(const uint16_t* data, size_t length);
std::string format_hex_pretty(const uint8_t* data, size_t lengt);
std::string format_hex_pretty(std::vector<unsigned char> data);
void phex(uint8_t* str, int len = 16, int start = 0);
void printHexString(uint8_t* str, int len = 16, int start = 0);
void strprintf(std::string* s, const char* fmt, ...);
std::string tostrprintf(const char* fmt, ...);
std::string tostrprintf(const std::string fmt, ...);
std::string bin2hex(const std::vector<uchar>& target);
std::string bin2hex(std::vector<uchar>::iterator data, std::vector<uchar>::iterator end, int len);
std::string bin2hex(std::vector<uchar>& data, int offset, int len);
bool hex2bin(const char* src, std::vector<uchar>* target);
bool hex2bin(const std::string& src, std::vector<uchar>* target);
bool hex2bin(std::vector<uchar>& src, std::vector<uchar>* target);
void warning_x(const char* file_name, int line, const char* fmt, ...);
void error_x(const char* file_name, int line, const char* fmt, ...);
void debug_x(const char* file_name, int line, const char* fmt, ...);
void verbose_x(const char* file_name, int line, const char* fmt, ...);
void trace_x(const char* file_name, int line, const char* fmt, ...);
bool parseExtras(const std::string& s, std::map<std::string, std::string>* extras);
char const hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A','B','C','D','E','F' };
std::string format3fdot3f(double v);
uint16_t crc16_EN13757(uchar* data, size_t len);
bool isTraceEnabled();
bool isDebugEnabled();
bool isVerboseEnabled();
std::string toIdsCommaSeparated(std::vector<std::string>& ids);
// Joing two status strings with a space, but merge OKs.
// I.e. "OK" + "OK" --> "OK"
//      "ERROR" + "OK"  --> "ERROR"
//      "OK" + "ERROR FLOW" --> "ERROR FLOW"
//      "ERROR" + "FLOW"    --> "ERROR FLOW"
//      It also translates empty strings into OK.
//      "" + "OK" --> "OK"
//      "" + "" --> "OK"
std::string joinStatusOKStrings(const std::string& a, const std::string& b);

// Same but do not introduce OK, keep empty strings empty.
std::string joinStatusEmptyStrings(const std::string& a, const std::string& b);

// Sort the words in a status string: "GAMMA BETA ALFA" --> "ALFA BETA GAMMA"
// Also identical flags are merged: "BETA ALFA ALFA" --> "ALFA BETA"
// Finally ~ is replaced with a space, this is only used for backwards compatibilty for deprecated fields.
std::string sortStatusString(const std::string& a);
// Split s into strings separated by c.
std::vector<std::string> splitString(const std::string& s, char c);
// Split s into strings separated by c and store inte set.
std::set<std::string> splitStringIntoSet(const std::string& s, char c);
// A BCD string 102030405060 is reversed to 605040302010
std::string reverseBCD(const std::string& v);
// A hex string encoding ascii chars is reversed and safely translated into a readble string.
std::string reverseBinaryAsciiSafeToString(const std::string& v);
// Check if hex string is likely to be ascii
bool isLikelyAscii(const std::string& v);
void addMonths(struct tm* date, int m);
double addMonths(double t, int m);
bool startsWith(const std::string& s, const char* prefix);
bool startsWith(const std::string& s, std::string& prefix);

// Return for example: 2010-03-21
std::string strdate(struct tm* date);
std::string strdate(double v);
// Return for example: 2010-03-21 15:22
std::string strdatetime(struct tm* date);
std::string strdatetime(double v);
// Return for example: 2010-03-21 15:22:03
std::string strdatetimesec(struct tm* date);
std::string strdatetimesec(double v);
// Eat characters from the vector v, iterating using i, until the end char c is found.
// If end char == -1, then do not expect any end char, get all until eof.
// If the end char is not found, return error.
// If the maximum length is reached without finding the end char, return error.
std::string eatTo(std::vector<uchar>& v, std::vector<uchar>::iterator& i, int c, size_t max, bool* eof, bool* err);
// Eat whitespace (space and tab, not end of lines).
void eatWhitespace(std::vector<uchar>& v, std::vector<uchar>::iterator& i, bool* eof);
// First eat whitespace, then start eating until c is found or eof. The found string is trimmed from beginning and ending whitespace.
std::string eatToSkipWhitespace(std::vector<char>& v, std::vector<char>::iterator& i, int c, size_t max, bool* eof, bool* err);
// Remove leading and trailing white space
void trimWhitespace(std::string* s);

std::string strTimestampUTC(double v);
// Given alfa=beta it returns "alfa":"beta"
std::string makeQuotedJson(const std::string& s);
void debugPayload(const std::string& intro, std::vector<uchar>& payload);
void debugPayload(const std::string& intro, std::vector<uchar>& payload, std::vector<uchar>::iterator& pos);
const char* toString(ELLSecurityMode esm);
ELLSecurityMode fromIntToELLSecurityMode(int i);
std::string toStringFromELLSN(int sn);
const char* toString(TPLSecurityMode tsm);
const char* mbusCField(uchar c_field);
const char* mbusCiField(uchar ci_field);
const char* toString(AFLAuthenticationType aat);
int toLen(AFLAuthenticationType aat);
const char* timeNN(int nn);
const char* timePP(int nn);

LinkMode toLinkMode(const char* arg);
LinkMode isLinkModeOption(const char* arg);
const char* toString(LinkMode lm);
std::string vif_6F_ThirdExtensionType(uchar dif, uchar vif, uchar vife);
std::string vif_7F_ManufacturerExtensionType(uchar dif, uchar vif, uchar vife);
LinkModeSet parseLinkModes(std::string m);
bool isValidAlias(const std::string& alias);
bool isValidBps(const std::string& b);
uchar bcd2bin(uchar c);
std::string currentYear();
std::string currentDay();
std::string currentHour();
std::string currentMinute();
std::string currentSeconds();
std::string currentMicros();

#define CHECK(n) if (!hasBytes(n, pos, frame)) return expectedMore(__LINE__);

bool hasBytes(int n, std::vector<uchar>::iterator &pos, std::vector<uchar> &frame);

#ifndef __FILENAME__
  #define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define warning(fmt, ...) \
  warning_x(__FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#define error(fmt, ...) \
  error_x(__FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#define debug(fmt, ...) \
  verbose_x(__FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#define verbose(fmt, ...) \
  debug_x(__FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#define trace(fmt, ...) \
  trace_x(__FILENAME__, __LINE__, fmt, ##__VA_ARGS__)

#endif
