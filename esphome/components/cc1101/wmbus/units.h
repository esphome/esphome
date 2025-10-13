#pragma once
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

#ifndef UNITS_H
#define UNITS_H

#include<string>
#include<vector>
#include<cstdint>

// A named quantity has a preferred unit,
// ie Volume has m3 (cubic meters) Energy has kwh, Power has kw.
// We search for quantities in the mbus telegrams instead of
// hard coding a specific unit. This is useful since some
// meters can send either mj or kwh depending on their configuration.

#define LIST_OF_QUANTITIES    \
    X(Time,Hour)              \
    X(Length,M)               \
    X(Mass,KG)                \
    X(Amperage,Ampere)        \
    X(Temperature,C)          \
    X(AmountOfSubstance,MOL)  \
    X(LuminousIntensity,CD)   \
    \
    X(Energy,KWH)             \
    X(Reactive_Energy,KVARH)  \
    X(Apparent_Energy,KVAH)   \
    X(Power,KW)               \
    \
    X(Volume,M3)              \
    X(Flow,M3H)               \
    \
    X(Voltage,Volt)           \
    X(Frequency,Hertz)        \
    X(Pressure,BAR)           \
    \
    X(PointInTime,DateTimeLT) \
    \
    X(RelativeHumidity,RH)    \
    X(HCA,HCA)                \
    X(Text,TXT)               \
    X(Angle,DEGREE)           \
    X(Dimensionless,COUNTER)  \

enum class Quantity
{
#define X(quantity,default_unit) quantity,
    LIST_OF_QUANTITIES
#undef X
    Unknown
};

// A named unit (Unit) is a recurring unit that is useful to represent values
// sent in mbus telegrams. A named unit can be translated into an SIUnit
// which is unnamed and can encode any combination of SI units & scale.
// The SIUnits are used inside formulas. Eventually a successful formula
// calculation will map the final SI Unit to a named unit.
#define LIST_OF_UNITS \
    X(Second,s,"s",Time,"second")     \
    X(M,m,"m",Length,"meter")         \
    X(KG,kg,"kg",Mass,"kilogram")     \
    X(Ampere,a,"A",Amperage,"ampere") \
    X(K,k,"K",Temperature,"kelvin")   \
    X(MOL,mol,"mol",AmountOfSubstance,"mole")  \
    X(CD,cd,"cd",LuminousIntensity,"candela")  \
    \
    X(KWH,kwh,"kWh",Energy,"kilo Watt hour")   \
    X(MJ,mj,"MJ",Energy,"Mega Joule")          \
    X(GJ,gj,"GJ",Energy,"Giga Joule")          \
    X(KVARH,kvarh,"kVARh",Reactive_Energy,"kilo volt amperes reactive hour") \
    X(KVAH,kvah,"kVAh",Apparent_Energy,"kilo volt amperes hour")        \
    X(M3C,m3c,"m³°C",Energy,"cubic meter celsius")                      \
    \
    X(KW,kw,"kW",Power,"kilo Watt")                                     \
    X(M3CH,m3ch,"m³°C/h",Power,"cubic meter celsius per hour")          \
    \
    X(M3,m3,"m³",Volume,"cubic meter")                                  \
    X(L,l,"l",Volume,"litre")                                           \
    X(M3H,m3h,"m³/h",Flow,"cubic meters per hour")                      \
    X(LH,lh,"l/h",Flow,"liters per hour")                               \
    \
    X(C,c,"°C",Temperature,"celsius")                                   \
    X(F,f,"°F",Temperature,"fahrenheit")                                \
    \
    X(Volt,v,"V",Voltage,"volt")                                        \
    X(Hertz,hz,"Hz",Frequency,"hz")                                     \
    X(PA,pa,"pa",Pressure,"pascal")                                     \
    X(BAR,bar,"bar",Pressure,"bar")                                     \
    \
    X(Minute,min,"min",Time,"minute")                                   \
    X(Hour,h,"h",Time,"hour")                                           \
    X(Day,d,"d",Time,"day")                                             \
    X(Month,month,"month",Time,"month")                                 \
    X(Year,y,"y",Time,"year")                                           \
    X(UnixTimestamp,ut,"ut",PointInTime,"unix timestamp")               \
    X(DateTimeUTC,utc,"utc",PointInTime,"coordinated universal time")   \
    X(DateTimeLT,datetime,"datetime",PointInTime,"local time")          \
    X(DateLT,date,"date",PointInTime,"local date")                      \
    X(TimeLT,time,"time",PointInTime,"local time")                      \
    \
    X(RH,rh,"RH",RelativeHumidity,"relative humidity")                  \
    X(HCA,hca,"hca",HCA,"heat cost allocation")                         \
    X(TXT,txt,"txt",Text,"text")                                        \
    X(DEGREE,deg,"°",Angle,"degree")                                    \
    X(RADIAN,rad,"rad",Angle,"radian")                                  \
    X(COUNTER,counter,"counter",Dimensionless,"counter")                \
    X(FACTOR,factor,"factor",Dimensionless,"factor")                    \
    X(NUMBER,nr,"number",Dimensionless,"number")                        \
    X(PERCENTAGE,pct,"percentage",Dimensionless,"percentage")           \

