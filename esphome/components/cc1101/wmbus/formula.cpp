#include "formula.h"
/*
 Copyright (C) 2022 Fredrik �hrstr�m (gpl-3.0-or-later)

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

#include"formula.h"
#include"formula_implementation.h"
#include"meters.h"
#include"units.h"

#include<cmath>
#include<string.h>
#include<limits>
#include <iomanip>

NumericFormula::~NumericFormula() { }
NumericFormulaConstant::~NumericFormulaConstant() { }
NumericFormulaMeterField::~NumericFormulaMeterField() { }
NumericFormulaDVEntryField::~NumericFormulaDVEntryField() { }
NumericFormulaPair::~NumericFormulaPair() { }
NumericFormulaAddition::~NumericFormulaAddition() { }
NumericFormulaSubtraction::~NumericFormulaSubtraction() { }
NumericFormulaMultiplication::~NumericFormulaMultiplication() { }
NumericFormulaDivision::~NumericFormulaDivision() { }
NumericFormulaExponentiation::~NumericFormulaExponentiation() { }
NumericFormulaSquareRoot::~NumericFormulaSquareRoot() { }

double NumericFormulaConstant::calculate(SIUnit to)
{
    double r{};
    siunit().convertTo(constant_, to, &r);
    return r;
}

double NumericFormulaMeterField::calculate(SIUnit to_si_unit)
{
    if (formula()->meter() == NULL) return std::numeric_limits<double>::quiet_NaN();

    FieldInfo* fi = formula()->meter()->findFieldInfo(vname_, quantity_);

    Unit field_unit = fi->displayUnit();
    double val = formula()->meter()->getNumericValue(fi, field_unit);

    const SIUnit& field_si_unit = toSIUnit(field_unit);

    double r{};
    field_si_unit.convertTo(val, to_si_unit, &r);
    return r;
}

double NumericFormulaDVEntryField::calculate(SIUnit to_si_unit)
{
    if (formula()->dventry() == NULL) return std::numeric_limits<double>::quiet_NaN();

    double val = formula()->dventry()->getCounter(counter_);

    const SIUnit& counter_si_unit = toSIUnit(Unit::COUNTER);

    double r{};
    counter_si_unit.convertTo(val, to_si_unit, &r);
    return r;
}

double NumericFormulaAddition::calculate(SIUnit to_siunit)
{
    double l = left_->calculate(left_->siunit());
    double r = right_->calculate(right_->siunit());

    double v{};
    SIUnit v_siunit(Unit::COUNTER);
    left_->siunit().mathOpTo(MathOp::ADD, l, r, right_->siunit(), &v_siunit, &v);

    double result{};
    v_siunit.convertTo(v, to_siunit, &result);

    if (isDebugEnabled())
    {
        debug("(formula) ADD %g (%s) %g (%s) --> %g %s --> %g %s",
            l, left_->siunit().info().c_str(),
            r, right_->siunit().info().c_str(),
            v, v_siunit.info().c_str(),
            result, siunit().info().c_str());
    }

    return result;
}

double NumericFormulaSubtraction::calculate(SIUnit to_siunit)
{
    double l = left_->calculate(left_->siunit());
    double r = right_->calculate(right_->siunit());

    double v{};
    SIUnit v_siunit(Unit::COUNTER);
    left_->siunit().mathOpTo(MathOp::SUB, l, r, right_->siunit(), &v_siunit, &v);

    double result{};
    v_siunit.convertTo(v, to_siunit, &result);

    if (isDebugEnabled())
    {
        debug("(formula) SUB %g (%s) %g (%s) --> %g %s --> %g %s",
            l, left_->siunit().info().c_str(),
            r, right_->siunit().info().c_str(),
            v, v_siunit.info().c_str(),
            result, siunit().info().c_str());
    }

    return result;
}

double NumericFormulaMultiplication::calculate(SIUnit to_siunit)
{
    double l = left_->calculate(left_->siunit());
    double r = right_->calculate(right_->siunit());
    double m = l * r;
    double v{};
    siunit().convertTo(m, to_siunit, &v);

    if (isDebugEnabled())
    {
        debug("(formula) MUL %g (%s) %g (%s) --> %g --> %g %s",
            l, left_->siunit().info().c_str(),
            r, right_->siunit().info().c_str(),
            m,
            v, to_siunit.info().c_str());
    }
    return v;
}

double NumericFormulaDivision::calculate(SIUnit to_siunit)
{
    double l = left_->calculate(left_->siunit());
    double r = right_->calculate(right_->siunit());
    double d = l / r;
    double v{};
    siunit().convertTo(d, to_siunit, &v);

    if (isDebugEnabled())
    {
        debug("(formula) DIV %g (%s) %g (%s) --> %g --> %g %s",
            l, left_->siunit().info().c_str(),
            r, right_->siunit().info().c_str(),
            d,
            v,
            to_siunit.info().c_str());
    }

    return v;
}

double NumericFormulaExponentiation::calculate(SIUnit to_siunit)
{
    double l = left_->calculate(to_siunit);
    double r = right_->calculate(to_siunit);
    double p = pow(l, r);
    double v{};
    siunit().convertTo(p, to_siunit, &v);

    debug("(formula) %g <-- %g <-- pow %g ^ %g", v, p, l, r);
    return v;
}

double NumericFormulaSquareRoot::calculate(SIUnit to_siunit)
{
    double i = inner_->calculate(inner_->siunit());
    double s = sqrt(i);
    double v{};
    siunit().convertTo(s, to_siunit, &v);

    if (isDebugEnabled())
    {
        debug("(formula) SQRT %g (%s) --> %g --> %g %s",
            i, inner_->siunit().info().c_str(),
            s,
            v,
            to_siunit.info().c_str());
    }
    return v;
}

const char* toString(TokenType tt)
{
    switch (tt) {
    case TokenType::SPACE: return "SPACE";
    case TokenType::NUMBER: return "NUMBER";
    case TokenType::DATETIME: return "DATETIME";
    case TokenType::TIME: return "TIME";
    case TokenType::LPAR: return "LPAR";
    case TokenType::RPAR: return "RPAR";
    case TokenType::PLUS: return "PLUS";
    case TokenType::MINUS: return "MINUS";
    case TokenType::TIMES: return "TIMES";
    case TokenType::DIV: return "DIV";
    case TokenType::EXP: return "EXP";
    case TokenType::SQRT: return "SQRT";
    case TokenType::UNIT: return "UNIT";
    case TokenType::FIELD: return "FIELD";
    }
    return "?";
}

string Token::str(const string& s)
{
    string v = s.substr(start, len);
    return tostrprintf("%s(%s)", toString(type), v.c_str());
}

double Token::val(const string& s)
{
    string v = s.substr(start, len);
    if (type == TokenType::NUMBER)
    {
        return atof(v.c_str());
    }
    else if (type == TokenType::DATETIME)
    {
        struct tm time {};
        char* ok = strptime(v.c_str(), "'%Y-%m-%d %H:%M:%S'", &time);
        if (!ok) ok = strptime(v.c_str(), "'%Y-%m-%dT%H:%M:%SZ'", &time);
        time_t epoch = mktime(&time);
        double result = (double)epoch;
        return result;
    }
    else if (type == TokenType::TIME)
    {
        int h = 0;
        int m = 0;
        int s = 0;
        sscanf(v.c_str(), "'%02d:%02d:%02d'", &h, &m, &s);
        double result = h * 3600 + m * 60 + s;
        return result;
    }

    assert(0);
    return 0;
}

string Token::vals(const string& s)
{
    return s.substr(start, len);
}

Unit Token::unit(const string& s)
{
    string v = s.substr(start, len);
    return toUnit(v);
}

void FormulaImplementation::clear()
{
    valid_ = true;
    op_stack_.clear();
    tokens_.clear();
    formula_ = "";
    dventry_ = NULL;
    meter_ = NULL;
}

bool is_letter(char c)
{
    return c >= 'a' && c <= 'z';
}

bool is_letter_or_underscore(char c)
{
    return c == '_' || (c >= 'a' && c <= 'z');
}

bool is_letter_digit_or_underscore(char c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

size_t FormulaImplementation::findSpace(size_t i)
{
    size_t len = 0;
    while (isspace(formula_[i]))
    {
        i++;
        len++;
    }
    return len;
}

size_t FormulaImplementation::findNumber(size_t i)
{
    size_t len = 0;
    size_t start = i;
    int num_dots = 0;
    while (true)
    {
        if (i >= formula_.length()) break;
        char c = formula_[i];
        if ((c < '0' || c > '9') && c != '.') break;
        if (c == '.')
        {
            if (start == i) return 0; // Numbers do not start with a dot.
            num_dots++;
        }
        i++;
        len++;
    }

    if (num_dots > 1) return 0; // More than one decimal dot is an error.

    return len;
}

size_t FormulaImplementation::findDateTime(size_t i)
{
    // A datetime is converted into a unix timestamp.
    // Patterns: '2022-12-30T09:32:45Z'
    //           '2222-22-22 11:11:00'
    //           '2222-22-22 11:11'
    //           '2222-22-22'

    struct tm time {};
    const char* start = &formula_[i];
    const char* end;

    memset(&time, 0, sizeof(time));
    if (i + 21 < formula_.length())
    {
        end = strptime(start, "'%Y-%m-%dT%H:%M:%SZ'", &time);
        if (distance(start, end) == 22) return 22;
    }

    if (i + 20 < formula_.length())
    {
        end = strptime(start, "'%Y-%m-%d %H:%M:%S'", &time);
        if (distance(start, end) == 21) return 21;
    }

    if (i + 17 < formula_.length())
    {
        end = strptime(start, "'%Y-%m-%d %H:%M'", &time);
        if (distance(start, end) == 18) return 18;
    }

    if (i + 11 < formula_.length())
    {
        end = strptime(start, "'%Y-%m-%d'", &time);
        if (distance(start, end) == 12) return 12;
    }

    return 0;
}

size_t FormulaImplementation::findTime(size_t i)
{
    // A time is converted into seconds 10:11:12 is 10*3600+11*60+12 seconds.
    // Patterns: '11:22:15'
    //           '11:22'

    if (i + 9 < formula_.length() &&
        '\'' == formula_[i + 0] &&
        isdigit(formula_[i + 1]) &&
        isdigit(formula_[i + 2]) &&
        ':' == formula_[i + 3] &&
        isdigit(formula_[i + 4]) &&
        isdigit(formula_[i + 5]) &&
        ':' == formula_[i + 6] &&
        isdigit(formula_[i + 7]) &&
        isdigit(formula_[i + 8]) &&
        '\'' == formula_[i + 9])
    {
        return 10;
    }

    if (i + 6 < formula_.length() &&
        '\'' == formula_[i + 0] &&
        isdigit(formula_[i + 1]) &&
        isdigit(formula_[i + 2]) &&
        ':' == formula_[i + 3] &&
        isdigit(formula_[i + 4]) &&
        isdigit(formula_[i + 5]) &&
        '\'' == formula_[i + 6])
    {
        return 7;
    }

    return 0;
}


size_t FormulaImplementation::findPlus(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '+') return 1;

    return 0;
}

size_t FormulaImplementation::findMinus(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '-') return 1;

    return 0;
}

size_t FormulaImplementation::findTimes(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '*') return 1;

    return 0;
}

size_t FormulaImplementation::findDiv(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '/') return 1;

    return 0;
}

size_t FormulaImplementation::findExp(size_t i)
{
    return 0;
    /*
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '^') return 1;

    return 0;
    */
}

