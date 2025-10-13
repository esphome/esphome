/*
 Copyright (C) 2017-2024 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"address.h"
#include"manufacturers.h"

#include<assert.h>
#include<algorithm>
#include<string.h>

using namespace std;

vector<string> splitSequenceOfAddressExpressionsAtCommas(const string& mes);
bool isValidMatchExpression(const std::string& s, bool *has_wildcard);
bool doesIdMatchExpression(const std::string& id, std::string match_rule);
bool doesAddressMatchExpressions(Address &address,
                                 std::vector<AddressExpression>& address_expressions,
                                 bool *used_wildcard,
                                 bool *filtered_out,
                                 bool *required_found,
                                 bool *required_failed);

bool isValidMatchExpression(const string& s, bool *has_wildcard)
{
    string me = s;

    // Examples of valid match expressions:
    //  12345678
    //  *
    //  123*
    // !12345677
    //  2222222*
    // !22222222
    // We also accept an secondary libmbus address:
    // 100002842941011B

    // A match expression cannot be empty.
    if (me.length() == 0) return false;

    // An me can be filtered out with an exclamation mark first.
    if (me.front() == '!') me.erase(0, 1);

    // More than one negation is not allowed.
    if (me.front() == '!') return false;

    // A match expression cannot be only a negation mark.
    if (me.length() == 0) return false;

    int count = 0;
    // Some non-compliant meters have full hex in the id,
    // but according to the standard there should only be bcd here...
    // We accept hex anyway.
    while (me.length() > 0 &&
           ((me.front() >= '0' && me.front() <= '9') ||
            (me.front() >= 'A' && me.front() <= 'F') ||
            (me.front() >= 'a' && me.front() <= 'f')))
    {
        me.erase(0,1);
        count++;
    }

    if (me.length() == 0 && count == 16)
    {
        // A secondary libmbus address: 100002842941011B
        // Strictly speaking the leading 8 digits should be bcd,
        // but we accept hex as well.
        *has_wildcard = false;
        return true;
    }

    bool wildcard_used = false;
    // An expression can end with a *
    if (me.length() > 0 && me.front() == '*')
    {
        me.erase(0,1);
        wildcard_used = true;
        if (has_wildcard) *has_wildcard = true;
    }

    // Now we should have eaten the whole expression.
    if (me.length() > 0) return false;

    // Check the length of the matching bcd/hex
    // If no wildcard is used, then the match expression must be exactly 8 digits.
    if (!wildcard_used) return count == 8;

    // If wildcard is used, then the match expressions must be 7 or less digits,
    // even zero is allowed which means a single *, which matches any bcd/hex id.
    return count <= 7;
}

vector<string> splitSequenceOfAddressExpressionsAtCommas(const string& mes)
{
    vector<string> r;
    bool eof, err;
    vector<uchar> v (mes.begin(), mes.end());
    auto i = v.begin();

    for (;;) {
        auto id = eatTo(v, i, ',', 64, &eof, &err);
        if (err) break;
        trimWhitespace(&id);
        if (id == "ANYID") id = "*";
        r.push_back(id);
        if (eof) break;
    }
    return r;
}

bool isValidSequenceOfAddressExpressions(const string& mes)
{
    vector<string> v = splitSequenceOfAddressExpressionsAtCommas(mes);

    for (string me : v)
    {
        AddressExpression ae;
        if (!ae.parse(me)) return false;
    }
    return true;
}

vector<AddressExpression> splitAddressExpressions(const string &aes)
{
    vector<string> v = splitSequenceOfAddressExpressionsAtCommas(aes);

    vector<AddressExpression> r;

    for (string me : v)
    {
        AddressExpression ae;
        if (ae.parse(me))
        {
            r.push_back(ae);
        }
    }
    return r;
}

bool doesIdMatchExpression(const string& s, string match)
{
    string id = s;
    if (id.length() == 0) return false;

    // Here we assume that the match expression has been
    // verified to be valid.
    bool can_match = true;

    // Now match bcd/hex until end of id, or '*' in match.
    while (id.length() > 0 && match.length() > 0 && match.front() != '*')
    {
        if (id.front() != match.front())
        {
            // We hit a difference, it cannot match.
            can_match = false;
            break;
        }
        id.erase(0,1);
        match.erase(0,1);
    }

    bool wildcard_used = false;
    if (match.length() && match.front() == '*')
    {
        wildcard_used = true;
        match.erase(0,1);
    }

    if (can_match)
    {
        // Ok, now the match expression should be empty.
        // If wildcard is true, then the id can still have digits,
        // otherwise it must also be empty.
        if (wildcard_used)
        {
            can_match = match.length() == 0;
        }
        else
        {
            can_match = match.length() == 0 && id.length() == 0;
        }
    }

    return can_match;
}

bool hasWildCard(const string& mes)
{
    return mes.find('*') != string::npos;
}

bool AddressExpression::match(const std::string &i, uint16_t m, uchar v, uchar t)
{
    if (!(mfct == 0xffff || mfct == m)) return false;
    if (!(version == 0xff || version == v)) return false;
    if (!(type == 0xff || type == t)) return false;
    if (!doesIdMatchExpression(i, id)) return false;

    return true;
}

void AddressExpression::trimToIdentity(IdentityMode im, Address &a)
{
    switch (im)
    {
    case IdentityMode::FULL:
        id = a.id;
        mfct = a.mfct;
        version = a.version;
        type = a.type;
        required = true;
        break;
    case IdentityMode::ID_MFCT:
        id = a.id;
        mfct = a.mfct;
        version = 0xff;
        type = 0xff;
        required = true;
        break;
    case IdentityMode::ID:
        id = a.id;
        mfct = 0xffff;
        version = 0xff;
        type = 0xff;
        required = true;
        break;
    default:
        break;
    }
}

bool AddressExpression::parse(const string &in)
{
    string s = in;
    // Example: 12345678
    // or       12345678.M=PII.T=1B.V=01
    // or       1234*
    // or       1234*.M=PII
    // or       1234*.V=01
    // or       12 // mbus primary
    // or       0  // mbus primary
    // or       250.MPII.V01.T1B // mbus primary
    // or       !12345678
    // or       !*.M=ABC
    // or libmbus secondary style:
    //          123456782941011B
    id = "";
    mbus_primary = false;
    mfct = 0xffff;
    type = 0xff;
    version = 0xff;
    filter_out = false;

    if (s.size() == 0) return false;

    if (s.size() > 1 && s[0] == '!')
    {
        filter_out = true;
        s = s.substr(1);
        // Double ! not allowed.
        if (s.size() > 1 && s[0] == '!') return false;
    }
    vector<string> parts = splitString(s, '.');

    assert(parts.size() > 0);

    id = parts[0];
    if (!isValidMatchExpression(id, &has_wildcard))
    {
        // Not a long id, so lets check if it is p0 to p250 for primary mbus ids.
        if (id.size() < 2) return false;
        if (id[0] != 'p') return false;
        for (size_t i=1; i < id.length(); ++i)
        {
            if (!isdigit(id[i])) return false;
        }
        // All digits good.
        int v = atoi(id.c_str()+1);
        if (v < 0 || v > 250) return false;
        // It is 0-250 which means it is an mbus primary address.
        mbus_primary = true;
    }

    if (parts.size() == 1 && id.length() == 16)
    {
        // This is a secondary libmbus address.
        string mfct_hex = id.substr(8,4);
        string version_hex = id.substr(12,2);
        string type_hex = id.substr(14,2);
        id = id.substr(0,8);

        vector<uchar> data;
        bool ok = hex2bin(mfct_hex.c_str(), &data);
        if (!ok) return false;
        if (data.size() != 2) return false;
        mfct = data[1] << 8 | data[0];

        data.clear();
        ok = hex2bin(version_hex.c_str(), &data);
        if (!ok) return false;
        if (data.size() != 1) return false;
        version = data[0];

        data.clear();
        ok = hex2bin(type_hex.c_str(), &data);
        if (!ok) return false;
        if (data.size() != 1) return false;
        type = data[0];

        return true;
    }

    for (size_t i=1; i<parts.size(); ++i)
    {
        if (parts[i].size() == 4) // V=xy or T=xy
        {
            if (parts[i][1] != '=') return false;

            vector<uchar> data;
            bool ok = hex2bin(&parts[i][2], &data);
            if (!ok) return false;
            if (data.size() != 1) return false;

            if (parts[i][0] == 'V')
            {
                version = data[0];
            }
            else if (parts[i][0] == 'T')
            {
                type = data[0];
            }
            else
            {
                return false;
            }
        }
        else if (parts[i].size() == 5) // M=xyz
        {
            if (parts[i][1] != '=') return false;
            if (parts[i][0] != 'M') return false;

            bool ok = flagToManufacturer(&parts[i][2], &mfct);
            if (!ok) return false;
        }
        else if (parts[i].size() == 6) // M=abcd explicit hex version
        {
            if (parts[i][1] != '=') return false;
            if (parts[i][0] != 'M') return false;

            vector<uchar> data;
            bool ok = hex2bin(&parts[i][2], &data);
            if (!ok) return false;
            if (data.size() != 2) return false;

            mfct = data[1] << 8 | data[0];
            if (!ok) return false;
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool flagToManufacturer(const char *s, uint16_t *out_mfct)
{
    if (s[0] == 0 || s[1] == 0 || s[2] == 0 || s[3] != 0) return false;
    if (s[0] < '@' || s[0] > 'Z' ||
        s[1] < '@' || s[1] > 'Z' ||
        s[2] < '@' || s[2] > 'Z') return false;

    *out_mfct = MANFCODE(s[0],s[1],s[2]);
    return true;
}

string AddressExpression::str()
{
    string s;

    if (filter_out) s = "!";
    if (required) s = "R";

    s.append(id);
    if (mfct != 0xffff)
    {
        s += ".M="+manufacturerFlag(mfct);
    }
    if (version != 0xff)
    {
        s += ".V="+tostrprintf("%02x", version);
    }
    if (type != 0xff)
    {
        s += ".T="+tostrprintf("%02x", type);
    }

    return s;
}

string Address::str()
{
    string s;

    s.append(id);
    if (mfct != 0xffff)
    {
        s += ".M="+manufacturerFlag(mfct);
    }
    if (version != 0xff)
    {
        s += ".V="+tostrprintf("%02x", version);
    }
    if (type != 0xff)
    {
        s += ".T="+tostrprintf("%02x", type);
    }

    return s;
}

string Address::concat(std::vector<Address> &addresses)
{
    string s;
    for (Address& a: addresses)
    {
        if (s.size() > 0) s.append(",");
        s.append(a.str());
    }
    return s;
}

string AddressExpression::concat(std::vector<AddressExpression> &address_expressions)
{
    string s;
    for (AddressExpression& a: address_expressions)
    {
        if (s.size() > 0) s.append(",");
        s.append(a.str());
    }
    return s;
}

string manufacturerFlag(int m_field) {
    char a = (m_field/1024)%32+64;
    char b = (m_field/32)%32+64;
    char c = (m_field)%32+64;

    string flag;
    flag += a;
    flag += b;
    flag += c;
    return flag;
}

void Address::decodeMfctFirst(const vector<uchar>::iterator &pos)
{
    mfct = *(pos+1) << 8 | *(pos+0);
    id = tostrprintf("%02x%02x%02x%02x", *(pos+5), *(pos+4), *(pos+3), *(pos+2));
    version = *(pos+6);
    type = *(pos+7);
}

void Address::decodeIdFirst(const vector<uchar>::iterator &pos)
{
    id = tostrprintf("%02x%02x%02x%02x", *(pos+3), *(pos+2), *(pos+1), *(pos+0));
    mfct = *(pos+5) << 8 | *(pos+4);
    version = *(pos+6);
    type = *(pos+7);
}

bool doesTelegramMatchExpressions(std::vector<Address> &addresses,
                                  std::vector<AddressExpression>& address_expressions,
                                  bool *used_wildcard)
{
    bool match = false;
    bool filtered_out = false;
    bool required_found = false; // An R12345678 field was found.
    bool required_failed = true; // Init to fail, set to true if R is satistifed anywhere.

    for (Address &a : addresses)
    {
        if (doesAddressMatchExpressions(a,
                                        address_expressions,
                                        used_wildcard,
                                        &filtered_out,
                                        &required_found,
                                        &required_failed))
        {
            match = true;
        }
        // Go through all ids even though there is an early match.
        // This way we can see if theres an exact match later.
    }
    // If any expression triggered a filter out, then the whole telegram does not match.
    if (filtered_out) match = false;
    // If a required field was found and it failed....
    if (required_found && required_failed) match = false;
    return match;
}

bool doesAddressMatchExpressions(Address &address,
                                 vector<AddressExpression>& address_expressions,
                                 bool *used_wildcard,
                                 bool *filtered_out,
                                 bool *required_found,
                                 bool *required_failed)
{
    bool found_match = false;
    bool found_negative_match = false;
    bool exact_match = false;

    // Goes through all possible match expressions.
    // If no expression matches, neither positive nor negative,
    // then the result is false. (ie no match)

    // If more than one positive match is found, and no negative,
    // then the result is true.

    // If more than one negative match is found, irrespective
    // if there is any positive matches or not, then the result is false.

    // If a positive match is found, using a wildcard not any exact match,
    // then *used_wildcard is set to true.

    // If an expression is required and it fails, then the match fails.
    for (AddressExpression &ae : address_expressions)
    {
        bool has_wildcard = ae.has_wildcard;
        bool is_negative_rule = ae.filter_out;
        // We currently assume that only a single expression is required, the last one!
        bool is_required = ae.required;

        if (is_required) *required_found = true;

        bool m = ae.match(address.id, address.mfct, address.version, address.type);

        if (is_negative_rule)
        {
            if (m) found_negative_match = true;
        }
        else
        {
            if (m)
            {
                // A match, but the required does not count.
                if (!is_required)
                {
                    found_match = true;
                    if (!has_wildcard)
                    {
                        exact_match = true;
                    }
                }
                else
                {
                    *required_failed = false;
                }
            }
        }
    }
    if (found_negative_match)
    {
        *filtered_out = true;
        return false;
    }
    if (found_match)
    {
        if (exact_match)
        {
            *used_wildcard = false;
        }
        else
        {
            *used_wildcard = true;
        }
        return true;
    }
    return false;
}

const char *toString(IdentityMode im)
{
    switch (im)
    {
    case IdentityMode::ID: return "id";
    case IdentityMode::ID_MFCT: return "id-mfct";
    case IdentityMode::FULL: return "full";
    case IdentityMode::NONE: return "none";
    case IdentityMode::INVALID: return "invalid";
    }
    return "?";
}

IdentityMode toIdentityMode(const char *s)
{
    if (!strcmp(s,"id")) return IdentityMode::ID;
    if (!strcmp(s,"id-mfct")) return IdentityMode::ID_MFCT;
    if (!strcmp(s, "full")) return IdentityMode::FULL;
    if (!strcmp(s, "none")) return IdentityMode::NONE;
    return IdentityMode::INVALID;
}

void AddressExpression::clear()
{
    id = "";
    has_wildcard = false;
    mbus_primary = false;
    mfct = 0xffff;
    version = 0xff;
    type = 0xff;
}

void AddressExpression::appendIdentity(IdentityMode im,
                                       AddressExpression *identity_expression,
                                       std::vector<Address> &as,
                                       std::vector<AddressExpression> &es)
{
    identity_expression->clear();
    if (im == IdentityMode::NONE) return;

    // Copy id, id-mfct, id-mfct-v-t to identity_expression from the last address.
    identity_expression->trimToIdentity(im, as.back());

    // Is this identity expression already in the list of address expressions?
    if (std::find(es.begin(), es.end(), *identity_expression) == es.end())
    {
        // No, then add it at the end.
        es.push_back(*identity_expression);
    }
}

bool AddressExpression::operator==(const AddressExpression&ae) const
{
    return id == ae.id &&
        has_wildcard == ae.has_wildcard&&
        mbus_primary == ae.mbus_primary  &&
        mfct == ae.mfct &&
        version == ae.version &&
        type == ae.type &&
        filter_out == ae.filter_out;
}
