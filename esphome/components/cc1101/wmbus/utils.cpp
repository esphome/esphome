/***********************************************************************************
    Filename: utils.cpp
***********************************************************************************/

#include "utils.h"
#include "aes.h"
#include <vector>
#include <cassert>
#include <cstdarg>
#include "Telegram.h"
#include <algorithm>
#include <string.h>
#include <cmath>


//----------------------------------------------------------------------------
// Functions
//----------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------
//  void dumpHex(uint8_t* data, uint8_t len, bool newLine) 
//
//  DESCRIPTION:
//      Print data buffer in HEX 
//
//  ARGUMENTS: 
//      uint8_t* data  - Data to perform the CRC-16 operation on.
//      uint8_t len    - Length to print
//      bool newLine   - Should add new line at the end
//-------------------------------------------------------------------------------------------------------
void dumpHex(uint8_t* data, int len, bool newLine) {
    char buffHex[3];
    for (int i = 0; i < len; i++) {
        sprintf(buffHex, "%02X", data[i]);

    }
    if (newLine) {
      
    }
    else {
        
    }
}

void dumpInt(uint8_t* data, int len, bool newLine)
{
    for (int i = 0; i < len; i++)
    {
      
    }
    
}

std::string str_snprintf(const char* fmt, size_t len, ...)
{
    std::string str;
    va_list args;

    str.resize(len);
    va_start(args, len);
    size_t out_length = vsnprintf(&str[0], len + 1, fmt, args);
    va_end(args);

    if (out_length < len)
        str.resize(out_length);

    return str;
}
//std::string to_string(int value) { return str_snprintf("%d", 32, value); }
char format_hex_pretty_char(uint8_t v) { return v >= 10 ? 'A' + (v - 10) : '0' + v; }
std::string format_hex_pretty(const uint16_t* data, size_t length)
{
    if (length == 0)
        return "";
    std::string ret;
    ret.resize(5 * length - 1);
    for (size_t i = 0; i < length; i++)
    {
        ret[5 * i] = format_hex_pretty_char((data[i] & 0xF000) >> 12);
        ret[5 * i + 1] = format_hex_pretty_char((data[i] & 0x0F00) >> 8);
        ret[5 * i + 2] = format_hex_pretty_char((data[i] & 0x00F0) >> 4);
        ret[5 * i + 3] = format_hex_pretty_char(data[i] & 0x000F);
        if (i != length - 1)
            ret[5 * i + 2] = '.';
    }
    if (length > 4)
        return ret + " (" + std::to_string(length) + ")";
    return ret;
}

std::string format_hex_pretty(const uint8_t* data, size_t length)
{
    if (length == 0)
        return "";
    std::string ret;
    ret.resize(3 * length - 1);
    for (size_t i = 0; i < length; i++)
    {
        ret[3 * i] = format_hex_pretty_char((data[i] & 0xF0) >> 4);
        ret[3 * i + 1] = format_hex_pretty_char(data[i] & 0x0F);
        if (i != length - 1)
            ret[3 * i + 2] = '.';
    }
    if (length > 4)
        return ret + " (" + std::to_string(length) + ")";
    return ret;
}

std::string format_hex_pretty(std::vector<unsigned char> data)
{
    return format_hex_pretty(data.data(), data.size());
}

void phex(uint8_t* str, int len, int start)
{

    unsigned char i;
    for (i = start; i < len; ++i)
    {
        printf("%.2x,", str[i]);
        if (i != 0 && i % 16 == 0)
            printf("\n");
    }
    printf("\n");
}

void printHexString(uint8_t* str, int len, int start)
{

    unsigned char i;
    for (i = start; i < len; ++i)
    {
        printf("%.2X", str[i]);
    }
    printf("\n");
}

unsigned char* safeButUnsafeVectorPtr(std::vector<unsigned char>& v) {
    if (v.size() == 0) return NULL;
    return &v[0];
}


std::string tostrprintf(const char* fmt, ...)
{
    std::string s;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(buf, 4096, fmt, args);
    assert(n < 4096);
    va_end(args);
    s = buf;
    return s;
}