size_t FormulaImplementation::findSqrt(size_t i)
{
    if (i + 4 >= formula_.length()) return 0;

    if (!strncmp(&formula_[i], "sqrt", 4) && !is_letter(formula_[i + 4]))
    {
        return 4;
    }

    return 0;
}

size_t FormulaImplementation::findLPar(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '(') return 1;

    return 0;
}

size_t FormulaImplementation::findRPar(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == ')') return 1;

    return 0;
}

size_t FormulaImplementation::findUnit(size_t i)
{
    if (i >= formula_.length()) return 0;

    size_t len = formula_.length();
    char c = formula_[i];

    // All units start with a lower case a-z, followed by more letters and _ underscores.
    if (!is_letter(c)) return 0;

    size_t longest_unit = 0;

#define X(cname,lcname,hrname,quantity,explanation) \
    if ( (i+sizeof(#lcname)-1 <= len) &&                                \
         !is_letter_digit_or_underscore(formula_[i+sizeof(#lcname)-1]) &&     \
         !strncmp(#lcname, formula_.c_str()+i, sizeof(#lcname)-1) &&    \
         longest_unit < (sizeof(#lcname)-1)) { longest_unit = (sizeof(#lcname)-1); }
    LIST_OF_UNITS
#undef X

        return longest_unit;
}

size_t FormulaImplementation::findField(size_t i)
{
    size_t len = 0;
    size_t start = i;

    // All field names are lower case a-z.
    if (!is_letter(formula_[start])) return 0;

    while (true)
    {
        if (i >= formula_.length()) break;
        char c = formula_[i];
        // After first letter field names can contain more letters digits and underscores.
        if (!is_letter_digit_or_underscore(c)) break;
        i++;
        len++;
    }

    return len;
}

bool FormulaImplementation::tokenize()
{
    size_t i = 0;

    while (i < formula_.length())
    {
        size_t len;

        len = findSpace(i);
        if (len > 0) { i += len; continue; } // No token added for whitespace.

        len = findDateTime(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::DATETIME, i, len)); i += len; continue; }

        len = findTime(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::TIME, i, len)); i += len; continue; }

        len = findNumber(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::NUMBER, i, len)); i += len; continue; }

        len = findLPar(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::LPAR, i, len)); i += len; continue; }

        len = findRPar(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::RPAR, i, len)); i += len; continue; }

        len = findPlus(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::PLUS, i, len)); i += len; continue; }

        len = findMinus(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::MINUS, i, len)); i += len; continue; }

        len = findTimes(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::TIMES, i, len)); i += len; continue; }

        len = findDiv(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::DIV, i, len)); i += len; continue; }

        len = findExp(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::EXP, i, len)); i += len; continue; }

        len = findSqrt(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::SQRT, i, len)); i += len; continue; }

        len = findUnit(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::UNIT, i, len)); i += len; continue; }

        len = findField(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::FIELD, i, len)); i += len; continue; }

        break;
    }

    // Interrupted early, thus there was an error tokenizing.
    if (i < formula_.length())
    {
        Token tok(TokenType::SPACE, i, 0);
        errors_.push_back(tostrprintf("Unknown token!\n" + tok.withMarker(formula_)));
        valid_ = false;
        return false;
    }

    return true;
}