enum class Unit
{
#define X(cname,lcname,hrname,quantity,explanation) cname,
    LIST_OF_UNITS
#undef X
    Unknown
};

// The SIUnit is used inside formulas and for conversion between units.
// Any numeric named Unit can be expressed using an SIUnit.
// Most SIUnits do not have a corresponding named Unit.
// However the result of a formula calculation or conversion will
// be eventually converted into a named unit. So even if you
// are allowed to write a formula: 9 kwh * 6 kwh which generates
// the unit kwh² which is not explicitly named above,
// you cannot assign this value to any calculated field since kwh^2
// is not named. However you can do: sqrt(10 kwh * 10 kwh) which
// will generated an SIUnit which is equivalent to kwh.
//
// Other valid formulas are for example:
// energy_kwh = 100 kw * 22 h
// flow_m3h = 100 m3 / 5 h
//
// We can only track units raised to the power of 127 or -128.
// The struct SIExp below is used to track the exponents of the units.
//
struct SIExp
{
    SIExp& s(int8_t i) { s_ = i; return *this; }
    SIExp& m(int8_t i) { m_ = i; return *this; }
    SIExp& kg(int8_t i) { kg_ = i; return *this; }
    SIExp& a(int8_t i) { a_ = i; return *this; }
    SIExp& mol(int8_t i) { mol_ = i; return *this; }
    SIExp& cd(int8_t i) { cd_ = i; return *this; }
    SIExp& k(int8_t i) { k_ = i; if (k_ != 0 && (c_ != 0 || f_ != 0)) { invalid_ = true; } return *this; }
    SIExp& c(int8_t i) { c_ = i; if (c_ != 0 && (k_ != 0 || f_ != 0)) { invalid_ = true; } return *this; }
    SIExp& f(int8_t i) { f_ = i; if (f_ != 0 && (k_ != 0 || c_ != 0)) { invalid_ = true; } return *this; }
    SIExp& month(int8_t i) { month_ = i; return *this; }
    SIExp& year(int8_t i) { year_ = i; return *this; }
    SIExp& unixTimestamp(int8_t i) { unix_timestamp_ = i; return *this; }

    int8_t s() const { return s_; }
    int8_t m() const { return m_; }
    int8_t kg() const { return kg_; }
    int8_t a() const { return a_; }
    int8_t mol() const { return mol_; }
    int8_t cd() const { return cd_; }
    int8_t k() const { return k_; }
    int8_t c() const { return c_; }
    int8_t f() const { return f_; }
    int8_t month() const { return month_; }
    int8_t year() const { return year_; }
    int8_t unixTimestamp() const { return unix_timestamp_; }

    SIExp mul(const SIExp& e) const;
    SIExp div(const SIExp& e) const;
    SIExp sqrt() const;
    int8_t safe_add(int8_t a, int8_t b);
    int8_t safe_sub(int8_t a, int8_t b);
    int8_t safe_div2(int8_t a);
    bool operator==(const SIExp& e) const;
    bool operator!=(const SIExp& e) const;

    std::string str() const;

    static SIExp build() { return SIExp(); }

private:

