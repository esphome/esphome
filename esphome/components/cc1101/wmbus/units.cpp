
#include "units.h"
/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"utils.h"
#include<assert.h>
#include<math.h>
#include<string.h>
#include<limits>

using namespace std;
// #define M_PI 3.14

#define LIST_OF_CONVERSIONS \
    X(Second, Minute, {vto=vfrom/60.0;}) \
    X(Minute, Second, {vto=vfrom*60.0;}) \
    X(Second, Hour, {vto=vfrom/3600.0;}) \
    X(Hour, Second, {vto=vfrom*3600.0;}) \
    X(Year, Second, {vto=vfrom*3600.0*24.0*365.2425;}) \
    X(Second, Year, {vto=vfrom/3600.0/24.0/365.2425;}) \
    X(Minute, Hour, {vto=vfrom/60.0;}) \
    X(Hour, Minute, {vto=vfrom*60.0;}) \
    X(Minute, Year, {vto=vfrom/60.0/24.0/365.2425;}) \
    X(Year, Minute, {vto=vfrom*60.0*24.0*365.2425;}) \
    X(Hour, Year, {vto=vfrom/24.0/365.2425;}) \
    X(Year, Hour, {vto=vfrom*24.0*365.2425;}) \
    X(Hour,  Day, {vto=vfrom/24.0;}) \
    X(Day,  Hour, {vto=vfrom*24.0;}) \
    X(Day,  Year, {vto=vfrom/365.2425;}) \
    X(Year,  Day, {vto=vfrom*365.2425;}) \
    X(KWH, GJ, {vto=vfrom*0.0036;})     \
    X(KWH, MJ, {vto=vfrom*0.0036*1000.0;})     \
    X(GJ,  KWH,{vto=vfrom/0.0036;}) \
    X(MJ,  GJ,  {vto=vfrom/1000.0;}) \
    X(MJ,  KWH,{vto=vfrom/1000.0/0.0036;}) \
    X(GJ,  MJ,  {vto=vfrom*1000.0;}) \
    X(M3,  L,  {vto=vfrom*1000.0;}) \
    X(M3H, LH, {vto=vfrom*1000.0;}) \
    X(L,   M3, {vto=vfrom/1000.0;}) \
    X(LH,  M3H,{vto=vfrom/1000.0;}) \
    X(C,   K,  {vto=vfrom+273.15;}) \
    X(K,   C,  {vto=vfrom-273.15;}) \
    X(C,   F,  {vto=(vfrom*9.0/5.0)+32.0;}) \
    X(F,   C,  {vto=(vfrom-32)*5.0/9.0;}) \
    X(PA,  BAR,{vto=vfrom/100000.0;}) \
    X(BAR, PA, {vto=vfrom*100000.0;}) \
    X(COUNTER, FACTOR,{vto=vfrom;})  \
    X(FACTOR, COUNTER, {vto=vfrom;}) \
    X(COUNTER, NUMBER,{vto=vfrom;})  \
    X(NUMBER, COUNTER, {vto=vfrom;}) \
    X(FACTOR, NUMBER, {vto=vfrom;})  \
    X(NUMBER, FACTOR, {vto=vfrom;}) \
    X(UnixTimestamp,DateTimeLT, {vto=vfrom; }) \
    X(DateTimeLT,UnixTimestamp, {vto=vfrom; }) \
    X(DateLT,UnixTimestamp, {vto=vfrom; }) \
    X(DateTimeLT, DateLT, {vto=vfrom; }) \
    X(DateLT, DateTimeLT, {vto=vfrom; }) \
    X(DEGREE, RADIAN, {vto=vfrom*M_PI/180.0;}) \
    X(RADIAN, DEGREE, {vto=vfrom*180.0/M_PI;}) \