size_t FormulaImplementation::parseOps(size_t i)
{
    Token* tok = LA(i);
    Token* next = LA(i + 1);
    if (tok == NULL) return i;

    if (tok->type == TokenType::FIELD)
    {
        handleField(tok);
        return i + 1;
    }

    if (tok->type == TokenType::DATETIME)
    {
        handleUnixTimestamp(tok);
        return i + 1;
    }

    if (tok->type == TokenType::TIME)
    {
        handleSeconds(tok);
        return i + 1;
    }

    if (tok->type == TokenType::PLUS)
    {
        size_t next = parseOps(i + 1);
        if (!valid_) return next;
        handleAddition(tok);
        return next;
    }

    if (tok->type == TokenType::MINUS)
    {
        size_t next = parseOps(i + 1);
        if (!valid_) return next;
        handleSubtraction(tok);
        return next;
    }

    if (tok->type == TokenType::TIMES)
    {
        size_t next = parseOps(i + 1);
        if (!valid_) return next;
        handleMultiplication(tok);
        return next;
    }

    if (tok->type == TokenType::DIV)
    {
        size_t next = parseOps(i + 1);
        if (!valid_) return next;
        handleDivision(tok);
        return next;
    }

    if (tok->type == TokenType::EXP)
    {
        size_t next = parseOps(i + 1);
        if (!valid_) return next;
        handleExponentiation(tok);
        return next;
    }

    if (tok->type == TokenType::SQRT)
    {
        size_t next = parseOps(i + 1);
        if (!valid_) return next;
        handleSquareRoot(tok);
        return next;
    }

    if (tok->type == TokenType::LPAR)
    {
        return parsePar(i);
    }

    if (tok->type == TokenType::RPAR)
    {
        return i;
    }

    if (tok->type == TokenType::NUMBER)
    {
        if (next == NULL || next->type != TokenType::UNIT)
        {
            errors_.push_back(tostrprintf("Constant number %s lacks a valid unit!\n%s",
                tok->vals(formula_).c_str(),
                tok->withMarker(formula_).c_str()));
            valid_ = false;
            return i;
        }
        handleConstant(tok, next);
        return i + 2;
    }

    return i;
}

