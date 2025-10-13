#pragma once
/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef FORMULA_H
#define FORMULA_H

#include<memory>
#include<vector>

#include"units.h"

struct Meter;
struct FieldInfo;
struct DVEntry;

struct Formula
{
    // Parse and return false if parse failed.
    virtual bool parse(Meter* m, const std::string& f) = 0;
    // Returns false if the parse has failed and the formula is invalid.
    virtual bool valid() = 0;
    // Returns lines of error messages.
    virtual std::string errors() = 0;
    // Calculate the formula. Returns nan if it could not be calculated.
    // If a dventry is supplied, then the values storage_counter, tariff_counter, subunit_counter are available.
    // If a meter is supplied it overrides the meter supplied when parsing.
    virtual double calculate(Unit to, DVEntry* dve = NULL, Meter* m = NULL) = 0;
    // Clear the formula, ie drop any parsed tree.
    virtual void clear() = 0;
    // Return a regenerated formula string.
    virtual std::string str() = 0;
    // Return the formula in a format where the tree structure is explicit.
    virtual std::string tree() = 0;
    // Specify which meter to read the meter fields from.
    virtual void setMeter(Meter* m) = 0;
    // Specify which dventry to read counter fields from.
    virtual void setDVEntry(DVEntry* dve) = 0;

    virtual ~Formula() = 0;
};

Formula* newFormula();

struct StringInterpolator
{
    /**
       parse: Parse a field name like: "historic_{storage_counter / - 12 counter}_value"
       @f: String to parse and find {...} subformulas inside.

       Parse and prepare it for being applied to an actual dve.
       E.g. for a dventry with storage 13 the formula above will generate "historic_1_value"
    */
    virtual bool parse(const std::string& f) = 0;
    /**
       apply: Insert inline formulas in field names like "total_{subunit_nr}" insert the result of the formula.
       @dve: A pointer to a telegram entry to use (ie containing storagenr/tariffnr/subunitnr etc.) Can be NULL.

    */
    virtual std::string apply(DVEntry* dve) = 0;

    virtual ~StringInterpolator() = 0;
};

StringInterpolator* newStringInterpolator();

#endif

