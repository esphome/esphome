/*
 Copyright (C) 2019 Jacek Tomasiak (gpl-3.0-or-later)
 Copyright (C) 2021 Vincent Privat (gpl-3.0-or-later)

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

#include<cstring>
#include<set>
#include"manufacturers.h"
#include"manufacturer_specificities.h"
#include"meters.h"

std::set<int> diehl_manufacturers = {
    MANUFACTURER_DME,
    MANUFACTURER_EWT,
    MANUFACTURER_HYD,
    MANUFACTURER_SAP,
    MANUFACTURER_SPL
};

// Default keys for Izar/PRIOS and Sharky meters
#define PRIOS_DEFAULT_KEY1 "39BC8A10E66D83F8"
#define PRIOS_DEFAULT_KEY2 "51728910E66D83F8"

// Diehl: Is "A field" coded differently from standard?
DiehlAddressTransformMethod mustTransformDiehlAddress(DiehlFrameInterpretation interpretation)
{
    switch (interpretation) {
    case DiehlFrameInterpretation::PRIOS:
    case DiehlFrameInterpretation::PRIOS_SCR:
    case DiehlFrameInterpretation::REAL_DATA:
        return DiehlAddressTransformMethod::SWAPPING;

    case DiehlFrameInterpretation::SAP_PRIOS:
        return DiehlAddressTransformMethod::SAP_PRIOS;

    case DiehlFrameInterpretation::SAP_PRIOS_STD:
        return DiehlAddressTransformMethod::SAP_PRIOS_STANDARD;

    case DiehlFrameInterpretation::RESERVED:
    case DiehlFrameInterpretation::OMS:
    case DiehlFrameInterpretation::NA:
    default:
        return DiehlAddressTransformMethod::NONE;
    }
}

// Diehl: Determines how to interpret frame
DiehlFrameInterpretation detectDiehlFrameInterpretation(uchar c_field, int m_field, uchar ci_field, int tpl_cfg)
{
    if (diehl_manufacturers.find(m_field) != diehl_manufacturers.end())
    {
        if (c_field == 0x44 || c_field == 0x46) // SND_NR / SND_IR
        {
            switch (ci_field) {
            case 0x71: // Alarm
                return DiehlFrameInterpretation::REAL_DATA;
            case 0x7A: // EN 13757-3 Application Layer (short tplh)
                if (((tpl_cfg >> 8) & 0x10) == 0x10) // Bit 12 from MMMMM bits of CFG field
                {
                    return DiehlFrameInterpretation::REAL_DATA;
                }
                return DiehlFrameInterpretation::OMS;
            case 0xA0: // Manufacturer specific
            case 0xA1: // Manufacturer specific
            case 0xA2: // Manufacturer specific
            case 0xA3: // Manufacturer specific
            case 0xA4: // Manufacturer specific
            case 0xA5: // Manufacturer specific
            case 0xA6: // Manufacturer specific
            case 0xA7: // Manufacturer specific
                if (m_field == MANUFACTURER_SAP)
                {
                    return DiehlFrameInterpretation::SAP_PRIOS;
                }
                return DiehlFrameInterpretation::PRIOS;
            case 0xB0: // Manufacturer specific
                if (m_field == MANUFACTURER_SAP)
                {
                    return DiehlFrameInterpretation::SAP_PRIOS_STD;
                }
                return DiehlFrameInterpretation::RESERVED;
            case 0xA8: // Manufacturer specific
            case 0xA9: // Manufacturer specific
            case 0xAA: // Manufacturer specific
            case 0xAB: // Manufacturer specific
            case 0xAC: // Manufacturer specific
            case 0xAD: // Manufacturer specific
            case 0xAE: // Manufacturer specific
            case 0xAF: // Manufacturer specific
            case 0xB4: // Manufacturer specific
            case 0xB5: // Manufacturer specific
            case 0xB6: // Manufacturer specific
            case 0xB7: // Manufacturer specific
                return DiehlFrameInterpretation::RESERVED;
            case 0xB1: // Manufacturer specific
            case 0xB2: // Manufacturer specific
            case 0xB3: // Manufacturer specific
                return DiehlFrameInterpretation::PRIOS_SCR;
            default:
                return DiehlFrameInterpretation::OMS;
            }
        }
    }
    return DiehlFrameInterpretation::NA;
}

// Diehl: Determines how to interpret frame
DiehlFrameInterpretation detectDiehlFrameInterpretation(const vector<uchar>& frame)
{
    if (frame.size() < 15)
        return DiehlFrameInterpretation::NA;

    uchar c_field = frame[1];
    int m_field = frame[3] << 8 | frame[2];
    uchar ci_field = frame[10];
    int tpl_cfg = frame[14] << 8 | frame[13];
    return detectDiehlFrameInterpretation(c_field, m_field, ci_field, tpl_cfg);
}

// Diehl: Is "A field" coded differently from standard?
DiehlAddressTransformMethod mustTransformDiehlAddress(const vector<uchar>& frame)
{
    return mustTransformDiehlAddress(detectDiehlFrameInterpretation(frame));
}

// Diehl: transform "A field" to make it compliant to standard
void transformDiehlAddress(vector<uchar>& frame, DiehlAddressTransformMethod transform_method)
{
    if (transform_method == DiehlAddressTransformMethod::SWAPPING)
    {
        debug("(diehl) Pre-processing: swapping address field");
        uchar version = frame[4];
        uchar type = frame[5];
        for (int i = 4; i < 8; i++)
        {
            frame[i] = frame[i + 2];
        }
        frame[8] = version;
        frame[9] = type;
    }
    else if (transform_method == DiehlAddressTransformMethod::SAP_PRIOS)
    {
        debug("(diehl) Pre-processing: setting device type to water meter for SAP PRIOS");
        frame[8] = 0x00; // version field is used by IZAR as part of meter id on 5 bytes instead of 4
        frame[9] = 0x07; // water meter
    }
    else if (transform_method == DiehlAddressTransformMethod::SAP_PRIOS_STANDARD)
    {
        warning("(diehl) Pre-processing: SAP PRIOS STANDARD transformation not implemented!"); // TODO
    }
}

// Diehl: decode LFSR encrypted data used in Izar/PRIOS and Sharky meters
vector<uchar> decodeDiehlLfsr(const vector<uchar>& origin, const vector<uchar>& frame, uint32_t key, DiehlLfsrCheckMethod check_method, uint32_t check_value)
{
    // modify seed key with header values
    key ^= uint32FromBytes(origin, 2); // manufacturer + address[0-1]
    key ^= uint32FromBytes(origin, 6); // address[2-3] + version + type
    key ^= uint32FromBytes(frame, 10); // ci + some more bytes from the telegram...

    int size = frame.size() - 15;
    vector<uchar> decoded(size);

    for (int i = 0; i < size; ++i) {
        // calculate new key (LFSR)
        // https://en.wikipedia.org/wiki/Linear-feedback_shift_register
        for (int j = 0; j < 8; ++j) {
            // calculate new bit value (xor of selected bits from previous key)
            uchar bit = ((key & 0x2) != 0) ^ ((key & 0x4) != 0) ^ ((key & 0x800) != 0) ^ ((key & 0x80000000) != 0);
            // shift key bits and add new one at the end
            key = (key << 1) | bit;
        }
        // decode i-th content byte with fresh/last 8-bits of key
        decoded[i] = frame[i + 15] ^ (key & 0xFF);

        if (check_method == DiehlLfsrCheckMethod::HEADER_1_BYTE)
        {
            // check-byte doesn't match?
            if (decoded[0] != 0x4B) {
                decoded.clear();
                return decoded;
            }
        }
        else if (check_method == DiehlLfsrCheckMethod::CHECKSUM_AND_0XEF)
        {
            if (i == size - 1) {
                uint32_t checksum = 0;
                for (int index = 0; index < size; index++) {
                    checksum += (decoded[index] & 0xFF);
                }
                if ((checksum & 0xEF) != check_value) {
                    decoded.clear();
                    return decoded;
                }
            }
        }
    }

    return decoded;
}

uint32_t uint32FromBytes(const vector<uchar>& data, int offset, bool reverse)
{
    if (reverse)
        return ((uint32_t)data[offset + 3] << 24) |
        ((uint32_t)data[offset + 2] << 16) |
        ((uint32_t)data[offset + 1] << 8) |
        (uint32_t)data[offset];
    else
        return ((uint32_t)data[offset] << 24) |
        ((uint32_t)data[offset + 1] << 16) |
        ((uint32_t)data[offset + 2] << 8) |
        (uint32_t)data[offset + 3];
}

uint32_t convertKey(const vector<uchar>& bytes)
{
    uint32_t key1 = uint32FromBytes(bytes, 0);
    uint32_t key2 = uint32FromBytes(bytes, 4);
    uint32_t key = key1 ^ key2;
    return key;
}

uint32_t convertKey(const char* hex)
{
    vector<uchar> bytes;
    hex2bin(hex, &bytes);
    return convertKey(bytes);
}

// Common: add default manufacturers key if none specified and we know one for the given frame
void addDefaultManufacturerKeyIfAny(const vector<uchar>& frame, TPLSecurityMode tpl_sec_mode, MeterKeys* meter_keys)
{
    if (!meter_keys->hasConfidentialityKey()
        && tpl_sec_mode == TPLSecurityMode::AES_CBC_IV
        && detectDiehlFrameInterpretation(frame) == DiehlFrameInterpretation::OMS)
    {
        vector<uchar> half;
        hex2bin(PRIOS_DEFAULT_KEY2, &half);
        meter_keys->confidentiality_key = vector<uchar>(half.begin(), half.end());
        meter_keys->confidentiality_key.insert(meter_keys->confidentiality_key.end(), half.begin(), half.end());
        debug("(mfct) added default key");
    }
}

// Diehl: initialize support of default keys in a meter
void initializeDiehlDefaultKeySupport(const vector<uchar>& confidentiality_key, vector<uint32_t>& keys)
{
    if (!confidentiality_key.empty())
        keys.push_back(convertKey(confidentiality_key));

    // fallback to default keys if no custom key provided
    if (keys.empty())
    {
        keys.push_back(convertKey(PRIOS_DEFAULT_KEY1));
        keys.push_back(convertKey(PRIOS_DEFAULT_KEY2));
    }
}

// Diehl: Is payload real data crypted (LFSR)?
bool mustDecryptDiehlRealData(const vector<uchar>& frame)
{
    DiehlFrameInterpretation fi = detectDiehlFrameInterpretation(frame);
    debug("(diehl) frame %s", toString(fi));
    return fi == DiehlFrameInterpretation::REAL_DATA;
}

// Diehl: decrypt real data payload (LFSR)
bool decryptDielhRealData(Telegram* t, vector<uchar>& frame, vector<uchar>::iterator& pos, const vector<uchar>& confidentiality_key)
{
    vector<uint32_t> keys;
    initializeDiehlDefaultKeySupport(confidentiality_key, keys);

    vector<uchar> decoded_content;
    for (auto& key : keys) {
        decoded_content = decodeDiehlLfsr(t->original.empty() ? frame : t->original, frame, key, DiehlLfsrCheckMethod::CHECKSUM_AND_0XEF, frame[14] & 0xEF);
        if (!decoded_content.empty())
            break;
    }

    if (decoded_content.empty())
    {
        if (!t->isSimulated() && t->parserWarns())
        {
            // Do not warn when decryption is not needed since we are running from a simulated file.
            // Or if the telegram cannot be decrypted when listening to all.
            warning("(diehl) Decoding LFSR real data failed.");
        }
        else
        {
            if (t->isSimulated())
            {
                debug("(diehl) Decoding LFSR real data failed, probably already decoded since telegram is simulated.");
            }
            else
            {
                debug("(diehl) Decoding LFSR real data failed.");
            }
        }
        return false;
    }

    debug("(diehl) Decoded LFSR real data: %s", bin2hex(decoded_content).c_str());

    frame.erase(pos, frame.end());
    frame.insert(frame.end(), decoded_content.begin(), decoded_content.end());

    return true;
}

const char* toString(DiehlFrameInterpretation i)
{
    switch (i)
    {
    case DiehlFrameInterpretation::NA: return "N/A";
    case DiehlFrameInterpretation::REAL_DATA: return "REAL_DATA";
    case DiehlFrameInterpretation::OMS: return "OMS";
    case DiehlFrameInterpretation::PRIOS: return "PRIOS";
    case DiehlFrameInterpretation::SAP_PRIOS: return "SAP_PRIOS";
    case DiehlFrameInterpretation::SAP_PRIOS_STD: return "SAP_PRIOS_STD";
    case DiehlFrameInterpretation::PRIOS_SCR: return "PRIOS_SCR";
    case DiehlFrameInterpretation::RESERVED: return "RESERVED";
    }
    return "?";
}

const char* toString(DiehlAddressTransformMethod m)
{
    switch (m)
    {
    case DiehlAddressTransformMethod::NONE: return "NONE";
    case DiehlAddressTransformMethod::SWAPPING: return "SWAPPING";
    case DiehlAddressTransformMethod::SAP_PRIOS: return "SAP_PRIOS";
    case DiehlAddressTransformMethod::SAP_PRIOS_STANDARD: return "SAP_PRIOS_STANDARD";
    }
    return "?";
}

void qdsExtractWalkByField(Telegram* t, Meter* driver, DVEntry& mfctEntry, int pos, int n, const string& key_s, const string& fieldName, Quantity quantity) {
    string bytes = mfctEntry.value.substr(pos, n);

    DifVifKey key(key_s);
    DVEntry fieldEntry(0,
        key,
        MeasurementType::Instantaneous,
        key.vif(),
        set<VIFCombinable>(),
        set<uint16_t>(),
        AnyStorageNr,
        AnyTariffNr,
        SubUnitNr(0),
        bytes);

    FieldInfo* fieldInfo = driver->findFieldInfo(fieldName, quantity);
    if (fieldInfo == nullptr) {
        error("(qds) field info not found: %s", fieldName.c_str());
    }

    fieldInfo->performExtraction(driver, t, &fieldEntry);
    string info = "*** " + bytes + " (" + fieldInfo->renderJson(driver, &fieldEntry) + ")";

    t->addSpecialExplanation(mfctEntry.offset + pos / 2, n / 2, KindOfData::CONTENT, Understanding::FULL, info.c_str());
}