size_t FormulaImplementation::parsePar(size_t i)
{
    Token* tok = LA(i);
    assert(tok->type == TokenType::LPAR);

    i++;
    for (;;)
    {
        tok = LA(i);
        if (tok == NULL) break;
        if (tok->type == TokenType::RPAR) break;
        size_t next = parseOps(i);
        if (!valid_ || next == i) break;
        i = next;
    }

    if (valid_ && tok == NULL)
    {
        errors_.push_back("Missing closing parenthesis at end of formula!\n");
        valid_ = false;
        return i;
    }

    if (valid_ && tok->type != TokenType::RPAR)
    {
        errors_.push_back("Expected closing parenthesis!\n" + tok->withMarker(formula_));
        valid_ = false;
        return i;
    }
    return i + 1;
}

void FormulaImplementation::handleConstant(Token* number, Token* unit)
{
    double c = number->val(formula_);
    Unit u = unit->unit(formula_);

    if (u == Unit::Unknown)
    {
        errors_.push_back(tostrprintf("Unknown unit \"%s\"!\n%s",
            unit->vals(formula_).c_str(),
            unit->withMarker(formula_).c_str()));
        valid_ = false;
        return;
    }

    doConstant(u, c);
}

void FormulaImplementation::handleUnixTimestamp(Token* number)
{
    double c = number->val(formula_);

    doConstant(Unit::UnixTimestamp, c);
}