#define LIST_OF_SI_CONVERSIONS  \
    X(Second, 1.0, SIExp().s(1))                                   \
    X(M,      1.0, SIExp().m(1))                                   \
    X(KG,     1.0, SIExp().kg(1))                                  \
    X(Ampere, 1.0, SIExp().a(1))                                   \
    X(K,      1.0, SIExp().k(1))                                   \
    X(MOL,    1.0, SIExp().mol(1))                                 \
    X(CD,     1.0, SIExp().cd(1))                                  \
    \
    X(KWH,    3.6e+06,    SIExp().kg(1).m(2).s(-2))                 \
    X(MJ,     1.0e+06,    SIExp().kg(1).m(2).s(-2))                 \
    X(GJ,     1.0e+09,    SIExp().kg(1).m(2).s(-2))                 \
    X(KVARH,  3.6e+06,    SIExp().kg(1).m(2).s(-2))                 \
    X(KVAH,   3.6e+06,    SIExp().kg(1).m(2).s(-2))                 \
    X(M3C,    1.0,        SIExp().m(3).c(1))                        \
    \
    X(KW,     1000.0,     SIExp().kg(1).m(2).s(-3))                 \
    X(M3CH,   3600.0,    SIExp().m(3).c(1).s(-1))                  \
    \
    X(M3,     1.0,        SIExp().m(3))                             \
    X(L,      1.0/1000.0, SIExp().m(3))                             \
    X(M3H,    3600.0,     SIExp().m(3).s(-1))                       \
    X(LH,     3.600,      SIExp().m(3).s(-1))                       \
    \
    X(C,      1.0,        SIExp().c(1)) \
    X(F,      1.0,        SIExp().f(1)) \
    \
    X(Volt,   1.0,           SIExp().kg(1).m(2).s(-3).a(-1))         \
    X(Hertz,     1.0,           SIExp().s(-1))                       \
    X(PA,     1.0,           SIExp().kg(1).m(-1).s(-2))              \
    X(BAR,    100000.0,      SIExp().kg(1).m(-1).s(-2))              \
    \
    X(Minute,      60.0,          SIExp().s(1))                      \
    X(Hour,        3600.0,        SIExp().s(1))                      \
    X(Day,         3600.0*24,     SIExp().s(1))                      \
    X(Month,       1,             SIExp().month(1))                  \
    X(Year,        1,             SIExp().year(1))                   \
    X(UnixTimestamp,1.0,          SIExp().unixTimestamp(1))          \
    X(DateTimeUTC, 1.0,           SIExp().unixTimestamp(1))          \
    X(DateTimeLT,  1.0,           SIExp().unixTimestamp(1))          \
    X(DateLT,      1.0,           SIExp().unixTimestamp(1))          \
    X(TimeLT,      1.0,           SIExp().unixTimestamp(1))          \
                                                                     \
    X(RH,          1.0, SIExp())                                     \
    X(HCA,         1.0, SIExp())                                     \
    X(DEGREE,      1.0, SIExp())                                     \
    X(RADIAN,      180.0/M_PI, SIExp())                              \
    X(COUNTER,     1.0, SIExp())                                     \
    X(FACTOR,      1.0, SIExp())                                     \
    X(NUMBER,      1.0, SIExp())                                     \
    X(TXT,         1.0, SIExp())                                     \


// 3600.0*24*365.2425
// 3600.0*24*30.437

#define X(cname,lcname,hrname,quantity,explanation) const SIUnit SI_##cname(Unit::cname);
LIST_OF_UNITS
#undef X

const SIUnit SI_Unknown(Unit::Unknown);

const SIUnit& toSIUnit(Unit u)
{
    switch (u)
    {
#define X(cname,lcname,hrname,quantity,explanation) case Unit::cname: return SI_##cname;
        LIST_OF_UNITS
#undef X
    default: break;
    }

    return SI_Unknown;
}

bool overrideConversion(Unit from, Unit to)
{
    // The mbus protocol lacks kvarh and kva. Some meters, like the abbb23 uses kwh for kvarh.
    // Permit 1 to 1 conversion from kwh to kvarh and kva in extractNumeric.
    if (from == Unit::KWH && (to == Unit::KVARH || to == Unit::KVAH)) return true;
    return false;
}

bool canConvert(Unit ufrom, Unit uto)
{
    if (ufrom == uto) return true;
#define X(from,to,code) if (Unit::from == ufrom && Unit::to == uto) return true;
    LIST_OF_CONVERSIONS
#undef X
        return false;
}

double convert(double vfrom, Unit ufrom, Unit uto)
{
    double vto = -4711.0;
    if (ufrom == uto) { { vto = vfrom; } return vto; }

#define X(from,to,code) if (Unit::from == ufrom && Unit::to == uto) { code return vto; }
    LIST_OF_CONVERSIONS
#undef X

    string from = unitToStringHR(ufrom);
    string to = unitToStringHR(uto);

    warning("Cannot convert between units! from '%s' to '%s'", from.c_str(), to.c_str());
    return std::numeric_limits<double>::quiet_NaN();
}

bool isKCF(const SIExp& e)
{
    return
        e == SI_K.exp() ||
        e == SI_C.exp() ||
        e == SI_F.exp();
}

