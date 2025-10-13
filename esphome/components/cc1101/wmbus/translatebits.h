#pragma once
/*
 Copyright (C) 2021-2022 Fredrik �hrstr�m (gpl-3.0-or-later)

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

#ifndef TRANSLATEBITS_H
#define TRANSLATEBITS_H

#include<cstdint>
#include<string>
#include<vector>

#include"utils.h"

struct TriggerBits
{
    TriggerBits() : bits_(0) {}
    TriggerBits(uint64_t b) : bits_(b) {}
    int intValue() const { return bits_; }
    bool operator==(const TriggerBits& tb) const { return bits_ == tb.bits_; }
    bool operator!=(const TriggerBits& tb) const { return bits_ != tb.bits_; }

private:
    uint64_t bits_;
};

extern TriggerBits AlwaysTrigger;

struct MaskBits
{
    MaskBits() : bits_(0) {}
    MaskBits(uint64_t b) : bits_(b) {}
    int intValue() { return bits_; }
    bool operator==(const MaskBits& tb) const { return bits_ == tb.bits_; }
    bool operator!=(const MaskBits& tb) const { return bits_ != tb.bits_; }

private:
    uint64_t bits_;
};

extern MaskBits AutoMask;

struct DefaultMessage
{
    DefaultMessage() : message_("") {}
    DefaultMessage(std::string m) : message_(m) {}
    const std::string& stringValue() { return message_; }
    bool operator==(const DefaultMessage& dm) const { return message_ == dm.message_; }
    bool operator!=(const DefaultMessage& dm) const { return message_ != dm.message_; }

private:
    std::string message_;
};

namespace Translate
{

    enum class MapType
    {
        Unknown,
        BitToString, // A bit translates to a text string.
        IndexToString, // A masked set of bits (a number) translates to a lookup index with text strings.
        DecimalsToString // Numbers are successively subtracted from input, each successfull subtraction translate into a text string.
    };

    struct Map
    {
        uint64_t from;
        std::string to;
        TestBit test;

        Map(uint64_t f, std::string t, TestBit b) : from(f), to(t), test(b) {};
        Map(uint64_t f, std::string t) : from(f), to(t), test(TestBit::Set) {};
    };

    struct Rule
    {
        std::string name;
        MapType type;
        TriggerBits trigger; // Bits that must be set.
        MaskBits mask; // Bits to be used are set as 1.
        DefaultMessage default_message; // If no bits are set print this, typically "OK" or "".
        std::vector<Map> map;

        Rule() {};
        Rule(std::string n, MapType t, TriggerBits tr, MaskBits mb, std::string dm, std::vector<Map> m)
            : name(n), type(t), trigger(tr), mask(mb), default_message(dm), map(m) {}
        Rule(std::string n, MapType t) :
            name(n), type(t), trigger(AlwaysTrigger), mask(AutoMask), default_message(DefaultMessage("")) {}
        Rule& set(TriggerBits t) { trigger = t; return *this; }
        Rule& set(MaskBits m) { mask = m; return *this; }
        Rule& set(DefaultMessage m) { default_message = m; return *this; }
        Rule& add(Map m) { map.push_back(m); return *this; }
    };

    struct Lookup
    {
        std::vector<Rule> rules;

        std::string translate(uint64_t bits);
        bool hasLookups() { return rules.size() > 0; }

        Lookup& add(Rule r) { rules.push_back(r); return *this; }

        std::string str();
    };
};

Translate::MapType toMapType(const char *s);

extern Translate::Lookup NoLookup;

#endif