void FormulaImplementation::handleSeconds(Token* number)
{
    double c = number->val(formula_);
    Unit u = Unit::Second;

    doConstant(u, c);
}

void FormulaImplementation::handleAddition(Token* tok)
{
    SIUnit right_siunit = topOp()->siunit();
    SIUnit left_siunit = top2Op()->siunit();
    SIUnit to_siunit(Unit::COUNTER);

    bool ok = left_siunit.mathOpTo(MathOp::ADD, 0, 0, right_siunit, &to_siunit, NULL);

    if (!ok)
    {
        string lsis = left_siunit.str();
        string rsis = right_siunit.str();
        errors_.push_back(tostrprintf("Cannot add %s to %s!\n%s",
            left_siunit.info().c_str(),
            right_siunit.info().c_str(),
            tok->withMarker(formula_).c_str()));
        valid_ = false;
        return;
    }

    doAddition(to_siunit);
}

void FormulaImplementation::handleSubtraction(Token* tok)
{
    SIUnit right_siunit = topOp()->siunit();
    SIUnit left_siunit = top2Op()->siunit();

    SIUnit v_siunit(Unit::COUNTER);

    bool ok = left_siunit.mathOpTo(MathOp::SUB, 0, 0, right_siunit, &v_siunit, NULL);

    if (!ok)
    {
        string lsis = left_siunit.str();
        string rsis = right_siunit.str();
        errors_.push_back(tostrprintf("Cannot subtract %s from %s!\n%s",
            left_siunit.info().c_str(),
            right_siunit.info().c_str(),
            tok->withMarker(formula_).c_str()));
        valid_ = false;
        return;
    }

    doSubtraction(v_siunit);
}

void FormulaImplementation::handleMultiplication(Token* tok)
{
    // Any two units can be multiplied! You might not like the answer thought....
    doMultiplication();
}