bool is_S_MONTH_YEAR_UT(const SIExp& e)
{
    return
        e == SI_Second.exp() ||
        e == SI_UnixTimestamp.exp() ||
        e == SI_Month.exp() ||
        e == SI_Year.exp();
}

void getScaleOffset(const SIExp& e, double* scale, double* offset)
{
    if (e == SI_K.exp())
    {
        *scale = 1.0;
        *offset = 0.0;
        return;
    }
    if (e == SI_C.exp())
    {
        *scale = 1.0;
        *offset = 273.15;
        return;
    }
    if (e == SI_F.exp())
    {
        *scale = 5.0 / 9.0;
        *offset = -32.0 + (273.15 * 9.0 / 5.0);
        return;
    }
    assert(0);
}

bool SIUnit::convertTo(double left, const SIUnit& out_siunit, double* out) const
{
    if (exp() == out_siunit.exp())
    {
        if (out != NULL) *out = (left * scale_) / out_siunit.scale_;
        return true;
    }

    // Now the special cases. K-C-F
    if (isKCF(exp()) && isKCF(out_siunit.exp()))
    {
        double from_scale{};
        double from_offset{};

        getScaleOffset(exp(), &from_scale, &from_offset);
        from_scale *= scale();

        double to_offset{};
        double to_scale{};

        getScaleOffset(out_siunit.exp(), &to_scale, &to_offset);
        to_scale *= out_siunit.scale();

        if (out != NULL) *out = ((left + from_offset) * from_scale) / to_scale - to_offset;
        return true;
    }

    if (out != NULL) *out = std::numeric_limits<double>::quiet_NaN();
    return false;
}

bool forbidden_op(MathOp op, const SIExp& a, const SIExp& b)
{
    // Two unix timestamps cannot be added together. They can be subtracted though!
    if (op == MathOp::ADD && a == SI_UnixTimestamp.exp() && b == SI_UnixTimestamp.exp()) return true;

    return false;
}

double do_op(MathOp op, double left, double right)
{
    if (op == MathOp::ADD) return left + right;
    if (op == MathOp::SUB) return left - right;
    assert(0);
}

bool SIUnit::mathOpTo(MathOp op, double left, double right, const SIUnit& right_siunit, SIUnit* out_siunit, double* out) const
{
    // Adding all values with the same units.
    if (exp() == right_siunit.exp())
    {
        if (forbidden_op(op, exp(), right_siunit.exp()))
        {
            if (out_siunit != NULL) *out_siunit = SI_COUNTER;
            if (out != NULL) *out = std::numeric_limits<double>::quiet_NaN();
            return false;
        }
        double left_converted{};
        convertTo(left, right_siunit, &left_converted);
        double result = do_op(op, left_converted, right);
        if (out_siunit != NULL) *out_siunit = right_siunit;
        if (out != NULL) *out = result;
        return true;
    }

    // Adding temperatures.
    if (isKCF(exp()) && isKCF(right_siunit.exp()))
    {
        double left_converted{};
        convertTo(left, right_siunit, &left_converted);
        double result = do_op(op, left_converted, right);
        if (out_siunit != NULL) *out_siunit = right_siunit;
        if (out != NULL) *out = result;
        return true;
    }

    // Operating on unix timestamps
    if (exp() == SI_UnixTimestamp.exp() || right_siunit.exp() == SI_UnixTimestamp.exp())
    {
        if (right_siunit.exp() == SI_UnixTimestamp.exp())
        {
            // The timestamp is right, flip the arguments.
            return right_siunit.mathOpTo(op, right, left, *this, out_siunit, out);
        }
        assert(exp() == SI_UnixTimestamp.exp() && right_siunit.exp() != SI_UnixTimestamp.exp());

        // The timestamp is left. Lets handle all permitted additions to UnixTimestamp.
        if (right_siunit.exp() == SI_Second.exp())
        {
            // Move right argument (day, hour, min, s) to seconds.
            double right_converted{};
            right_siunit.convertTo(right, SI_Second, &right_converted);
            // Add the seconds to the unix timestamp.
            double result = do_op(op, left, right_converted);
            if (out_siunit != NULL) *out_siunit = SI_UnixTimestamp;
            if (out != NULL) *out = result;
            return true;
        }
        if (right_siunit.exp() == SI_Month.exp())
        {
            // Move right argument (day, hour, min, s) to seconds.
            if (op == MathOp::SUB) right = -right;
            double result = addMonths(left, right);
            if (out_siunit != NULL) *out_siunit = SI_UnixTimestamp;
            if (out != NULL) *out = result;
            return true;
        }
    }

    // Oups, should not get here....
    return false;
}

