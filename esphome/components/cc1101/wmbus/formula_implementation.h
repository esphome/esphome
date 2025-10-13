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

#ifndef FORMULA_IMPLEMENTATION
#define FORMULA_IMPLEMENTATION

#include"formula.h"
#include"meters.h"

#include<stack>

struct FormulaImplementation;

struct NumericFormula
{
    NumericFormula(FormulaImplementation* f, SIUnit u) : formula_(f), siunit_(u) { }
    SIUnit& siunit() { return siunit_; }
    // Calculate the formula and return the value in the given "to" unit.
    virtual double calculate(SIUnit to) = 0;
    virtual string str() = 0;
    virtual string tree() = 0;
    virtual ~NumericFormula() = 0;

    FormulaImplementation* formula() { return formula_; }

private:

    FormulaImplementation* formula_;
    SIUnit siunit_;
};

struct NumericFormulaConstant : public NumericFormula
{
    NumericFormulaConstant(FormulaImplementation* f, Unit u, double c) : NumericFormula(f, u), constant_(c) {}
    double calculate(SIUnit to);
    string str();
    string tree();
    ~NumericFormulaConstant();

private:

    double constant_;
};

struct NumericFormulaMeterField : public NumericFormula
{
    NumericFormulaMeterField(FormulaImplementation* f, Unit u, string v, Quantity q)
        : NumericFormula(f, u), vname_(v), quantity_(q) {}

    double calculate(SIUnit to);
    string str();
    string tree();
    ~NumericFormulaMeterField();

private:

    string vname_;
    Quantity quantity_;
};

struct NumericFormulaDVEntryField : public NumericFormula
{
    NumericFormulaDVEntryField(FormulaImplementation* f, Unit u, DVEntryCounterType ct) : NumericFormula(f, u), counter_(ct) {}

    double calculate(SIUnit to);
    string str();
    string tree();
    ~NumericFormulaDVEntryField();

private:

    DVEntryCounterType counter_;
};

struct NumericFormulaPair : public NumericFormula
{
    NumericFormulaPair(FormulaImplementation* f, SIUnit siu,
        unique_ptr<NumericFormula>& a,
        unique_ptr<NumericFormula>& b,
        string name, string op)
        : NumericFormula(f, siu),
        left_(std::move(a)),
        right_(std::move(b)),
        name_(name),
        op_(op)
    {}

    string str();
    string tree();
    ~NumericFormulaPair();

protected:

    std::unique_ptr<NumericFormula> left_;
    std::unique_ptr<NumericFormula> right_;
    std::string name_;
    std::string op_;
};

struct NumericFormulaAddition : public NumericFormulaPair
{
    NumericFormulaAddition(FormulaImplementation* f, SIUnit siu,
        unique_ptr<NumericFormula>& a,
        unique_ptr<NumericFormula>& b)
        : NumericFormulaPair(f, siu, a, b, "ADD", "+") {}

    double calculate(SIUnit to);

    ~NumericFormulaAddition();
};

struct NumericFormulaSubtraction : public NumericFormulaPair
{
    NumericFormulaSubtraction(FormulaImplementation* f, SIUnit siu,
        unique_ptr<NumericFormula>& a,
        unique_ptr<NumericFormula>& b)
        : NumericFormulaPair(f, siu, a, b, "SUB", "-") {}

    double calculate(SIUnit to);

    ~NumericFormulaSubtraction();
};

struct NumericFormulaMultiplication : public NumericFormulaPair
{
    NumericFormulaMultiplication(FormulaImplementation* f, SIUnit siu,
        unique_ptr<NumericFormula>& a,
        unique_ptr<NumericFormula>& b)
        : NumericFormulaPair(f, siu, a, b, "TIMES", "×") {}

    double calculate(SIUnit to);

    ~NumericFormulaMultiplication();
};

struct NumericFormulaDivision : public NumericFormulaPair
{
    NumericFormulaDivision(FormulaImplementation* f, SIUnit siu,
        unique_ptr<NumericFormula>& a,
        unique_ptr<NumericFormula>& b)
        : NumericFormulaPair(f, siu, a, b, "DIV", "÷") {}

    double calculate(SIUnit to);

    ~NumericFormulaDivision();
};

struct NumericFormulaExponentiation : public NumericFormulaPair
{
    NumericFormulaExponentiation(FormulaImplementation* f, SIUnit siu,
        unique_ptr<NumericFormula>& a,
        unique_ptr<NumericFormula>& b)
        : NumericFormulaPair(f, siu, a, b, "EXP", "^") {}

    double calculate(SIUnit to);

    ~NumericFormulaExponentiation();
};

struct NumericFormulaSquareRoot : public NumericFormula
{
    NumericFormulaSquareRoot(FormulaImplementation* f, SIUnit siu,
        unique_ptr<NumericFormula>& inner)
        : NumericFormula(f, siu), inner_(std::move(inner)) {}