    int8_t s_{};
    int8_t m_{};
    int8_t kg_{};
    int8_t a_{};
    int8_t mol_{};
    int8_t cd_{};
    int8_t k_{};
    int8_t c_{}; // Why c and f here? Because they are offset against K.
    int8_t f_{}; // Since SIUnits are also used for conversions of values, not just unit tracking,
    // this means that the offset units (c,f) must be treated as distinct units.
    // E.g. if you calculated m3*k (and forget the m3 and k value) then you
    // cannot convert this value into m3*c since  the offset makes the calculation
    // depend on the k value, which we no longer know.
    // But kw*h can be translated into kw*s since scaling (*3600) can be done on the
    // calculated value without knowing the h value. Therefore we have to
    // treat k, c and f as distinct units. I.e. you cannot add m3*k+m3*f+m3*c.
    int8_t month_{}; // Why month and year here instead of using s? Because they vary in length and thus cannot
    int8_t year_{};  // be auto-converted to seconds, ie you cannot add/subtract months/years without knowing what you add to.
    int8_t unix_timestamp_{}; // Why not s? Because point in time is referenced agains an offset. UT is 1970-01-01 01:00.

    // If exponents have over/underflowed or if multiple of (k,c,f) (month,year,unix_timestamp) are set,
    // then the SIExp is not valid any more!

    bool invalid_ = false;
};

enum class MathOp
{
    ADD, SUB
};

struct SIUnit
{
    // Transform a double,double,uint64_t into an SIUnit.
    // The exp can be created compile time like this: SIUNIT(3.6E6, 0, SI_KG(1)|SI_M(2)|SI_S(-2)) which is kwh.
    SIUnit(Quantity q, double scale, SIExp exponents) :
        quantity_(q), scale_(scale), exponents_(exponents) {}
    // Transform a named unit into an SIUnit.
    SIUnit(Unit);
    // Parse string like: 3.6E106*kg*m^2*s^-3*a^−1 (ie volt)
    SIUnit(std::string s);
    // Return the known unit that best matches the SIUnit, or Unit::Unknown if none.
    Unit asUnit() const;
    // Return the known unit that best matches the SIUnit and the quantity, or Unit::Unknown if none.
    Unit asUnit(Quantity q) const;
    // Return the quantity that this unit is used for.
    Quantity quantity() const { return quantity_; }
    // Get the scale.
    double scale() const { return scale_; }
    // Get the exponents.
    const SIExp& exp() const { return exponents_; }
    // Return a string like 3.6⁶s⁻²m²kg
    std::string str() const;
    // Return a detailed string like: kwh[3.6⁶s⁻²m²kg]Energy
    std::string info() const;
    // Check if the exponents (ie units) are the same.
    bool sameExponents(SIUnit& to_siunit) const { return exponents_ == to_siunit.exponents_; }
    // Convert value from this unit to another unit and store it in out. Return false if conversion is impossible!
    bool convertTo(double left, const SIUnit& out_siunit, double* out) const;
    // Do a math op. Store the resulting unit and value into the destination pointers.
    // Return false if the addion cannot be performed.
    bool mathOpTo(MathOp op, double left, double right, const SIUnit& right_siunit, SIUnit* out_siunit, double* out) const;
    // Multiply this unit with another unit.
    SIUnit mul(const SIUnit& m) const;
    // Dividethis unit with another unit.
    SIUnit div(const SIUnit& m) const;
    // Square root this unit.
    SIUnit sqrt() const;

private:

    Quantity quantity_;
    double scale_;
    SIExp exponents_;
};

bool canConvert(Unit from, Unit to);
double convert(double v, Unit from, Unit to);
Unit whenMultiplied(Unit left, Unit right);
double multiply(double l, Unit left, double r, Unit right);

// Used to convert protocol KWH to KVARH/KVA, strictly speaking
// not a valid conversion, but permitted to work around limitations in the mbus protocol usage.
bool overrideConversion(Unit from, Unit to);

// Either uppercase KWH or lowercase kwh works here.
Unit toUnit(std::string s);
Quantity toQuantity(std::string s);
const SIUnit& toSIUnit(Unit u);
const char* toString(Quantity q);
bool isQuantity(Unit u, Quantity q);
Quantity toQuantity(Unit u);
void assertQuantity(Unit u, Quantity q);
Unit defaultUnitForQuantity(Quantity q);
std::string unitToStringHR(Unit u);
std::string unitToStringLowerCase(Unit u);
std::string unitToStringUpperCase(Unit u);
std::string valueToString(double v, Unit u);

bool extractUnit(const std::string& s, std::string* vname, Unit* u);

#define X(cname,lcname,hrname,quantity,explanation) extern const SIUnit SI_##cname;
LIST_OF_UNITS
#undef X

const char* availableQuantities();
const char* availableUnits();

#endif