SIUnit SIUnit::mul(const SIUnit& m) const
{
    // Multipliying the SIUnits adds the exponents.
    SIExp exps = exponents_.mul(m.exponents_);

    double new_scale = scale_ * m.scale_;
    SIUnit tmp(Quantity::Unknown,
        new_scale,
        exps);

    Unit u = tmp.asUnit(Quantity::Unknown);
    Quantity q = toQuantity(u);

    return SIUnit(q, new_scale, exps);
}

SIUnit SIUnit::div(const SIUnit& m) const
{
    // Dividing with a  SIUnit subtracts the exponents.
    SIExp exps = exponents_.div(m.exponents_);

    double new_scale = scale_ / m.scale_;

    SIUnit tmp(Quantity::Unknown,
        new_scale,
        exps);

    Unit u = tmp.asUnit(Quantity::Unknown);
    Quantity q = toQuantity(u);

    return SIUnit(q, new_scale, exps);
}

SIUnit SIUnit::sqrt() const
{
    // Square rooting SIUnit halfs the exponents.
    SIExp exps = exponents_.sqrt();

    double new_scale = ::sqrt(scale_);
    SIUnit tmp(Quantity::Unknown,
        new_scale,
        exps);

    Unit u = tmp.asUnit(Quantity::Unknown);
    Quantity q = toQuantity(u);

    return SIUnit(q, new_scale, exps);
}

SIUnit whenMultiplied(SIUnit left, SIUnit right)
{
    return Unit::Unknown;
}

double multiply(double l, SIUnit left, double r, SIUnit right)
{
    return 0;
}

bool isQuantity(Unit u, Quantity q)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return Quantity::quantity == q;
    LIST_OF_UNITS
#undef X

        return false;
}

Quantity toQuantity(Unit u)
{
    switch (u)
    {
#define X(cname,lcname,hrname,quantity,explanation) case Unit::cname: return Quantity::quantity;
        LIST_OF_UNITS
#undef X
    default:
        break;
    }
    return Quantity::Unknown;
}

Quantity toQuantity(string q)
{
#define X(qname,qunit) if (q == #qname) return Quantity::qname;
    LIST_OF_QUANTITIES
#undef X
        return Quantity::Unknown;
}

void assertQuantity(Unit u, Quantity q)
{
    if (!isQuantity(u, q))
    {
        error("Internal error! Unit is not of this quantity.");
    }
}

Unit defaultUnitForQuantity(Quantity q)
{
#define X(quantity,default_unit) if (q == Quantity::quantity) return Unit::default_unit;
    LIST_OF_QUANTITIES
#undef X

        return Unit::Unknown;
}

const char* toString(Quantity q)
{
#define X(quantity,default_unit) if (q == Quantity::quantity) return #quantity;
    LIST_OF_QUANTITIES
#undef X
        return "?";
}

Unit toUnit(string s)
{
#define X(cname,lcname,hrname,quantity,explanation) if (s == #cname || s == #lcname || s == hrname) return Unit::cname;
    LIST_OF_UNITS
#undef X

        return Unit::Unknown;
}

string unitToStringHR(Unit u)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return hrname;
    LIST_OF_UNITS
#undef X

        return "?";
}

string unitToStringLowerCase(Unit u)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return #lcname;
    LIST_OF_UNITS
#undef X

        return "?";
}

string unitToStringUpperCase(Unit u)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return #cname;
    LIST_OF_UNITS
#undef X

        return "?";
}

string strWithUnitHR(double v, Unit u)
{
    string r = format3fdot3f(v);
    r += " " + unitToStringHR(u);
    return r;
}

string strWithUnitLowerCase(double v, Unit u)
{
    string r = format3fdot3f(v);
    r += " " + unitToStringLowerCase(u);
    return r;
}

string valueToString(double v, Unit u)
{
    if (::isnan(v))
    {
        return "null";
    }
    string s = to_string(v);
    while (s.size() > 0 && s.back() == '0') s.pop_back();
    if (s.back() == '.') {
        s.pop_back();
        if (s.length() == 0) return "0";
        return s;
    }
    if (s.length() == 0) return "0";
    return s;
}

