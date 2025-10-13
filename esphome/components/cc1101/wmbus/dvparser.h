#pragma once
/*
 Copyright (C) 2018-2022 Fredrik �hrstr�m (gpl-3.0-or-later)

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

#ifndef DVPARSER_H
#define DVPARSER_H

#include"utils.h"
#include"units.h"

#include<map>
#include<set>
#include<cstdint>
#include<time.h>
#include<functional>
#include<vector>
#include "types.h"
#include "Telegram.h"


const char* toString(VIFRange v);
VIFRange toVIFRange(const char* s);
Unit toDefaultUnit(VIFRange v);

Unit toDefaultUnit(Vif v);
VIFRange toVIFRange(int i);
bool isInsideVIFRange(int i, VIFRange range);
std::string difType(int dif);
std::string vifType(int vif); // Long description
double vifScale(int vif);
std::string vifeType(int dif, int vif, int vife); // Long description
static IndexNr AnyIndexNr = IndexNr(-1);

struct FieldMatcher
{
    // If not actually used, this remains false.
    bool active = false;

    // Exact difvif hex string matching all other checks are ignored.
    bool match_dif_vif_key = false;
    DifVifKey dif_vif_key{ "" };

    // Match the measurement_type.
    bool match_measurement_type = false;
    MeasurementType measurement_type{ MeasurementType::Instantaneous };

    // Match the value information range. See dvparser.h
    bool match_vif_range = false;
    VIFRange vif_range{ VIFRange::Any };

    // Match the vif exactly, used for manufacturer specific vifs.
    bool match_vif_raw = false;
    uint16_t vif_raw{};

    // Match any vif combinables.
    std::set<VIFCombinable> vif_combinables;
    std::set<uint16_t> vif_combinables_raw;

    // Match the storage nr. If no storage is specified, default to match only 0.
    bool match_storage_nr = true;
    StorageNr storage_nr_from{ 0 };
    StorageNr storage_nr_to{ 0 };

    // Match the tariff nr. If no tariff is specified, default to match only 0.
    bool match_tariff_nr = true;
    TariffNr tariff_nr_from{ 0 };
    TariffNr tariff_nr_to{ 0 };

    // Match the subunit. If no subunit is specified, default to match only 0.
    bool match_subunit_nr = true;
    SubUnitNr subunit_nr_from{ 0 };
    SubUnitNr subunit_nr_to{ 0 };

    // If the telegram has multiple identical difvif entries matching this field
    // and you want to catch the second matching entry, then set the index nr to 2.
    // The default is 1.
    IndexNr index_nr{ 1 };

    FieldMatcher() : active(false) { }
    FieldMatcher(bool act) : active(act) { }
    static FieldMatcher build() { return FieldMatcher(true); }
    static FieldMatcher noMatcher() { return FieldMatcher(false); }
    FieldMatcher& set(DifVifKey k) {
        dif_vif_key = k;
        match_dif_vif_key = (k.str() != ""); return *this;
    }
    FieldMatcher& set(MeasurementType mt) {
        measurement_type = mt;
        match_measurement_type = (mt != MeasurementType::Any);
        return *this;
    }
    FieldMatcher& set(VIFRange v) {
        vif_range = v;
        match_vif_range = (v != VIFRange::Any);
        return *this;
    }
    FieldMatcher& set(VIFRaw v) {
        vif_raw = v.value;
        match_vif_raw = true;
        return *this;
    }
    FieldMatcher& add(VIFCombinable v) {
        vif_combinables.insert(v);
        return *this;
    }
    FieldMatcher& add(VIFCombinableRaw v) {
        vif_combinables_raw.insert(v.value);
        return *this;
    }
    FieldMatcher& set(StorageNr s) {
        storage_nr_from = storage_nr_to = s;
        match_storage_nr = (s != AnyStorageNr);
        return *this;
    }
    FieldMatcher& set(StorageNr from, StorageNr to) {
        storage_nr_from = from;
        storage_nr_to = to;
        match_storage_nr = true;
        return *this;
    }
    FieldMatcher& set(TariffNr s) {
        tariff_nr_from = tariff_nr_to = s;
        match_tariff_nr = (s != AnyTariffNr);
        return *this;
    }
    FieldMatcher& set(TariffNr from, TariffNr to) {
        tariff_nr_from = from;
        tariff_nr_to = to;
        match_tariff_nr = true;
        return *this;
    }
    FieldMatcher& set(SubUnitNr s) {
        subunit_nr_from = subunit_nr_to = s;
        match_subunit_nr = true;
        return *this;
    }
    FieldMatcher& set(SubUnitNr from, SubUnitNr to) {
        subunit_nr_from = from;
        subunit_nr_to = to;
        match_subunit_nr = true; return *this;
    }

    FieldMatcher& set(IndexNr i) { index_nr = i; return *this; }

    bool matches(DVEntry& dv_entry);

    // Returns true of there is any range for storage, tariff, subunit nrs.
    // I.e. this matcher is expected to match against multiple dv entries!
    bool expectedToMatchAgainstMultipleEntries()
    {
        return (match_storage_nr && storage_nr_from != storage_nr_to)
            || (match_tariff_nr && tariff_nr_from != tariff_nr_to)
            || (match_subunit_nr && subunit_nr_from != subunit_nr_to);
    }

    std::string str();
};

bool loadFormatBytesFromSignature(uint16_t format_signature, std::vector<uchar>* format_bytes);
bool parseDV(Telegram* t,
    std::vector<uchar>& databytes,
    std::vector<uchar>::iterator data,
    size_t data_len,
    std::map<std::string, std::pair<int, DVEntry>>* dv_entries,
    std::vector<uchar>::iterator* format = NULL,
    size_t format_len = 0,
    uint16_t* format_hash = NULL);

// Instead of using a hardcoded difvif as key in the extractDV... below,
// find an existing difvif entry in the values based on the desired value information type.
// Like: Volume, VolumeFlow, FlowTemperature, ExternalTemperature etc
// in combination with the storagenr. (Later I will add tariff/subunit)
bool findKey(MeasurementType mt, VIFRange vi, StorageNr storagenr, TariffNr tariffnr,
    std::string* key, std::map<std::string, std::pair<int, DVEntry>>* values);
// Some meters have multiple identical DIF/VIF values! Meh, they are not using storage nrs or tariff nrs.
// So here we can pick for example nr 2 of an identical set if DIF/VIF values.
// Nr 1 means the first found value.
bool findKeyWithNr(MeasurementType mt, VIFRange vi, StorageNr storagenr, TariffNr tariffnr, int indexnr,
    std::string* key, std::map<std::string, std::pair<int, DVEntry>>* values);

bool hasKey(std::map<std::string, std::pair<int, DVEntry>>* values, std::string key);

bool extractDVuint8(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    uchar* value);

bool extractDVuint16(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    uint16_t* value);

bool extractDVuint24(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    uint32_t* value);

bool extractDVuint32(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    uint32_t* value);

// All values are scaled according to the vif and wmbusmeters scaling defaults.
bool extractDVdouble(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    double* value,
    bool auto_scale = true,
    bool force_unsigned = false);

// Extract a value without scaling. Works for 8bits to 64 bits, binary and bcd.
bool extractDVlong(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    uint64_t* value);

// Just copy the raw hex data into the string, not reversed or anything.
bool extractDVHexString(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    std::string* value);

// Read the content and attempt to reverse and transform it into a readble string
// based on the dif information.
bool extractDVReadableString(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    std::string* value);

bool extractDVdate(std::map<std::string, std::pair<int, DVEntry>>* values,
    std::string key,
    int* offset,
    struct tm* value);


const std::string& availableVIFRanges();

#endif