void FormulaImplementation::handleDivision(Token* tok)
{
    // Any two units can be divided! You might not like the answer thought....
    doDivision();
}

void FormulaImplementation::handleExponentiation(Token* tok)
{
    // You can only exponentiate to a number.
    doExponentiation();
}

void FormulaImplementation::handleSquareRoot(Token* tok)
{
    doSquareRoot();
}

void FormulaImplementation::handleField(Token* field)
{
    string field_name = field->vals(formula_); // Full field: total_m3
    string vname;                              // Without unit: total
    Unit named_unit;                      // The extracted unit: m3
    bool ok = extractUnit(field_name, &vname, &named_unit);

    if (!ok)
    {
        errors_.push_back("Cannot extra a valid unit from field name \"" + field_name + "\"\n" + field->withMarker(formula_));
        return;
    }

    DVEntryCounterType dve = toDVEntryCounterType(field_name);

    if (dve != DVEntryCounterType::UNKNOWN)
    {
        debug("(formula) handle dventry field %s into %s %s", field_name.c_str(), vname.c_str(),
            unitToStringLowerCase(named_unit).c_str());
        doDVEntryField(named_unit, dve);
    }
    else
    {
        debug("(formula) handle meter field %s into %s %s", field_name.c_str(), vname.c_str(),
            unitToStringLowerCase(named_unit).c_str());

        Quantity q = toQuantity(named_unit);
        FieldInfo* f = meter_->findFieldInfo(vname, q);

        if (f == NULL)
        {
            errors_.push_back("No such field found \"" + field_name + "\"\n" + field->withMarker(formula_));
            valid_ = false;
            return;
        }

        doMeterField(named_unit, f);
    }
}

bool FormulaImplementation::go()
{
    size_t i = 0;

    for (;;)
    {
        size_t next = parseOps(i);
        if (!valid_ || next == i) break;
        i = next;
    }

    return i == tokens_.size();
}

Token* FormulaImplementation::LA(size_t i)
{
    if (i < 0 || i >= tokens_.size()) return NULL;
    return &tokens_[i];
}

bool FormulaImplementation::parse(Meter* m, const string& f)
{
    bool ok;

    clear();

    meter_ = m;
    formula_ = f;

    debug("(formula) parsing \"%s\"", formula_.c_str());

    ok = tokenize();
    if (!ok) return false;

    if (isDebugEnabled())
    {
        debug("(formula) tokens: ");
        for (Token& t : tokens_)
        {
            debug("%s ", t.str(formula_).c_str());
        }
    }

    ok = go();
    if (!ok) return false;

    if (isDebugEnabled())
    {
        debug("(formula) %s", tree().c_str());
    }
    return valid_;
}

bool FormulaImplementation::valid()
{
    return valid_ == true && op_stack_.size() == 1;
}

string FormulaImplementation::errors()
{
    string s;
    for (string& e : errors_) s += e;
    return s;
}

double FormulaImplementation::calculate(Unit to, DVEntry* dve, Meter* m)
{
    if (dve != NULL) dventry_ = dve;
    if (m != NULL) meter_ = m;

    if (!valid_)
    {
        string t = tree();
        warning("Warning! Formula is not valid! Returning nan!\n%s", t.c_str());
        return std::nan("");
    }

    if (op_stack_.size() != 1)
    {
        string t = tree();
        warning("Warning! Formula is not valid! Multiple ops on stack! Returning nan!\n%s", t.c_str());
        return std::nan("");
    }

    return topOp()->calculate(toSIUnit(to));
}

void FormulaImplementation::doConstant(Unit u, double c)
{
    pushOp(new NumericFormulaConstant(this, u, c));
}

void FormulaImplementation::doAddition(const SIUnit& to_siunit)
{
    assert(op_stack_.size() >= 2);

    unique_ptr<NumericFormula> right_node = popOp();
    unique_ptr<NumericFormula> left_node = popOp();

    pushOp(new NumericFormulaAddition(this, to_siunit, left_node, right_node));
}