bool extractUnit(const string& s, string* vname, Unit* u)
{
    size_t pos;
    string vn;
    const char* c;

    if (s.length() < 3) goto err;

    pos = s.rfind('_');

    if (pos == string::npos) goto err;
    if (pos + 1 >= s.length()) goto err;
    vn = s.substr(0, pos);
    pos++;

    c = s.c_str() + pos;

#define X(cname,lcname,hrname,quantity,explanation) if (!strcmp(c, #lcname)) { *u = Unit::cname; *vname = vn; return true; }
    LIST_OF_UNITS
#undef X

        err :
    *vname = "";
    *u = Unit::Unknown;
    return false;
}

SIUnit::SIUnit(Unit u)
{
    quantity_ = toQuantity(u);

    switch (u)
    {
#define X(cname,si_scale,si_exponents) \
        case Unit::cname: scale_ = si_scale; exponents_ = si_exponents; break;
        LIST_OF_SI_CONVERSIONS
#undef X
    default:
        quantity_ = Quantity::Unknown;
        scale_ = 0;
    }
}

SIUnit::SIUnit(string s)
{
}

Unit SIUnit::asUnit() const
{
#define X(cname,si_scale,si_exponents) \
    if ((scale_ == si_scale) && (exponents_ == (si_exponents)) && quantity_ == toQuantity(Unit::cname)) return Unit::cname;
    LIST_OF_SI_CONVERSIONS
#undef X

        return Unit::Unknown;
}

Unit SIUnit::asUnit(Quantity q) const
{
#define X(cname,si_scale,si_exponents) \
    if ((scale_ == si_scale) && (exponents_ == (si_exponents)) && q == toQuantity(Unit::cname)) return Unit::cname;
    LIST_OF_SI_CONVERSIONS
#undef X

        return Unit::Unknown;
}

string super(uchar c)
{
    switch (c)
    {
    case '-': return "⁻";
    case '+': return "⁺";
    case '0': return "⁰";
    case '1': return "¹";
    case '2': return "²";
    case '3': return "³";
    case '4': return "⁴";
    case '5': return "⁵";
    case '6': return "⁶";
    case '7': return "⁷";
    case '8': return "⁸";
    case '9': return "⁹";
    }
    assert(false);
    return "?";
}

string to_superscript(int8_t n)
{
    string out;

    char buf[5];
    sprintf(buf, "%d", n);

    for (int i = 0; i < 5; ++i)
    {
        if (buf[i] == 0) break;
        out += super(buf[i]);
    }

    return out;
}

string to_superscript(string& s)
{
    string out;

    size_t i = 0;
    size_t len = s.length();

    // Skip non-superscript number.
    while (i < len && (s[i] == '-' || s[i] == '.' || (s[i] >= '0' && s[i] <= '9')))
    {
        out += s[i];
        i++;
    }

    while (i < len && (s[i] == 'e' || s[i] == 'E'))
    {
        i++;
        out += "×10";
    }

    bool found_start = false;
    while (i < len)
    {
        // Remove leading +0
        if (!found_start && s[i] != '+' && s[i] != '0') found_start = true;

        if (found_start)
        {
            out += super(s[i]);
        }
        i++;
    }

    return out;
}

string SIUnit::str() const
{
    string r = exponents_.str();

    string num = tostrprintf("%g", scale_);

    num = to_superscript(num);
    return num + r;
}


string SIUnit::info() const
{
    Unit u = asUnit();
    string unit = unitToStringLowerCase(u) + "|";
    if (unit == "?|") unit = "";
    string quantity = toString(quantity_) + string("|");
    if (quantity == "?|") quantity = "";

    return tostrprintf("[%s%s%s]",
        unit.c_str(),
        quantity.c_str(),
        str().c_str());
}

int8_t SIExp::safe_add(int8_t a, int8_t b)
{
    int sum = a + b;
    if (sum > 127) invalid_ = true;
    return sum;
}

int8_t SIExp::safe_sub(int8_t a, int8_t b)
{
    int diff = a - b;
    if (diff < -128) invalid_ = true;
    return diff;
}

int8_t SIExp::safe_div2(int8_t a)
{
    int8_t d = a / 2;
    if (d * 2 != a) invalid_ = true;
    return d;
}

bool SIExp::operator!=(const SIExp& e) const
{
    return !(*this == e);
}

bool SIExp::operator==(const SIExp& e) const
{
    return
        s_ == e.s_ &&
        m_ == e.m_ &&
        kg_ == e.kg_ &&
        a_ == e.a_ &&
        mol_ == e.mol_ &&
        cd_ == e.cd_ &&
        k_ == e.k_ &&
        c_ == e.c_ &&
        f_ == e.f_ &&
        month_ == e.month_ &&
        year_ == e.year_ &&
        unix_timestamp_ == e.unix_timestamp_;
}