std::string tostrprintf(const std::string fmt, ...)
{
    std::string s;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(buf, 4096, fmt.c_str(), args);
    assert(n < 4096);
    va_end(args);
    s = buf;
    return s;
}

void strprintf(std::string* s, const char* fmt, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(buf, 4096, fmt, args);
    assert(n < 4096);
    va_end(args);
    *s = buf;
}

std::string bin2hex(std::vector<uchar>& data, int offset, int len) {
    std::string str;
    std::vector<uchar>::iterator i = data.begin();
    i += offset;
    while (i != data.end() && len-- > 0) {
        const char ch = *i;
        i++;
        str.append(&hex[(ch & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
    }
    return str;
}

std::string bin2hex(std::vector<uchar>::iterator data, std::vector<uchar>::iterator end, int len) {
    std::string str;
    while (data != end && len-- > 0) {
        const char ch = *data;
        data++;
        str.append(&hex[(ch & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
    }
    return str;
}

std::string bin2hex(const std::vector<uchar>& target) {
    std::string str;
    for (size_t i = 0; i < target.size(); ++i) {
        const char ch = target[i];
        str.append(&hex[(ch & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
    }
    return str;
}

std::string safeString(std::vector<uchar>& target) {
    std::string str;
    for (size_t i = 0; i < target.size(); ++i) {
        const char ch = target[i];
        if (ch >= 32 && ch < 127 && ch != '<' && ch != '>') {
            str += ch;
        }
        else {
            str += '<';
            str.append(&hex[(ch & 0xF0) >> 4], 1);
            str.append(&hex[ch & 0xF], 1);
            str += '>';
        }
    }
    return str;
}


void warning_x(const char* file_name, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    esphome::esp_log_vprintf_(ESPHOME_LOG_LEVEL_WARN, file_name, line, fmt, args);
    va_end(args);
}

void error_x(const char* file_name, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    esphome::esp_log_vprintf_(ESPHOME_LOG_LEVEL_ERROR, file_name, line, fmt, args);
    va_end(args);
}

void debug_x(const char* file_name, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    esphome::esp_log_vprintf_(ESPHOME_LOG_LEVEL_DEBUG, file_name, line, fmt, args);
    va_end(args);
}

void verbose_x(const char* file_name, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    esphome::esp_log_vprintf_(ESPHOME_LOG_LEVEL_VERBOSE, file_name, line, fmt, args);
    va_end(args);
}

void trace_x(const char* file_name, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    esphome::esp_log_vprintf_(ESPHOME_LOG_LEVEL_DEBUG, file_name, line, fmt, args);
    va_end(args);
}


std::vector<std::string> splitString(const std::string& s, char c)
{
    auto end = s.cend();
    auto start = end;

    std::vector<std::string> v;
    for (auto i = s.cbegin(); i != end; ++i)
    {
        if (*i != c)
        {
            if (start == end)
            {
                start = i;
            }
            continue;
        }
        if (start != end)
        {
            v.emplace_back(start, i);
            start = end;
        }
    }
    if (start != end)
    {
        v.emplace_back(start, end);
    }
    return v;
}

bool parseExtras(const std::string& s, std::map<std::string, std::string>* extras)
{
    std::vector<std::string> parts = splitString(s, ' ');

    for (auto& p : parts)
    {
        std::vector<std::string> kv = splitString(p, '=');
        if (kv.size() != 2) return false;
        (*extras)[kv[0]] = kv[1];
    }
    return true;
}

int char2int(char input)
{
    if (input >= '0' && input <= '9')
        return input - '0';
    if (input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    return -1;
}

bool isHexChar(uchar c)
{
    return char2int(c) != -1;
}

bool hex2bin(const char* src, std::vector<uchar>* target)
{
    if (!src) return false;
    while (*src && src[1]) {
        if (*src == ' ' || *src == '#' || *src == '|' || *src == '_') {
            // Ignore space and hashes and pipes and underlines.
            src++;
        }
        else {
            int hi = char2int(*src);
            int lo = char2int(src[1]);
            if (hi < 0 || lo < 0) return false;
            target->push_back(hi * 16 + lo);
            src += 2;
        }
    }
    return true;
}

bool hex2bin(const std::string& src, std::vector<uchar>* target)
{
    return hex2bin(src.c_str(), target);
}

bool hex2bin(std::vector<uchar>& src, std::vector<uchar>* target)
{
    if (src.size() % 2 == 1) return false;
    for (size_t i = 0; i < src.size(); i += 2) {
        if (src[i] != ' ') {
            int hi = char2int(src[i]);
            int lo = char2int(src[i + 1]);
            if (hi < 0 || lo < 0) return false;
            target->push_back(hi * 16 + lo);
        }
    }
    return true;
}

std::string format3fdot3f(double v)
{
    std::string r;
    strprintf(&r, "%3.3f", v);
    return r;
}

#define CRC16_EN_13757 0x3D65

uint16_t crc16_EN13757_per_byte(uint16_t crc, uchar b)
{
    unsigned char i;

    for (i = 0; i < 8; i++) {

        if (((crc & 0x8000) >> 8) ^ (b & 0x80)) {
            crc = (crc << 1) ^ CRC16_EN_13757;
        }
        else {
            crc = (crc << 1);
        }

        b <<= 1;
    }

    return crc;
}

uint16_t crc16_EN13757(uchar* data, size_t len)
{
    uint16_t crc = 0x0000;

    assert(len == 0 || data != NULL);

    for (size_t i = 0; i < len; ++i)
    {
        crc = crc16_EN13757_per_byte(crc, data[i]);
    }

    return (~crc);
}

bool isTraceEnabled() { return true; }

bool isDebugEnabled() { return true; }

bool isVerboseEnabled() { return true; }

std::string toIdsCommaSeparated(std::vector<std::string>& ids)
{
    std::string cs;
    for (std::string& s : ids)
    {
        cs += s;
        cs += ",";
    }
    if (cs.length() > 0) cs.pop_back();
    return cs;
}


std::string joinStatusOKStrings(const std::string& aa, const std::string& bb)
{
    std::string a = aa;
    while (a.length() > 0 && a.back() == ' ') a.pop_back();
    std::string b = bb;
    while (b.length() > 0 && b.back() == ' ') b.pop_back();

    if (a == "" || a == "OK" || a == "null")
    {
        if (b == "" || b == "null") return "OK";
        return b;
    }
    if (b == "" || b == "OK" || b == "null")
    {
        if (a == "" || a == "null") return "OK";
        return a;
    }

    return a + " " + b;
}

std::string joinStatusEmptyStrings(const std::string& aa, const std::string& bb)
{
    std::string a = aa;
    while (a.length() > 0 && a.back() == ' ') a.pop_back();
    std::string b = bb;
    while (b.length() > 0 && b.back() == ' ') b.pop_back();

    if (a == "" || a == "null")
    {
        if (b == "" || b == "null") return "";
        return b;
    }
    if (b == "" || b == "null")
    {
        if (a == "" || a == "null") return "";
        return a;
    }

    if (a != "OK" && b == "OK") return a;
    if (a == "OK" && b != "OK") return b;
    if (a == "OK" && b == "OK") return a;

    return a + " " + b;
}


std::string sortStatusString(const std::string& a)
{
    std::set<std::string> flags;
    std::string curr;

    for (size_t i = 0; i < a.length(); ++i)
    {
        uchar c = a[i];
        if (c == ' ')
        {
            if (curr.length() > 0)
            {
                flags.insert(curr);
                curr = "";
            }
        }
        else
        {
            curr += c;
        }
    }

    if (curr.length() > 0)
    {
        flags.insert(curr);
        curr = "";
    }

    std::string result;
    for (auto& s : flags)
    {
        result += s + " ";
    }

    while (result.size() > 0 && result.back() == ' ') result.pop_back();

    // This feature is only used for the em24 deprecated backwards compatible error field.
    // This should go away in the future.
    replace(result.begin(), result.end(), '~', ' ');

    return result;
}

std::set<std::string> splitStringIntoSet(const std::string& s, char c)
{
    std::vector<std::string> v = splitString(s, c);
    std::set<std::string> words(v.begin(), v.end());
    return words;
}


std::string reverseBCD(const std::string& v)
{
    int vlen = v.length();
    if (vlen % 2 != 0)
    {
        return "BADHEX:" + v;
    }

    std::string n = "";
    for (int i = 0; i < vlen; i += 2)
    {
        n += v[vlen - 2 - i];
        n += v[vlen - 1 - i];
    }
    return n;
}

std::string reverseBinaryAsciiSafeToString(const std::string& v)
{
    std::vector<uchar> bytes;
    bool ok = hex2bin(v, &bytes);
    if (!ok) return "BADHEX:" + v;
    reverse(bytes.begin(), bytes.end());
    return safeString(bytes);
}

// Check if hex string is likely to be ascii
bool isLikelyAscii(const std::string& v)
{
    std::vector<uchar> val;
    bool ok = hex2bin(v, &val);

    // For example 64 bits:
    // 0000 0000 4142 4344
    // is probably the string DCBA

    if (!ok) return false;

    size_t i = 0;
    for (; i < val.size(); ++i)
    {
        if (val[i] != 0) break;
    }

    if (i == val.size())
    {
        // Value is all zeroes, this is probably a number.
        return false;
    }

    for (; i < val.size(); ++i)
    {
        if (val[i] < 20 || val[i] > 126) return false;
    }

    return true;
}

bool is_leap_year(int year)
{
    year += 1900;
    if (year % 4 != 0) return false;
    if (year % 400 == 0) return true;
    if (year % 100 == 0) return false;
    return true;
}

int days_in_months[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

int get_days_in_month(int year, int month)
{
    if (month < 0 || month >= 12)
    {
        month = 0;
    }

    int days = days_in_months[month];

    if (month == 1 && is_leap_year(year))
    {
        // Handle february in a leap year.
        days += 1;
    }

    return days;
}


double addMonths(double t, int months)
{
    time_t ut = (time_t)t;
    struct tm time;
    localtime_r( &ut,&time);
    addMonths(&time, months);
    return (double)mktime(&time);
}

void addMonths(struct tm* date, int months)
{
    bool is_last_day_in_month = date->tm_mday == get_days_in_month(date->tm_year, date->tm_mon);

    int year = date->tm_year + months / 12;
    int month = date->tm_mon + months % 12;

    while (month > 11)
    {
        year += 1;
        month -= 12;
    }

    while (month < 0)
    {
        year -= 1;
        month += 12;
    }

    int day;

    if (is_last_day_in_month)
    {
        day = get_days_in_month(year, month); // Last day of month maps to last day of result month
    }
    else
    {
        day = std::min(date->tm_mday, get_days_in_month(year, month));
    }

    date->tm_year = year;
    date->tm_mon = month;
    date->tm_mday = day;
}

bool startsWith(const std::string& s, std::string& prefix)
{
    return startsWith(s, prefix.c_str());
}

bool startsWith(const std::string& s, const char* prefix)
{
    size_t len = strlen(prefix);
    if (s.length() < len) return false;
    if (s.length() == len) return s == prefix;
    return !strncmp(&s[0], prefix, len);
}


std::string strdate(struct tm* date)
{
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%d", date);
    return std::string(buf);
}

std::string strdate(double v)
{
    if (std::isnan(v)) return "null";
    struct tm date;
    time_t t = v;
    localtime_r( &t,&date);
    return strdate(&date);
}

std::string strdatetime(struct tm* datetime)
{
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", datetime);
    return std::string(buf);
}

std::string strdatetime(double v)
{
    if (std::isnan(v)) return "null";
    struct tm datetime;
    time_t t = v;
    localtime_r( &t,&datetime);
    return strdatetime(&datetime);
}

std::string strdatetimesec(struct tm* datetime)
{
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", datetime);
    return std::string(buf);
}

std::string strdatetimesec(double v)
{
    struct tm datetime;
    time_t t = v;
    localtime_r( &t,&datetime);
    return strdatetimesec(&datetime);
}

void eatWhitespace(std::vector<uchar>& v, std::vector<uchar>::iterator& i, bool* eof)
{
    *eof = false;
    while (i != v.end() && (*i == ' ' || *i == '\t'))
    {
        i++;
    }
    if (i == v.end()) {
        *eof = true;
    }
}

std::string eatToSkipWhitespace(std::vector<uchar>& v, std::vector<uchar>::iterator& i, int c, size_t max, bool* eof, bool* err)
{
    eatWhitespace(v, i, eof);
    if (*eof) {
        if (c != -1) {
            *err = true;
        }
        return "";
    }
    std::string s = eatTo(v, i, c, max, eof, err);
    trimWhitespace(&s);
    return s;
}

std::string eatTo(std::vector<uchar>& v, std::vector<uchar>::iterator& i, int c, size_t max, bool* eof, bool* err)
{
    std::string s;

    *eof = false;
    *err = false;
    while (max > 0 && i != v.end() && (c == -1 || *i != c))
    {
        s += *i;
        i++;
        max--;
    }
    if (c != -1 && (i == v.end() || *i != c))
    {
        *err = true;
    }
    if (i != v.end())
    {
        i++;
    }
    if (i == v.end()) {
        *eof = true;
    }
    return s;
}



void trimWhitespace(std::string* s)
{
    const char* ws = " \t";
    s->erase(0, s->find_first_not_of(ws));
    s->erase(s->find_last_not_of(ws) + 1);
}

std::string strTimestampUTC(double v)
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    time_t d = v;
    struct tm ts;
    gmtime_r( &d,&ts);
    strftime(datetime, sizeof(datetime), "%FT%TZ", &ts);
    return std::string(datetime);
}


std::string makeQuotedJson(const std::string& s)
{
    size_t p = s.find('=');
    std::string key, value;
    if (p != std::string::npos)
    {
        key = s.substr(0, p);
        value = s.substr(p + 1);
    }
    else
    {
        key = s;
        value = "";
    }

    return std::string("\"") + key + "\":\"" + value + "\"";
}


void debugPayload(const std::string& intro, std::vector<uchar>& payload)
{
    if (isDebugEnabled())
    {
        std::string msg = bin2hex(payload);
        debug("%s \"%s\"", intro.c_str(), msg.c_str());
    }
}

void debugPayload(const std::string& intro, std::vector<uchar>& payload, std::vector<uchar>::iterator& pos)
{
    if (isDebugEnabled())
    {
        std::string msg = bin2hex(pos, payload.end(), 1024);
        debug("%s \"%s\"", intro.c_str(), msg.c_str());
    }
}

const char* toString(ELLSecurityMode esm)
{
    switch (esm) {

#define X(name,nr) case ELLSecurityMode::name : return #name;
        LIST_OF_ELL_SECURITY_MODES
#undef X
    }

    return "?";
}

ELLSecurityMode fromIntToELLSecurityMode(int i)
{
    switch (i) {

#define X(name,nr) case nr: return ELLSecurityMode::name;
        LIST_OF_ELL_SECURITY_MODES
#undef X
    }

    return ELLSecurityMode::RESERVED;
}

std::string toStringFromELLSN(int sn)
{
    int session = (sn >> 0) & 0x0f; // lowest 4 bits
    int time = (sn >> 4) & 0x1ffffff; // next 25 bits
    int sec = (sn >> 29) & 0x7; // next 3 bits.
    std::string info;
    ELLSecurityMode esm = fromIntToELLSecurityMode(sec);
    info += toString(esm);
    info += " session=";
    info += std::to_string(session);
    info += " time=";
    info += std::to_string(time);
    return info;
}

const char* toString(TPLSecurityMode tsm)
{
    switch (tsm) {

#define X(name,nr) case TPLSecurityMode::name : return #name;
        LIST_OF_TPL_SECURITY_MODES
#undef X
    }

    return "Reserved";
}


const char* mbusCField(uchar c_field)
{
    std::string s;
    switch (c_field)
    {
    case 0x08: return "RSP_UD2";
    }
    return "?";
}

const char* mbusCiField(uchar c_field)
{
    std::string s;
    switch (c_field)
    {
    case 0x78: return "no header";
    case 0x7a: return "short header";
    case 0x72: return "long header";
    case 0x79: return "no header compact frame";
    case 0x7b: return "short header compact frame";
    case 0x73: return "long header compact frame";
    case 0x69: return "no header format frame";
    case 0x6a: return "short header format frame";
    case 0x6b: return "long header format frame";
    }
    return "?";
}


const char* toString(AFLAuthenticationType tsm)
{
    switch (tsm) {

#define X(name,nr,len) case AFLAuthenticationType::name : return #name;
        LIST_OF_AFL_AUTH_TYPES
#undef X
    }

    return "Reserved";
}

int toLen(AFLAuthenticationType aat)
{
    switch (aat) {

#define X(name,nr,len) case AFLAuthenticationType::name : return len;
        LIST_OF_AFL_AUTH_TYPES
#undef X
    }

    return 0;
}

const char* timeNN(int nn) {
    switch (nn) {
    case 0: return "second(s)";
    case 1: return "minute(s)";
    case 2: return "hour(s)";
    case 3: return "day(s)";
    }
    return "?";
}

const char* timePP(int nn) {
    switch (nn) {
    case 0: return "hour(s)";
    case 1: return "day(s)";
    case 2: return "month(s)";
    case 3: return "year(s)";
    }
    return "?";
}

const char* toString(LinkMode lm)
{
#define X(name,lcname,option,val) if (lm == LinkMode::name) return #lcname;
    LIST_OF_LINK_MODES
#undef X

        return "unknown";
}

std::string vif_7F_ManufacturerExtensionType(uchar dif, uchar vif, uchar vife)
{
    assert(vif == 0xff);
    return "?";
}

std::string vif_6F_ThirdExtensionType(uchar dif, uchar vif, uchar vife)
{
    assert(vif == 0xef);
    return "?";
}

LinkModeSet parseLinkModes(std::string m)
{
    LinkModeSet lms;
    char buf[100];
    strcpy(buf, m.c_str());
    char* saveptr{};
    const char* tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL)
    {
        LinkMode lm = toLinkMode(tok);
        if (lm == LinkMode::UNKNOWN)
        {
            error("(wmbus) not a valid link mode: %s", tok);
        }
        lms.addLinkMode(lm);
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return lms;
}


bool is_ascii_alnum(char c)
{
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == '_') return true;
    return false;
}

bool is_ascii(char c)
{
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    return false;
}

bool isValidAlias(const std::string& alias)
{
    if (alias.length() == 0) return false;

    if (!is_ascii(alias[0])) return false;

    for (char c : alias)
    {
        if (!is_ascii_alnum(c)) return false;
    }

    return true;
}

// The byte 0x13 i converted into the integer value 13.
uchar bcd2bin(uchar c)
{
    return (c & 15) + (c >> 4) * 10;
}

std::string currentYear()
{
    return std::string("2000");
}

std::string currentDay()
{
    return std::string("2000-01-01");
}

std::string currentHour()
{
    return std::string("2000-01-01_00");
}

std::string currentMinute()
{
    return std::string("2000-01-01_00:00");
}

std::string currentSeconds()
{
    return std::string("2000-01-01_00:00:00");
}

std::string currentMicros()
{
    return std::string("2000-01-01_00:00");
}

bool hasBytes(int n, std::vector<uchar>::iterator &pos, std::vector<uchar> &frame)
{
    int remaining = distance(pos, frame.end());
    if (remaining < n) return false;
    return true;
}