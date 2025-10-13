/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef ADDRESS_H_
#define ADDRESS_H_

#include "utils.h"
#include <string>

/**
    IdentityMode:

    @ID: The default, only the id groups the meter content.
    @ID_MFCT: Used when you have two meters with the same id but different manufacturers.
    @FULL: Used when you want to fully separate meter content on id.mft.v.t
    @NONE: Do not separate any meters! This might lead to telegrams overwriting each others state.
           Use this when no state is to be kept in the wmbusmeters object.
    @INVALID: Cannot parse cmdline.
*/
enum class IdentityMode
{
    ID,
    ID_MFCT,
    FULL,
    NONE,
    INVALID
};

const char *toString(IdentityMode im);
IdentityMode toIdentityMode(const char *s);

struct Address
{
    std::string id; // p1 or 12345678 or non-compliant hex: 1234abcd
    uint16_t mfct {};
    uchar type {};
    uchar version {};

    void decodeMfctFirst(const std::vector<uchar>::iterator &pos);
    void decodeIdFirst(const std::vector<uchar>::iterator &pos);

    std::string str();
    static std::string concat(std::vector<Address> &addresses);
};

struct AddressExpression
{
    // An address expression is used to select which telegrams to decode for a driver.
    // An address expression is also used to select a specific meter to poll for data.
    // Example address: 12345678
    // Or fully qualified: 12345678.M=PII.T=1b.V=01
    // which means manufacturer triplet PII, type/media=0x1b, version=0x01
    // Or wildcards in id: 12*.T=16
    // which matches all cold water meters whose ids start with 12.
    // Or negated tests: 12345678.V!=66
    // which will decode all telegrams from 12345678 except those where the version is 0x66.
    // Or every telegram which is does not start with 12 and is not from ABB:
    // !12*.M!=ABB

    std::string id; // p1 or 12345678 or non-compliant hex: 1234abcd
    bool has_wildcard {}; // The id contains a *
    bool mbus_primary {}; // Signals that the id is 0-250

    uint16_t mfct { 0xffff }; // If 0xffff then any mfct matches this address.
    uchar version { 0xff }; // If 0xff then any version matches this address.
    uchar type { 0xff }; // If 0xff then any type matches this address.

    bool filter_out {}; // Telegrams matching this rule should be filtered out!
    bool required {}; // If true, then this address expression must be matched!

    AddressExpression() {}
    AddressExpression(Address &a) : id(a.id), mfct(a.mfct), version(a.version), type(a.type) { }
    bool operator==(const AddressExpression&) const;
    void clear();
    void trimToIdentity(IdentityMode im, Address &a);
    bool parse(const std::string &s);
    bool match(const std::string &id, uint16_t mfct, uchar version, uchar type);
    std::string str();
    static std::string concat(std::vector<AddressExpression> &address_expressions);
    static void appendIdentity(IdentityMode im,
                               AddressExpression *identity_expression,
                               std::vector<Address> &as,
                               std::vector<AddressExpression> &es);
};

/**
    isValidSequenceOfAddressExpressions:

    Valid sequenes look like this:
    12345678
    12345678,22334455,34*
    12*.T=16,!*.M=XYZ
    !*.V=33
*/
bool isValidSequenceOfAddressExpressions(const std::string& s);
std::vector<AddressExpression> splitAddressExpressions(const std::string &aes);
bool flagToManufacturer(const char *s, uint16_t *out_mfct);
std::string manufacturerFlag(int m_field);
bool doesTelegramMatchExpressions(std::vector<Address> &addresses,
                                  std::vector<AddressExpression>& address_expressions,
                                  bool *used_wildcard);

#endif