SIExp SIExp::mul(const SIExp& e) const
{
    SIExp ee;

    ee.s(ee.safe_add(s(), e.s()))
        .m(ee.safe_add(m(), e.m()))
        .kg(ee.safe_add(kg(), e.kg()))
        .a(ee.safe_add(a(), e.a()))
        .mol(ee.safe_add(mol(), e.mol()))
        .cd(ee.safe_add(cd(), e.cd()))
        .k(ee.safe_add(k(), e.k()))
        .c(ee.safe_add(c(), e.c()))
        .f(ee.safe_add(f(), e.f()))
        .month(ee.safe_add(month(), e.month()))
        .year(ee.safe_add(year(), e.year()))
        .unixTimestamp(ee.safe_add(unixTimestamp(), e.unixTimestamp()));

    return ee;
}

SIExp SIExp::div(const SIExp& e) const
{
    SIExp ee;
    ee.s(ee.safe_sub(s(), e.s()))
        .m(ee.safe_sub(m(), e.m()))
        .kg(ee.safe_sub(kg(), e.kg()))
        .a(ee.safe_sub(a(), e.a()))
        .mol(ee.safe_sub(mol(), e.mol()))
        .cd(ee.safe_sub(cd(), e.cd()))
        .k(ee.safe_sub(k(), e.k()))
        .c(ee.safe_sub(c(), e.c()))
        .f(ee.safe_sub(f(), e.f()))
        .month(ee.safe_sub(month(), e.month()))
        .year(ee.safe_sub(year(), e.year()))
        .unixTimestamp(ee.safe_sub(unixTimestamp(), e.unixTimestamp()));

    return ee;
}

SIExp SIExp::sqrt() const
{
    SIExp ee;

    ee.s(ee.safe_div2(s()))
        .m(ee.safe_div2(m()))
        .kg(ee.safe_div2(kg()))
        .a(ee.safe_div2(a()))
        .mol(ee.safe_div2(mol()))
        .cd(ee.safe_div2(cd()))
        .k(ee.safe_div2(k()))
        .c(ee.safe_div2(c()))
        .f(ee.safe_div2(f()))
        .month(ee.safe_div2(month()))
        .year(ee.safe_div2(year()))
        .unixTimestamp(ee.safe_div2(unixTimestamp()));

    return ee;
}

#define DO_UNIT_SIEXP(var, name) if (var != 0) { if (r.length()>0) { } r += #name; if (var != 1) { r += to_superscript(var); } }

string SIExp::str() const
{
    string r;

    DO_UNIT_SIEXP(mol_, mol);
    DO_UNIT_SIEXP(cd_, cd);
    DO_UNIT_SIEXP(kg_, kg);
    DO_UNIT_SIEXP(m_, m);
    DO_UNIT_SIEXP(k_, k);
    DO_UNIT_SIEXP(c_, c);
    DO_UNIT_SIEXP(f_, f);
    DO_UNIT_SIEXP(s_, s);
    DO_UNIT_SIEXP(a_, a);
    DO_UNIT_SIEXP(month_, month);
    DO_UNIT_SIEXP(year_, year);
    DO_UNIT_SIEXP(unix_timestamp_, ut);

    if (invalid_) r = "!" + r + "-Invalid!";

    return r;
}

char available_quantities_[2048];

const char* availableQuantities()
{
    if (available_quantities_[0]) return available_quantities_;

#define X(q,u) if (Quantity::q != Quantity::Unknown) {                   \
        strcat(available_quantities_, #q); strcat(available_quantities_, "\n"); \
        assert(strlen(available_quantities_) < 1024); }
    LIST_OF_QUANTITIES
#undef X

        // Remove last newline
        available_quantities_[strlen(available_quantities_) - 1] = 0;
    return available_quantities_;
}

char available_units_[2048];

const char* availableUnits()
{
    if (available_units_[0]) return available_units_;

#define X(n,suffix,sn,q,ln) if (Unit::n != Unit::Unknown) {     \
        strcat(available_units_, #suffix); strcat(available_units_, "\n"); \
        assert(strlen(available_units_) < 1024); }
    LIST_OF_UNITS
#undef X

        // Remove last newline
        available_units_[strlen(available_units_) - 1] = 0;
    return available_units_;
}