    double calculate(SIUnit to);
    string str();
    string tree();

    ~NumericFormulaSquareRoot();

private:

    std::unique_ptr<NumericFormula> inner_;
};

enum class TokenType
{
    SPACE,
    LPAR,
    RPAR,
    NUMBER,
    DATETIME,
    TIME,
    PLUS,
    MINUS,
    TIMES,
    DIV,
    EXP,
    SQRT,
    UNIT,
    FIELD
};

const char* toString(TokenType tt);

struct Token
{
    Token(TokenType t, size_t s, size_t l) : type(t), start(s), len(l) {}

    TokenType type;
    size_t start;
    size_t len;

    string str(const string& s);
    string vals(const string& s);
    double val(const string& s);
    Unit unit(const string& s);

    string withMarker(const string& s);
};

struct FormulaImplementation : public Formula
{
    bool parse(Meter* m, const string& f);
    bool valid();
    string errors();
    double calculate(Unit to, DVEntry* dve = NULL, Meter* m = NULL);
    void clear();
    string str();
    string tree();
    void setMeter(Meter* m);
    void setDVEntry(DVEntry* dve);

    // Pushes a constant on the formula builder stack.
    void doConstant(Unit u, double c);
    // Pushes a meter field read on the formula builder stack.
    void doMeterField(Unit u, FieldInfo* fi);
    // Pushes a dve entry field read on the formula builder stack.
    void doDVEntryField(Unit u, DVEntryCounterType ct);
    // Pops the two top nodes of the formula builder stack and pushes an addition on the formula builder stack.
    // The target unit will be the supplied to unit.
    void doAddition(const SIUnit& to);
    // Pops the two top nodes of the formula builder stack and pushes a subtraction on the formula builder stack.
    // The target unit will be the supplied to unit.
    void doSubtraction(const SIUnit& to);
    // Pops the two top nodes of the formula builder stack and pushes a multiplication on the formula builder stack.
    // The target unit will be multiplication of the SI Units.
    void doMultiplication();
    // Pops the two top nodes of the formula builder stack and pushes a division on the formula builder stack.
    // The target unit will be first SIUnit divided by the second SIUnit.
    void doDivision();
    // Pops the two top nodes of the formula builder stack and pushes an exponentiation on the formula builder stack.
    // The target unit will be first SIUnit exponentiated.
    void doExponentiation();
    // Pops the single top node of the formula builder stack and pushes an squareroot on the formula builder stack.
    // The target unit will be SIUnit square rooted.
    void doSquareRoot();

    ~FormulaImplementation();

    bool tokenize();
    bool go();
    size_t findSpace(size_t i);
    size_t findNumber(size_t i);
    size_t findDateTime(size_t i);
    size_t findTime(size_t i);
    size_t findUnit(size_t i);
    size_t findPlus(size_t i);
    size_t findMinus(size_t i);
    size_t findTimes(size_t i);
    size_t findDiv(size_t i);
    size_t findExp(size_t i);
    size_t findSqrt(size_t i);
    size_t findLPar(size_t i);
    size_t findRPar(size_t i);
    size_t findField(size_t i);

    Token* LA(size_t i);
    size_t parseOps(size_t i);
    size_t parsePar(size_t i);

    void handleConstant(Token* number, Token* unit);
    void handleSeconds(Token* number);
    void handleUnixTimestamp(Token* number);
    void handleAddition(Token* add);
    void handleSubtraction(Token* add);
    void handleMultiplication(Token* add);
    void handleDivision(Token* add);
    void handleExponentiation(Token* add);
    void handleSquareRoot(Token* add);
    void handleField(Token* field);

    void pushOp(NumericFormula* nf);
    std::unique_ptr<NumericFormula> popOp();
    NumericFormula* topOp();
    NumericFormula* top2Op();

    Meter* meter() { return meter_; }
    DVEntry* dventry() { return dventry_; }

private:

    bool valid_ = true;
    std::vector<std::unique_ptr<NumericFormula>> op_stack_;
    std::vector<Token> tokens_;
    std::string formula_; // To be parsed.
    Meter* meter_; // To be referenced when parsing and calculating.
    DVEntry* dventry_; // To be referenced when calculating.

    // Any errors during parsing are store here.
    std::vector<std::string> errors_;
};

struct StringInterpolatorImplementation : public StringInterpolator
{
    // Create a string interpolation from for example: "historic_{storage_counter / - 12 counter}_value"
    // Which for a dventry with storage 13 will "generate historic_1_value"
    bool parse(const std::string& f);
    std::string apply(DVEntry* dve);
    ~StringInterpolatorImplementation();

    // The strings store "historic_" "_value"
    std::vector<std::string> strings_;
    // The formula stores the parsed "storage_counter / - 12 counter" formula.
    std::vector<std::unique_ptr<Formula>> formulas_;
};

#endif