void FormulaImplementation::doSubtraction(const SIUnit& to_siunit)
{
    assert(op_stack_.size() >= 2);

    unique_ptr<NumericFormula> right_node = popOp();
    unique_ptr<NumericFormula> left_node = popOp();

    pushOp(new NumericFormulaSubtraction(this, to_siunit, left_node, right_node));
}

void FormulaImplementation::doMultiplication()
{
    assert(op_stack_.size() >= 2);

    SIUnit right_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> right_node = popOp();

    SIUnit left_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> left_node = popOp();

    SIUnit mul_siunit = left_siunit.mul(right_siunit);

    string lsis = left_siunit.info();
    string rsis = right_siunit.info();
    string msis = mul_siunit.info();

    pushOp(new NumericFormulaMultiplication(this, mul_siunit, left_node, right_node));
}

void FormulaImplementation::doDivision()
{
    assert(op_stack_.size() >= 2);

    SIUnit right_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> right_node = popOp();

    SIUnit left_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> left_node = popOp();

    SIUnit div_siunit = left_siunit.div(right_siunit);

    string lsis = left_siunit.info();
    string rsis = right_siunit.info();
    string dsis = div_siunit.info();

    debug("(formula) unit %s DIV %s ==> %s", lsis.c_str(), rsis.c_str(), dsis.c_str());

    pushOp(new NumericFormulaDivision(this, div_siunit, left_node, right_node));
}

void FormulaImplementation::doExponentiation()
{
    assert(op_stack_.size() >= 2);

    //    SIUnit right_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> right_node = popOp();

    SIUnit left_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> left_node = popOp();

    pushOp(new NumericFormulaDivision(this, left_siunit, left_node, right_node));

    //    assert(canConvert(left_siunit, right_siunit));
}

void FormulaImplementation::doSquareRoot()
{
    assert(op_stack_.size() >= 1);

    SIUnit inner_siunit = topOp()->siunit();

    SIUnit siunit = inner_siunit.sqrt();

    unique_ptr<NumericFormula> inner_node = popOp();

    pushOp(new NumericFormulaSquareRoot(this, siunit, inner_node));
}


void FormulaImplementation::doMeterField(Unit u, FieldInfo* fi)
{
    SIUnit from_si_unit = toSIUnit(fi->displayUnit());
    SIUnit to_si_unit = toSIUnit(u);
    assert(from_si_unit.convertTo(0, to_si_unit, NULL));

    pushOp(new NumericFormulaMeterField(this, u, fi->vname(), fi->xuantity()));
}

void FormulaImplementation::doDVEntryField(Unit u, DVEntryCounterType ct)
{
    pushOp(new NumericFormulaDVEntryField(this, Unit::COUNTER, ct));
}

Formula* newFormula()
{
    return new FormulaImplementation();
}

Formula::~Formula()
{
}

FormulaImplementation::~FormulaImplementation()
{
}

string FormulaImplementation::str()
{
    return topOp()->str();
}

string FormulaImplementation::tree()
{
    string s;

    for (auto& op : op_stack_)
    {
        if (s.length() > 0) s += " | ";
        s += op->tree();
        if (s.back() == ' ') s.pop_back();
    }
    return s;
}

void FormulaImplementation::setMeter(Meter* m)
{
    meter_ = m;
}

void FormulaImplementation::setDVEntry(DVEntry* dve)
{
    dventry_ = dve;
}

string NumericFormulaConstant::str()
{
    return tostrprintf("%.17g %s", constant_, siunit());
}

string NumericFormulaConstant::tree()
{
    Quantity q = siunit().quantity();
    Unit nearest = siunit().asUnit(q);
    string sis = siunit().str();

    return tostrprintf("<CONST %.17g %s[%s]%s> ", constant_, unitToStringLowerCase(nearest).c_str(), sis.c_str(), toString(q));
}

string NumericFormulaPair::str()
{
    string left = left_->tree();
    string right = right_->tree();
    return left + " " + op_ + " " + right;
}

string NumericFormulaPair::tree()
{
    string left = left_->tree();
    string right = right_->tree();
    return "<" + name_ + " " + left + right + "> ";
}

string NumericFormulaSquareRoot::str()
{
    string inner = inner_->str();
    return "sqrt(" + inner + ")";
}

string NumericFormulaSquareRoot::tree()
{
    string inner = inner_->tree();
    return "<SQRT " + inner + "> ";
}

string NumericFormulaMeterField::str()
{
    if (formula()->meter() == NULL) return "<?" + vname_ + "?>";
    FieldInfo* fi = formula()->meter()->findFieldInfo(vname_, quantity_);

    return fi->vname() + "_" + unitToStringLowerCase(fi->displayUnit());
}

string NumericFormulaMeterField::tree()
{
    if (formula()->meter() == NULL) return "<?" + vname_ + "?>";
    FieldInfo* fi = formula()->meter()->findFieldInfo(vname_, quantity_);
    return "<FIELD " + fi->vname() + "_" + unitToStringLowerCase(fi->displayUnit()) + "> ";
}

string NumericFormulaDVEntryField::str()
{
    return toString(counter_);
}

string NumericFormulaDVEntryField::tree()
{
    return string("<DVENTRY ") + toString(counter_) + "> ";
}

void FormulaImplementation::pushOp(NumericFormula* nf)
{
    op_stack_.push_back(unique_ptr<NumericFormula>(nf));
}

unique_ptr<NumericFormula> FormulaImplementation::popOp()
{
    assert(op_stack_.size() > 0);
    unique_ptr<NumericFormula> nf = std::move(op_stack_.back());
    op_stack_.pop_back();
    return nf;
}

NumericFormula* FormulaImplementation::topOp()
{
    assert(op_stack_.size() > 0);
    return op_stack_.back().get();
}

NumericFormula* FormulaImplementation::top2Op()
{
    assert(op_stack_.size() > 1);
    return op_stack_[op_stack_.size() - 2].get();
}

string Token::withMarker(const string& formula)
{
    string indent;
    size_t n = start;
    while (n > 0)
    {
        indent += " ";
        n--;
    }
    return formula + "\n" + indent + "^~~~~\n";
}

StringInterpolator* newStringInterpolator()
{
    return new StringInterpolatorImplementation();
}

StringInterpolator::~StringInterpolator()
{
}

StringInterpolatorImplementation::~StringInterpolatorImplementation()
{
}

bool StringInterpolatorImplementation::parse(const std::string& f)
{
    strings_.clear();
    formulas_.clear();

    size_t prev_string_start = 0;
    size_t next_start_brace = f.find('{', prev_string_start);

    while (next_start_brace != string::npos)
    {
        // Push the string up to the brace.
        string part = f.substr(prev_string_start, next_start_brace - prev_string_start);
        strings_.push_back(part);

        // Find the end of the formula.
        size_t next_end_brace = f.find('}', next_start_brace);
        if (next_end_brace == string::npos) return false; // Oups, missing closing }

        string formula = f.substr(next_start_brace + 1, next_end_brace - next_start_brace - 1);

        formulas_.push_back(unique_ptr<Formula>(newFormula()));
        bool ok = formulas_.back()->parse(NULL, formula);
        if (!ok) return false;

        prev_string_start = next_end_brace + 1;

        next_start_brace = f.find('{', prev_string_start);
    }

    // Add any remaining string segment after the last formula.
    if (prev_string_start < f.length())
    {
        string part = f.substr(prev_string_start);
        strings_.push_back(part);
    }

    return true;
}

string StringInterpolatorImplementation::apply(DVEntry* dve)
{
    string result;
    size_t s = 0;
    size_t f = 0;

    for (;;)
    {
        if (s >= strings_.size() && f >= formulas_.size()) break;

        if (s < strings_.size())
        {
            result += strings_[s];
            s++;
        }

        if (f < formulas_.size())
        {
            if (dve != NULL)
            {
                result += tostrprintf("%g", formulas_[f]->calculate(Unit::COUNTER, dve));
            }
            else
            {
                result += "{NULL}";
            }
            f++;
        }
    }

    return result;
}
