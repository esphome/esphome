/*
 Copyright (C) 2017-2023 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"dvparser.h"
#include"manufacturer_specificities.h"
#include<assert.h>
#include<cmath>
#include<stdarg.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/types.h>

#include<deque>
#include<algorithm>
#include "manufacturers.h"
#include "Telegram.h"
#include "aes.h"
#include "types.h"

uchar vec87[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};


void xorit(uchar* srca, uchar* srcb, uchar* dest, int len)
{
    for (int i = 0; i < len; ++i) { dest[i] = srca[i] ^ srcb[i]; }
}

void shiftLeft(uchar* srca, uchar* srcb, int len)
{
    uchar overflow = 0;

    for (int i = len - 1; i >= 0; i--)
    {
        srcb[i] = srca[i] << 1;
        srcb[i] |= overflow;
        overflow = (srca[i] & 0x80) >> 7;
    }
    return;
}

void generateSubkeys(uchar* key, uchar* K1, uchar* K2)
{
    uchar L[16];
    uchar Z[16];
    uchar tmp[16];

    memset(Z, 0, 16);

    AES_ECB_encrypt(Z, key, L, 16);

    if (!(L[0] & 0x80))
    {
        shiftLeft(L, K1, 16);
    }
    else
    {
        shiftLeft(L, tmp, 16);
        xorit(tmp, vec87, K1, 16);
    }

    if (!(K1[0] & 0x80))
    {
        shiftLeft(K1, K2, 16);
    }
    else
    {
        shiftLeft(K1, tmp, 16);
        xorit(tmp, vec87, K2, 16);
    }
}

void pad(uchar* in, uchar* out, int len)
{
    for (int i = 0; i < 16; i++)
    {
        if (i < len)
        {
            out[i] = in[i];
        }
        else if (i == len)
        {
            out[i] = 0x80;
        }
        else
        {
            out[i] = 0x00;
        }
    }
}

void AES_CMAC(uchar* key, uchar* input, int len, uchar* mac)
{
    bool len_is_multiple_of_block;
    uchar X[16], Y[16];
    uchar K1[16], K2[16];
    uchar M_last[16], padded[16];

    generateSubkeys(key, K1, K2);

    int num_blocks = (len + 15) / 16;

    if (!num_blocks)
    {
        num_blocks = 1;
        len_is_multiple_of_block = false;
    }
    else
    {
        len_is_multiple_of_block = !(len % 16);
    }

    if (len_is_multiple_of_block)
    {
        xorit(input + (16 * (num_blocks - 1)), K1, M_last, 16);
    }
    else
    {
        pad(input + (16 * (num_blocks - 1)), padded, len % 16);
        xorit(padded, K2, M_last, 16);
    }

    memset(X, 0, 16);

    for (int i = 0; i < num_blocks - 1; i++)
    {
        xorit(X, input + (16 * i), Y, 16);
        AES_ECB_encrypt(Y, key, X, 16);
    }

    xorit(X, M_last, Y, 16);
    AES_ECB_encrypt(Y, key, X, 16);

    memcpy(mac, X, 16);
}


void incrementIV(uchar* iv, size_t len) {
    uchar* p = iv + len - 1;
    while (p >= iv) {
        int pp = *p;
        (*p)++;
        if (pp + 1 <= 255) {
            // Nice, no overflow. We are done here!
            break;
        }
        // Move left add add one.
        p--;
    }
}

AFLAuthenticationType fromIntToAFLAuthenticationType(int i)
{
    switch (i) {
#define X(name,nr,len) case nr: return AFLAuthenticationType::name;
        LIST_OF_AFL_AUTH_TYPES
#undef X
    }

    return AFLAuthenticationType::Reserved1;
}

TPLSecurityMode fromIntToTPLSecurityMode(int i)
{
    switch (i) {

#define X(name,nr) case nr: return TPLSecurityMode::name;
        LIST_OF_TPL_SECURITY_MODES
#undef X
    }

    return TPLSecurityMode::SPECIFIC_16_31;
}

bool decrypt_ELL_AES_CTR(Telegram* t, vector<uchar>& frame, vector<uchar>::iterator& pos, vector<uchar>& aeskey)
{
    if (aeskey.size() == 0) return true;

    vector<uchar> encrypted_bytes;
    vector<uchar> decrypted_bytes;
    encrypted_bytes.insert(encrypted_bytes.end(), pos, frame.end());
    debug("(ELL) decrypting", encrypted_bytes);

    uchar iv[16];
    int i = 0;
    // M-field
    iv[i++] = t->dll_mfct_b[0]; iv[i++] = t->dll_mfct_b[1];
    // A-field
    for (int j = 0; j < 6; ++j) { iv[i++] = t->dll_a[j]; }
    // CC-field
    iv[i++] = t->ell_cc;
    // SN-field
    for (int j = 0; j < 4; ++j) { iv[i++] = t->ell_sn_b[j]; }
    // FN
    iv[i++] = 0; iv[i++] = 0;
    // BC
    iv[i++] = 0;

    vector<uchar> ivv(iv, iv + 16);
    string s = bin2hex(ivv);
    debug("(ELL) IV %s", s.c_str());

    int block = 0;
    for (size_t offset = 0; offset < encrypted_bytes.size(); offset += 16)
    {
        size_t block_size = 16;
        if (offset + block_size > encrypted_bytes.size())
        {
            block_size = encrypted_bytes.size() - offset;
        }

        assert(block_size > 0 && block_size <= 16);

        // Generate the pseudo-random bits from the IV and the key.
        uchar xordata[16];
        AES_ECB_encrypt(iv, safeButUnsafeVectorPtr(aeskey), xordata, 16);

        // Xor the data with the pseudo-random bits to decrypt into tmp.
        uchar tmp[16];
        xorit(xordata, &encrypted_bytes[offset], tmp, block_size);

        debug("(ELL) block %d block_size %d offset %zu", block, block_size, offset);
        block++;

        vector<uchar> tmpv(tmp, tmp + block_size);
        debug("(ELL) decrypted", tmpv);

        decrypted_bytes.insert(decrypted_bytes.end(), tmpv.begin(), tmpv.end());

        incrementIV(iv, sizeof(iv));
    }
    debug("(ELL) decrypted", decrypted_bytes);

    // Remove the encrypted bytes.
    frame.erase(pos, frame.end());
    // Insert the decrypted bytes.
    frame.insert(frame.end(), decrypted_bytes.begin(), decrypted_bytes.end());

    return true;
}

string frameTypeKamstrupC1(int ft) {
    if (ft == 0x78) return "long frame";
    if (ft == 0x79) return "short frame";
    return "?";
}

bool decrypt_TPL_AES_CBC_IV(Telegram* t,
    vector<uchar>& frame,
    vector<uchar>::iterator& pos,
    vector<uchar>& aeskey,
    int* num_encrypted_bytes,
    int* num_not_encrypted_at_end)
{
    vector<uchar> buffer;
    buffer.insert(buffer.end(), pos, frame.end());

    size_t num_bytes_to_decrypt = frame.end() - pos;

    if (t->tpl_num_encr_blocks)
    {
        num_bytes_to_decrypt = t->tpl_num_encr_blocks * 16;
    }

    *num_encrypted_bytes = num_bytes_to_decrypt;

    if (buffer.size() < num_bytes_to_decrypt)
    {
        warning("(TPL) warning: aes-cbc-iv decryption received less bytes than expected for decryption! "
            "Got %zu bytes but expected at least %zu bytes since num encr blocks was %d.",
            buffer.size(), num_bytes_to_decrypt,
            t->tpl_num_encr_blocks);
        num_bytes_to_decrypt = buffer.size();
        *num_encrypted_bytes = num_bytes_to_decrypt;

        // We must have at least 16 bytes to decrypt. Give up otherwise.
        if (num_bytes_to_decrypt < 16) return false;
    }

    *num_not_encrypted_at_end = buffer.size() - num_bytes_to_decrypt;

    debug("(TPL) num encrypted blocks %zu (%d bytes and remaining unencrypted %zu bytes)",
        t->tpl_num_encr_blocks, num_bytes_to_decrypt, buffer.size() - num_bytes_to_decrypt);

    if (aeskey.size() == 0) return false;

    debug("(TPL) AES CBC IV decrypting", buffer);

    // The content should be a multiple of 16 since we are using AES CBC mode.
    if (num_bytes_to_decrypt % 16 != 0)
    {
        warning("(TPL) warning: decryption received non-multiple of 16 bytes! "
            "Got %zu bytes shrinking message to %zu bytes.",
            num_bytes_to_decrypt, num_bytes_to_decrypt - num_bytes_to_decrypt % 16);
        num_bytes_to_decrypt -= num_bytes_to_decrypt % 16;
        *num_encrypted_bytes = num_bytes_to_decrypt;
        assert(num_bytes_to_decrypt % 16 == 0);
        // There must be at least 16 bytes remaining.
        if (num_bytes_to_decrypt < 16) return false;
    }

    uchar iv[16];
    int i = 0;
    // If there is a tpl_id, then use it, else use ddl_id.
    if (t->tpl_id_found)
    {
        // M-field
        iv[i++] = t->tpl_mfct_b[0]; iv[i++] = t->tpl_mfct_b[1];

        // A-field
        for (int j = 0; j < 6; ++j) { iv[i++] = t->tpl_a[j]; }
    }
    else
    {
        // M-field
        iv[i++] = t->dll_mfct_b[0]; iv[i++] = t->dll_mfct_b[1];

        // A-field
        for (int j = 0; j < 6; ++j) { iv[i++] = t->dll_a[j]; }
    }

    // ACC
    for (int j = 0; j < 8; ++j) { iv[i++] = t->tpl_acc; }

    vector<uchar> ivv(iv, iv + 16);
    string s = bin2hex(ivv);
    debug("(TPL) IV %s", s.c_str());

    uchar buffer_data[1000];
    memcpy(buffer_data, safeButUnsafeVectorPtr(buffer), num_bytes_to_decrypt);
    uchar decrypted_data[1000];

    AES_CBC_decrypt_buffer(decrypted_data, buffer_data, num_bytes_to_decrypt, safeButUnsafeVectorPtr(aeskey), iv);

    // Remove the encrypted bytes.
    frame.erase(pos, frame.end());

    // Insert the decrypted bytes.
    frame.insert(frame.end(), decrypted_data, decrypted_data + num_bytes_to_decrypt);

    debugPayload("(TPL) decrypted ", frame, pos);

    if (num_bytes_to_decrypt < buffer.size())
    {
        frame.insert(frame.end(), buffer.begin() + num_bytes_to_decrypt, buffer.end());
        debugPayload("(TPL) appended  ", frame, pos);
    }
    return true;
}

bool decrypt_TPL_AES_CBC_NO_IV(Telegram* t, vector<uchar>& frame, vector<uchar>::iterator& pos, vector<uchar>& aeskey,
    int* num_encrypted_bytes,
    int* num_not_encrypted_at_end)
{
    if (aeskey.size() == 0) return true;

    vector<uchar> buffer;
    buffer.insert(buffer.end(), pos, frame.end());

    size_t num_bytes_to_decrypt = buffer.size();

    if (t->tpl_num_encr_blocks)
    {
        num_bytes_to_decrypt = t->tpl_num_encr_blocks * 16;
    }

    *num_encrypted_bytes = num_bytes_to_decrypt;
    if (buffer.size() < num_bytes_to_decrypt)
    {
        warning("(TPL) warning: aes-cbc-no-iv decryption received less bytes than expected for decryption! "
            "Got %zu bytes but expected at least %zu bytes since num encr blocks was %d.",
            buffer.size(), num_bytes_to_decrypt,
            t->tpl_num_encr_blocks);
        num_bytes_to_decrypt = buffer.size();
    }

    *num_not_encrypted_at_end = buffer.size() - num_bytes_to_decrypt;

    debug("(TPL) num encrypted blocks %d (%d bytes and remaining unencrypted %d bytes)",
        t->tpl_num_encr_blocks, num_bytes_to_decrypt, buffer.size() - num_bytes_to_decrypt);

    if (aeskey.size() == 0) return false;

    // The content should be a multiple of 16 since we are using AES CBC mode.
    if (num_bytes_to_decrypt % 16 != 0)
    {
        warning("(TPL) warning: decryption received non-multiple of 16 bytes! "
            "Got %zu bytes shrinking message to %zu bytes.",
            num_bytes_to_decrypt, num_bytes_to_decrypt - num_bytes_to_decrypt % 16);
        num_bytes_to_decrypt -= num_bytes_to_decrypt % 16;
        assert(num_bytes_to_decrypt % 16 == 0);
    }

    uchar iv[16];
    memset(iv, 0, sizeof(iv));

    vector<uchar> ivv(iv, iv + 16);
    string s = bin2hex(ivv);
    debug("(TPL) IV %s", s.c_str());

    uchar buffer_data[1000];
    memcpy(buffer_data, safeButUnsafeVectorPtr(buffer), num_bytes_to_decrypt);
    uchar decrypted_data[1000];

    AES_CBC_decrypt_buffer(decrypted_data, buffer_data, num_bytes_to_decrypt, safeButUnsafeVectorPtr(aeskey), iv);

    // Remove the encrypted bytes and any potentially not decryptes bytes after.
    frame.erase(pos, frame.end());

    // Insert the decrypted bytes.
    frame.insert(frame.end(), decrypted_data, decrypted_data + num_bytes_to_decrypt);

    debugPayload("(TPL) decrypted ", frame, pos);

    if (num_bytes_to_decrypt < buffer.size())
    {
        frame.insert(frame.end(), buffer.begin() + num_bytes_to_decrypt, buffer.end());
        debugPayload("(TPL) appended ", frame, pos);
    }

    return true;
}

struct Manufacturer {
    const char* code;
    int m_field;
    const char* name;

    Manufacturer(const char* c, int m, const char* n) {
        code = c;
        m_field = m;
        name = n;
    }
};

vector<Manufacturer> manufacturers_;

struct Initializer { Initializer(); };

static Initializer initializser_;

Initializer::Initializer() {

#define X(key,code,name) manufacturers_.push_back(Manufacturer(#key,code,name));
    LIST_OF_MANUFACTURERS
#undef X

}

void Telegram::addAddressMfctFirst(const std::vector<uchar>::iterator &pos)
{
    Address a;
    a.decodeMfctFirst(pos);
    addresses.push_back(a);
}

void Telegram::addAddressIdFirst(const std::vector<uchar>::iterator &pos)
{
    Address a;
    a.decodeIdFirst(pos);
    addresses.push_back(a);
}

void Telegram::print()
{
    uchar a = 0, b = 0, c = 0, d = 0;
    if (dll_id.size() >= 4)
    {
        a = dll_id[0];
        b = dll_id[1];
        c = dll_id[2];
        d = dll_id[3];
    }
    const char* enc = "";

    if (ell_sec_mode != ELLSecurityMode::NoSecurity ||
        tpl_sec_mode != TPLSecurityMode::NoSecurity)
    {
        enc = " encrypted";
    }

    debug("Received telegram from: %02x%02x%02x%02x", a, b, c, d);
    debug("          manufacturer: (%s) %s (0x%02x)",
        manufacturerFlag(dll_mfct).c_str(),
        manufacturer(dll_mfct).c_str(),
        dll_mfct);
    debug("                  type: %s (0x%02x)%s", mediaType(dll_type, dll_mfct).c_str(), dll_type, enc);

    debug("                   ver: 0x%02x", dll_version);

    if (tpl_id_found)
    {
        debug("      Concerning meter: %02x%02x%02x%02x", tpl_id_b[3], tpl_id_b[2], tpl_id_b[1], tpl_id_b[0]);
        debug("          manufacturer: (%s) %s (0x%02x)",
            manufacturerFlag(tpl_mfct).c_str(),
            manufacturer(tpl_mfct).c_str(),
            tpl_mfct);
        debug("                  type: %s (0x%02x)%s", mediaType(tpl_type, dll_mfct).c_str(), tpl_type, enc);

        debug("                   ver: 0x%02x", tpl_version);
    }
    if (about.device != "")
    {
        debug("                device: %s", about.device.c_str());
        debug("                  rssi: %d dBm", about.rssi_dbm);
    }
    string possible_drivers = autoDetectPossibleDrivers();
    debug("                driver: %s", possible_drivers.c_str());
}

void Telegram::printDLL()
{
    if (about.type == FrameType::WMBUS)
    {
        string possible_drivers = autoDetectPossibleDrivers();

        string man = manufacturerFlag(dll_mfct);
        debug("(telegram) DLL L=%02x C=%02x (%s) M=%04x (%s) A=%02x%02x%02x%02x VER=%02x TYPE=%02x (%s) (driver %s) DEV=%s RSSI=%d",
            dll_len,
            dll_c, cType(dll_c).c_str(),
            dll_mfct,
            man.c_str(),
            dll_id[0], dll_id[1], dll_id[2], dll_id[3],
            dll_version,
            dll_type,
            mediaType(dll_type, dll_mfct).c_str(),
            possible_drivers.c_str(),
            about.device.c_str(),
            about.rssi_dbm);
    }

    if (about.type == FrameType::MBUS)
    {
        verbose("(telegram) DLL L=%02x C=%02x (%s) A=%02x",
            dll_len,
            dll_c, cType(dll_c).c_str(),
            mbus_primary_address);
    }

}

void Telegram::printELL()
{
    if (ell_ci == 0) return;

    string ell_cc_info = ccType(ell_cc);
    verbose("(telegram) ELL CI=%02x CC=%02x (%s) ACC=%02x",
        ell_ci, ell_cc, ell_cc_info.c_str(), ell_acc);

    if (ell_ci == 0x8d || ell_ci == 0x8f)
    {
        string ell_sn_info = toStringFromELLSN(ell_sn);

        verbose(" SN=%02x%02x%02x%02x (%s) CRC=%02x%02x",
            ell_sn_b[0], ell_sn_b[1], ell_sn_b[2], ell_sn_b[3], ell_sn_info.c_str(),
            ell_pl_crc_b[0], ell_pl_crc_b[1]);
    }
    if (ell_ci == 0x8e || ell_ci == 0x8f)
    {
        string man = manufacturerFlag(ell_mfct);
        verbose(" M=%02x%02x (%s) ID=%02x%02x%02x%02x",
            ell_mfct_b[0], ell_mfct_b[1], man.c_str(),
            ell_id_b[0], ell_id_b[1], ell_id_b[2], ell_id_b[3]);
    }
}

void Telegram::printNWL()
{
    if (nwl_ci == 0) return;

    verbose("(telegram) NWL CI=%02x",
        nwl_ci);
}

void Telegram::printAFL()
{
    if (afl_ci == 0) return;

    verbose("(telegram) AFL CI=%02x",
        afl_ci);

}


string toStringFromTPLConfig(int cfg)
{
    string info = "";
    if (cfg & 0x8000) info += "bidirectional ";
    if (cfg & 0x4000) info += "accessibility ";
    if (cfg & 0x2000) info += "synchronous ";
    if (cfg & 0x1f00)
    {
        int m = (cfg >> 8) & 0x1f;
        TPLSecurityMode tsm = fromIntToTPLSecurityMode(m);
        info += toString(tsm);
        info += " ";
        if (tsm == TPLSecurityMode::AES_CBC_IV)
        {
            int num_blocks = (cfg & 0x00f0) >> 4;
            int cntn = (cfg & 0x000c) >> 2;
            int ra = (cfg & 0x0002) >> 1;
            int hc = cfg & 0x0001;
            info += "nb=" + std::to_string(num_blocks);
            info += " cntn=" + std::to_string(cntn);
            info += " ra=" + std::to_string(ra);
            info += " hc=" + std::to_string(hc);
            info += " ";
        }
    }
    if (info.length() > 0) info.pop_back();
    return info;
}

void Telegram::printTPL()
{
    if (tpl_ci == 0) return;

    debug("(telegram) TPL CI=%02x", tpl_ci);

    if (tpl_ci == 0x7a || tpl_ci == 0x72)
    {
        string tpl_cfg_info = toStringFromTPLConfig(tpl_cfg);
        debug(" ACC=%02x STS=%02x CFG=%04x (%s)",
            tpl_acc, tpl_sts, tpl_cfg, tpl_cfg_info.c_str());
    }

    if (tpl_ci == 0x72)
    {
        string info = mediaType(tpl_type, tpl_mfct);
        debug(" ID=%02x%02x%02x%02x MFT=%02x%02x VER=%02x TYPE=%02x (%s)",
            tpl_id_b[0], tpl_id_b[1], tpl_id_b[2], tpl_id_b[3],
            tpl_mfct_b[0], tpl_mfct_b[1],
            tpl_version, tpl_type, info.c_str());
    }
}

string manufacturer(int m_field) {
    for (auto& m : manufacturers_) {
        if (m.m_field == m_field) return m.name;
    }
    return "Unknown";
}

string mediaType(int a_field_device_type, int m_field) {
    switch (a_field_device_type) {
    case 0: return "Other";
    case 1: return "Oil meter";
    case 2: return "Electricity meter";
    case 3: return "Gas meter";
    case 4: return "Heat meter";
    case 5: return "Steam meter";
    case 6: return "Warm Water (30°C-90°C) meter";
    case 7: return "Water meter";
    case 8: return "Heat Cost Allocator";
    case 9: return "Compressed air meter";
    case 0x0a: return "Cooling load volume at outlet meter";
    case 0x0b: return "Cooling load volume at inlet meter";
    case 0x0c: return "Heat volume at inlet meter";
    case 0x0d: return "Heat/Cooling load meter";
    case 0x0e: return "Bus/System component";
    case 0x0f: return "Unknown";
    case 0x15: return "Hot water (>=90°C) meter";
    case 0x16: return "Cold water meter";
    case 0x17: return "Hot/Cold water meter";
    case 0x18: return "Pressure meter";
    case 0x19: return "A/D converter";
    case 0x1A: return "Smoke detector";
    case 0x1B: return "Room sensor (eg temperature or humidity)";
    case 0x1C: return "Gas detector";
    case 0x1D: return "Reserved for sensors";
    case 0x1F: return "Reserved for sensors";
    case 0x20: return "Breaker (electricity)";
    case 0x21: return "Valve (gas or water)";
    case 0x22: return "Reserved for switching devices";
    case 0x23: return "Reserved for switching devices";
    case 0x24: return "Reserved for switching devices";
    case 0x25: return "Customer unit (display device)";
    case 0x26: return "Reserved for customer units";
    case 0x27: return "Reserved for customer units";
    case 0x28: return "Waste water";
    case 0x29: return "Garbage";
    case 0x2A: return "Reserved for Carbon dioxide";
    case 0x2B: return "Reserved for environmental meter";
    case 0x2C: return "Reserved for environmental meter";
    case 0x2D: return "Reserved for environmental meter";
    case 0x2E: return "Reserved for environmental meter";
    case 0x2F: return "Reserved for environmental meter";
    case 0x30: return "Reserved for system devices";
    case 0x31: return "Reserved for communication controller";
    case 0x32: return "Reserved for unidirectional repeater";
    case 0x33: return "Reserved for bidirectional repeater";
    case 0x34: return "Reserved for system devices";
    case 0x35: return "Reserved for system devices";
    case 0x36: return "Radio converter (system side)";
    case 0x37: return "Radio converter (meter side)";
    case 0x38: return "Reserved for system devices";
    case 0x39: return "Reserved for system devices";
    case 0x3A: return "Reserved for system devices";
    case 0x3B: return "Reserved for system devices";
    case 0x3C: return "Reserved for system devices";
    case 0x3D: return "Reserved for system devices";
    case 0x3E: return "Reserved for system devices";
    case 0x3F: return "Reserved for system devices";
    }

    if (m_field == MANUFACTURER_TCH)
    {
        switch (a_field_device_type) {
            // Techem MK Radio 3/4 manufacturer specific.
        case 0x62: return "Warm water"; // MKRadio3/MKRadio4
        case 0x72: return "Cold water"; // MKRadio3/MKRadio4
            // Techem FHKV.
        case 0x80: return "Heat Cost Allocator"; // FHKV data ii/iii
            // Techem Vario 4 Typ 4.5.1 manufacturer specific.
        case 0xC3: return "Heat meter";
            // Techem V manufacturer specific.
        case 0x43: return "Heat meter";
        case 0xf0: return "Smoke detector";
        }
    }
    return "Unknown";
}

string mediaTypeJSON(int a_field_device_type, int m_field)
{
    switch (a_field_device_type) {
    case 0: return "other";
    case 1: return "oil";
    case 2: return "electricity";
    case 3: return "gas";
    case 4: return "heat";
    case 5: return "steam";
    case 6: return "warm water";
    case 7: return "water";
    case 8: return "heat cost allocation";
    case 9: return "compressed air";
    case 0x0a: return "cooling load volume at outlet";
    case 0x0b: return "cooling load volume at inlet";
    case 0x0c: return "heat volume at inlet";
    case 0x0d: return "heat/cooling load";
    case 0x0e: return "bus/system component";
    case 0x0f: return "unknown";
    case 0x15: return "hot water";
    case 0x16: return "cold water";
    case 0x17: return "hot/cold water";
    case 0x18: return "pressure";
    case 0x19: return "a/d converter";
    case 0x1A: return "smoke detector";
    case 0x1B: return "room sensor";
    case 0x1C: return "gas detector";
    case 0x1D: return "reserved";
    case 0x1F: return "reserved";
    case 0x20: return "breaker";
    case 0x21: return "valve";
    case 0x22: return "reserved";
    case 0x23: return "reserved";
    case 0x24: return "reserved";
    case 0x25: return "customer unit (display device)";
    case 0x26: return "reserved";
    case 0x27: return "reserved";
    case 0x28: return "waste water";
    case 0x29: return "garbage";
    case 0x2A: return "reserved";
    case 0x2B: return "reserved";
    case 0x2C: return "reserved";
    case 0x2D: return "reserved";
    case 0x2E: return "reserved";
    case 0x2F: return "reserved";
    case 0x30: return "reserved";
    case 0x31: return "reserved";
    case 0x32: return "reserved";
    case 0x33: return "reserved";
    case 0x34: return "reserved";
    case 0x35: return "reserved";
    case 0x36: return "radio converter (system side)";
    case 0x37: return "radio converter (meter side)";
    case 0x38: return "reserved";
    case 0x39: return "reserved";
    case 0x3A: return "reserved";
    case 0x3B: return "reserved";
    case 0x3C: return "reserved";
    case 0x3D: return "reserved";
    case 0x3E: return "reserved";
    case 0x3F: return "reserved";
    }

    if (m_field == MANUFACTURER_TCH)
    {
        switch (a_field_device_type) {
            // Techem MK Radio 3/4 manufacturer specific.
        case 0x62: return "warm water"; // MKRadio3/MKRadio4
        case 0x72: return "cold water"; // MKRadio3/MKRadio4
            // Techem FHKV.
        case 0x80: return "heat cost allocator"; // FHKV data ii/iii
            // Techem Vario 4 Typ 4.5.1 manufacturer specific.
        case 0xC3: return "heat";
            // Techem V manufacturer specific.
        case 0x43: return "heat";
        case 0xf0: return "smoke detector";
        }
    }
    return "Unknown";
}

/*
    X(0x73, TPL_73,  "TPL: long header compact APL follows", 0, CI_TYPE::TPL, "") \
*/

#define LIST_OF_CI_FIELDS \
    X(0x51, TPL_51,  "TPL: APL follows", 0, CI_TYPE::TPL, "")       \
    X(0x72, TPL_72,  "TPL: long header APL follows", 0, CI_TYPE::TPL, "") \
    X(0x78, TPL_78,  "TPL: no header APL follows", 0, CI_TYPE::TPL, "") \
    X(0x79, TPL_79,  "TPL: compact APL follows", 0, CI_TYPE::TPL, "") \
    X(0x7A, TPL_7A,  "TPL: short header APL follows", 0, CI_TYPE::TPL, "") \
    X(0x81, NWL_81,  "NWL: TPL or APL follows?", 0, CI_TYPE::NWL, "") \
    X(0x8C, ELL_I,   "ELL: I",    2, CI_TYPE::ELL, "CC, ACC") \
    X(0x8D, ELL_II,  "ELL: II",   8, CI_TYPE::ELL, "CC, ACC, SN, Payload CRC") \
    X(0x8E, ELL_III, "ELL: III", 10, CI_TYPE::ELL, "CC, ACC, M2, A2") \
    X(0x8F, ELL_IV,  "ELL: IV",  16, CI_TYPE::ELL, "CC, ACC, M2, A2, SN, Payload CRC") \
    X(0x86, ELL_V,   "ELL: V",   -1, CI_TYPE::ELL, "Variable length") \
    X(0x90, AFL,     "AFL", 10, CI_TYPE::AFL, "")

enum CI_Field_Values {
#define X(val,name,cname,len,citype,explain) name = val,
    LIST_OF_CI_FIELDS
#undef X
};

bool isCiFieldOfType(int ci_field, CI_TYPE type)
{
#define X(val,name,cname,len,citype,explain) if (ci_field == val && type == citype) return true;
    LIST_OF_CI_FIELDS
#undef X
        return false;
}

int ciFieldLength(int ci_field)
{
#define X(val,name,cname,len,citype,explain) if (ci_field == val) return len;
    LIST_OF_CI_FIELDS
#undef X
        return -2;
}

bool isCiFieldManufacturerSpecific(int ci_field)
{
    return ci_field >= 0xA0 && ci_field <= 0xB7;
}

string ciType(int ci_field)
{
    if (ci_field >= 0xA0 && ci_field <= 0xB7) {
        return "Mfct specific";
    }
    if (ci_field >= 0x00 && ci_field <= 0x1f) {
        return "Reserved for DLMS";
    }

    if (ci_field >= 0x20 && ci_field <= 0x4f) {
        return "Reserved";
    }

    switch (ci_field) {
    case 0x50: return "Application reset or select to device (no tplh)";
    case 0x51: return "Command to device (no tplh)"; // Only for mbus, not wmbus.
    case 0x52: return "Selection of device (no tplh)";
    case 0x53: return "Application reset or select to device (long tplh)";
    case 0x54: return "Request of selected application to device (no tplh)";
    case 0x55: return "Request of selected application to device (long tplh)";
    case 0x56: return "Reserved";
    case 0x57: return "Reserved";
    case 0x58: return "Reserved";
    case 0x59: return "Reserved";
    case 0x5a: return "Command to device (short tplh)";
    case 0x5b: return "Command to device (long tplh)";
    case 0x5c: return "Sync action (no tplh)";
    case 0x5d: return "Reserved";
    case 0x5e: return "Reserved";
    case 0x5f: return "Specific usage";
    case 0x60: return "COSEM Data sent by the Readout device to the meter (long tplh)";
    case 0x61: return "COSEM Data sent by the Readout device to the meter (short tplh)";
    case 0x62: return "?";
    case 0x63: return "?";
    case 0x64: return "Reserved for OBIS-based Data sent by the Readout device to the meter (long tplh)";
    case 0x65: return "Reserved for OBIS-based Data sent by the Readout device to the meter (short tplh)";
    case 0x66: return "Response of selected application from device (no tplh)";
    case 0x67: return "Response of selected application from device (short tplh)";
    case 0x68: return "Response of selected application from device (long tplh)";
    case 0x69: return "EN 13757-3 Application Layer with Format frame (no tplh)";
    case 0x6A: return "EN 13757-3 Application Layer with Format frame (short tplh)";
    case 0x6B: return "EN 13757-3 Application Layer with Format frame (long tplh)";
    case 0x6C: return "Clock synchronisation (absolute) (long tplh)";
    case 0x6D: return "Clock synchronisation (relative) (long tplh)";
    case 0x6E: return "Application error from device (short tplh)";
    case 0x6F: return "Application error from device (long tplh)";
    case 0x70: return "Application error from device without Transport Layer";
    case 0x71: return "Reserved for Alarm Report";
    case 0x72: return "EN 13757-3 Application Layer (long tplh)";
    case 0x73: return "EN 13757-3 Application Layer with Compact frame and long Transport Layer";
    case 0x74: return "Alarm from device (short tplh)";
    case 0x75: return "Alarm from device (long tplh)";
    case 0x76: return "?";
    case 0x77: return "?";
    case 0x78: return "EN 13757-3 Application Layer (no tplh)";
    case 0x79: return "EN 13757-3 Application Layer with Compact frame (no tplh)";
    case 0x7A: return "EN 13757-3 Application Layer (short tplh)";
    case 0x7B: return "EN 13757-3 Application Layer with Compact frame (short tplh)";
    case 0x7C: return "COSEM Application Layer (long tplh)";
    case 0x7D: return "COSEM Application Layer (short tplh)";
    case 0x7E: return "Reserved for OBIS-based Application Layer (long tplh)";
    case 0x7F: return "Reserved for OBIS-based Application Layer (short tplh)";
    case 0x80: return "EN 13757-3 Transport Layer (long tplh) from other device to the meter";

    case 0x81: return "Network Layer data";
    case 0x82: return "Network management data to device (short tplh)";
    case 0x83: return "Network Management data to device (no tplh)";
    case 0x84: return "Transport layer to device (compact frame) (long tplh)";
    case 0x85: return "Transport layer to device (format frame) (long tplh)";
    case 0x86: return "Extended Link Layer V (variable length)";
    case 0x87: return "Network management data from device (long tplh)";
    case 0x88: return "Network management data from device (short tplh)";
    case 0x89: return "Network management data from device (no tplh)";
    case 0x8A: return "EN 13757-3 Transport Layer (short tplh) from the meter to the other device"; // No application layer, e.g. ACK
    case 0x8B: return "EN 13757-3 Transport Layer (long tplh) from the meter to the other device"; // No application layer, e.g. ACK

    case 0x8C: return "ELL: Extended Link Layer I (2 Byte)"; // CC, ACC
    case 0x8D: return "ELL: Extended Link Layer II (8 Byte)"; // CC, ACC, SN, Payload CRC
    case 0x8E: return "ELL: Extended Link Layer III (10 Byte)"; // CC, ACC, M2, A2
    case 0x8F: return "ELL: Extended Link Layer IV (16 Byte)"; // CC, ACC, M2, A2, SN, Payload CRC

    case 0x90: return "AFL: Authentication and Fragmentation Sublayer";
    case 0x91: return "Reserved";
    case 0x92: return "Reserved";
    case 0x93: return "Reserved";
    case 0x94: return "Reserved";
    case 0x95: return "Reserved";
    case 0x96: return "Reserved";
    case 0x97: return "Reserved";
    case 0x98: return "?";
    case 0x99: return "?";

    case 0xB8: return "Set baud rate to 300";
    case 0xB9: return "Set baud rate to 600";
    case 0xBA: return "Set baud rate to 1200";
    case 0xBB: return "Set baud rate to 2400";
    case 0xBC: return "Set baud rate to 4800";
    case 0xBD: return "Set baud rate to 9600";
    case 0xBE: return "Set baud rate to 19200";
    case 0xBF: return "Set baud rate to 38400";
    case 0xC0: return "Image transfer to device (long tplh)";
    case 0xC1: return "Image transfer from device (short tplh)";
    case 0xC2: return "Image transfer from device (long tplh)";
    case 0xC3: return "Security info transfer to device (long tplh)";
    case 0xC4: return "Security info transfer from device (short tplh)";
    case 0xC5: return "Security info transfer from device (long tplh)";
    }
    return "?";
}

void Telegram::addExplanationAndIncrementPos(vector<uchar>::iterator& pos, int len, KindOfData k, Understanding u, const char* fmt, ...)
{
    char buf[1024];
    buf[1023] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    Explanation e(parsed.size(), len, buf, k, u);
    explanations.push_back(e);
    parsed.insert(parsed.end(), pos, pos + len);
    pos += len;
}

void Telegram::setExplanation(vector<uchar>::iterator& pos, int len, KindOfData k, Understanding u, const char* fmt, ...)
{
    char buf[1024];
    buf[1023] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    Explanation e(distance(frame.begin(), pos), len, buf, k, u);
    explanations.push_back(e);
}

void Telegram::addMoreExplanation(int pos, string json)
{
    addMoreExplanation(pos, " (%s)", json.c_str());
}

void Telegram::addMoreExplanation(int pos, const char* fmt, ...)
{
    char buf[1024];

    buf[1023] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    bool found = false;
    for (auto& p : explanations) {
        if (p.pos == pos)
        {
            // Append more information.
            p.info = p.info + buf;
            // Since we are adding more information, we assume that we have a full understanding.
            p.understanding = Understanding::FULL;
            found = true;
        }
    }

    if (!found) {
        debug("(wmbus) warning: cannot find offset %d to add more explanation \"%s\"", pos, buf);
    }
}

void Telegram::addSpecialExplanation(int offset, int len, KindOfData k, Understanding u, const char* fmt, ...)
{
    char buf[1024];
    buf[1023] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    explanations.push_back({ offset, len, buf, k, u });
}

bool expectedMore(int line)
{
    verbose("(wmbus) parser expected more data! (%d)", line);
    return false;
}

bool Telegram::parseMBusDLLandTPL(vector<uchar>::iterator& pos)
{
    int remaining = distance(pos, frame.end());

    if (remaining == 1 && *pos == 0xE5)
    {
        addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "E5");
        return true;
    }

    if (remaining < 5) {
        return expectedMore(__LINE__);
    }

    debug("(mbus) parse MBUS DLL @%d %d", distance(frame.begin(), pos), remaining);
    debug("(mbus) ", frame);

    if (*pos != 0x68) return false;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "68 start");

    dll_len = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x length (%d bytes)", dll_len, dll_len);

    // Two identical length bytes are expected!
    if (*pos != dll_len) return false;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x length again (%d bytes)", dll_len, dll_len);

    if (*pos != 0x68) return false;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "68 start");

    if (remaining < dll_len) return expectedMore(__LINE__);

    // Last byte should be 0x16
    auto end = frame.end();
    end--;
    if (*end != 0x16) return false;
    setExplanation(end, 1, KindOfData::PROTOCOL, Understanding::FULL, "16 end");

    // Second last byte should be crc. Should have been checked before! No need to check again here?
    end--;
    setExplanation(end, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02X crc", *end);

    // Mark crc and end as suffix, ie should not be parsed by dvparser.
    suffix_size = 2;

    dll_c = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x dll-c (%s)", dll_c, mbusCField(dll_c));

    mbus_primary_address = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x dll-a primary (%d)", mbus_primary_address, mbus_primary_address);

    // Add dll_id to ids.
    string id = tostrprintf("p%d", mbus_primary_address);
    Address a;
    a.id = id;
    addresses.push_back(a);

    mbus_ci = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x tpl-ci (%s)", mbus_ci, mbusCiField(mbus_ci));

    if (mbus_ci == 0x72)
    {
        return parse_TPL_72(pos);
    }

    return false;
}

bool Telegram::parseDLL(vector<uchar>::iterator& pos)
{
    int remaining = distance(pos, frame.end());
    if (remaining == 0) return expectedMore(__LINE__);

    debug("(wmbus) parseDLL @%d %d", distance(frame.begin(), pos), remaining);
    dll_len = *pos;
    if (remaining < dll_len) return expectedMore(__LINE__);
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x length (%d bytes)", dll_len, dll_len);

    dll_c = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x dll-c (%s)", dll_c, cType(dll_c).c_str());

    CHECK(8)
    addAddressMfctFirst(pos);

    dll_mfct_b[0] = *(pos + 0);
    dll_mfct_b[1] = *(pos + 1);
    dll_mfct = dll_mfct_b[1] << 8 | dll_mfct_b[0];
    string man = manufacturerFlag(dll_mfct);
    addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x dll-mfct (%s)",
        dll_mfct_b[0], dll_mfct_b[1], man.c_str());

    dll_a.resize(6);
    dll_id.resize(4);
    for (int i = 0; i < 6; ++i)
    {
        dll_a[i] = *(pos + i);
        if (i < 4)
        {
            dll_id_b[i] = *(pos + i);
            dll_id[i] = *(pos + 3 - i);
        }
    }
    // Add dll_id to ids.
    addExplanationAndIncrementPos(pos, 4, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x%02x%02x dll-id (%s)",
                                  *(pos+0), *(pos+1), *(pos+2), *(pos+3), addresses.back().id.c_str());

    dll_version = *(pos+0);
    dll_type = *(pos + 1);
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x dll-version", dll_version);
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x dll-type (%s)", dll_type,
        mediaType(dll_type, dll_mfct).c_str());

    return true;
}

bool Telegram::parseELL(vector<uchar>::iterator& pos)
{
    int remaining = distance(pos, frame.end());
    if (remaining == 0) return false;

    debug("(wmbus) parseELL @%d %d", distance(frame.begin(), pos), remaining);
    int ci_field = *pos;
    if (!isCiFieldOfType(ci_field, CI_TYPE::ELL)) return true;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x ell-ci-field (%s)",
        ci_field, ciType(ci_field).c_str());
    ell_ci = ci_field;
    int len = ciFieldLength(ell_ci);

    if (remaining < len + 1) return expectedMore(__LINE__);

    // All ELL:s (including ELL I) start with cc,acc.

    ell_cc = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x ell-cc (%s)", ell_cc, ccType(ell_cc).c_str());

    ell_acc = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x ell-acc", ell_acc);

    bool has_target_mft_address = false;
    bool has_session_number_pl_crc = false;

    switch (ell_ci)
    {
    case CI_Field_Values::ELL_I:
        // Already handled above.
        break;
    case CI_Field_Values::ELL_II:
        has_session_number_pl_crc = true;
        break;
    case CI_Field_Values::ELL_III:
        has_target_mft_address = true;
        break;
    case CI_Field_Values::ELL_IV:
        has_session_number_pl_crc = true;
        has_target_mft_address = true;
        break;
    case CI_Field_Values::ELL_V:
        ("ELL V not yet handled");
        return false;
    }

    if (has_target_mft_address)
    {
        CHECK(8);
        addAddressMfctFirst(pos);

        ell_mfct_b[0] = *(pos + 0);
        ell_mfct_b[1] = *(pos + 1);
        ell_mfct = ell_mfct_b[1] << 8 | ell_mfct_b[0];
        string man = manufacturerFlag(ell_mfct);
        addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x ell-mfct (%s)",
            ell_mfct_b[0], ell_mfct_b[1], man.c_str());

        ell_id_found = true;
        ell_id_b[0] = *(pos + 0);
        ell_id_b[1] = *(pos + 1);
        ell_id_b[2] = *(pos + 2);
        ell_id_b[3] = *(pos + 3);

        // Add ell_id to ids.
        addExplanationAndIncrementPos(pos, 4, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x%02x%02x ell-id",
            ell_id_b[0], ell_id_b[1], ell_id_b[2], ell_id_b[3]);

        ell_version = *pos;
        addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x ell-version", ell_version);

        ell_type = *pos;
        addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x ell-type", ell_type);
    }

    if (has_session_number_pl_crc)
    {
        string sn_info;
        ell_sn_b[0] = *(pos + 0);
        ell_sn_b[1] = *(pos + 1);
        ell_sn_b[2] = *(pos + 2);
        ell_sn_b[3] = *(pos + 3);
        ell_sn = ell_sn_b[3] << 24 | ell_sn_b[2] << 16 | ell_sn_b[1] << 8 | ell_sn_b[0];

        ell_sn_session = (ell_sn >> 0) & 0x0f; // lowest 4 bits
        ell_sn_time = (ell_sn >> 4) & 0x1ffffff; // next 25 bits
        ell_sn_sec = (ell_sn >> 29) & 0x7; // next 3 bits.
        ell_sec_mode = fromIntToELLSecurityMode(ell_sn_sec);
        string info = toString(ell_sec_mode);
        addExplanationAndIncrementPos(pos, 4, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x%02x%02x sn (%s)",
            ell_sn_b[0], ell_sn_b[1], ell_sn_b[2], ell_sn_b[3], info.c_str());

        if (ell_sec_mode == ELLSecurityMode::AES_CTR)
        {
            if (meter_keys)
            {
                decrypt_ELL_AES_CTR(this, frame, pos, meter_keys->confidentiality_key);
                // Actually this ctr decryption always succeeds, if wrong key, it will decrypt to garbage.
            }
            // Now the frame from pos and onwards has been decrypted, perhaps.
        }

        ell_pl_crc_b[0] = *(pos + 0);
        ell_pl_crc_b[1] = *(pos + 1);
        ell_pl_crc = (ell_pl_crc_b[1] << 8) | ell_pl_crc_b[0];

        int dist = distance(frame.begin(), pos + 2);
        int len = distance(pos + 2, frame.end());
        uint16_t check = crc16_EN13757(&(frame[dist]), len);

        addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL,
            "%02x%02x payload crc (calculated %02x%02x %s)",
            ell_pl_crc_b[0], ell_pl_crc_b[1],
            check & 0xff, check >> 8, (ell_pl_crc == check ? "OK" : "ERROR"));


        if (ell_pl_crc == check || FUZZING)
        {
        }
        else
        {
            // Ouch, checksum of the payload does not match.
            // A wrong key, or no key was probably used for decryption.
            decryption_failed = true;

            // Log the content as failed decryption.
            int num_encrypted_bytes = frame.end() - pos;
            string info = bin2hex(pos, frame.end(), num_encrypted_bytes);
            info += " failed decryption. Wrong key?";
            addExplanationAndIncrementPos(pos, num_encrypted_bytes, KindOfData::CONTENT, Understanding::ENCRYPTED, info.c_str());

            if (parser_warns_)
            {
                if (!beingAnalyzed() && (isVerboseEnabled() || isDebugEnabled()))
                {
                    // Print this warning only once! Unless you are using verbose or debug.
                    warning("(wmbus) WARNING! decrypted payload crc failed check, did you use the correct decryption key? "
                        "%02x%02x payload crc (calculated %02x%02x) "
                        "Permanently ignoring telegrams from id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x",
                        ell_pl_crc_b[0], ell_pl_crc_b[1],
                        check & 0xff, check >> 8,
                        dll_id_b[3], dll_id_b[2], dll_id_b[1], dll_id_b[0],
                        manufacturerFlag(dll_mfct).c_str(),
                        manufacturer(dll_mfct).c_str(),
                        dll_mfct,
                        mediaType(dll_type, dll_mfct).c_str(), dll_type,
                        dll_version);
                }
            }
        }
    }

    return true;
}

bool Telegram::parseNWL(vector<uchar>::iterator& pos)
{
    int remaining = distance(pos, frame.end());
    if (remaining == 0) return false;

    debug("(wmbus) parseNWL @%d %d", distance(frame.begin(), pos), remaining);
    int ci_field = *pos;
    if (!isCiFieldOfType(ci_field, CI_TYPE::NWL)) return true;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x nwl-ci-field (%s)",
        ci_field, ciType(ci_field).c_str());
    nwl_ci = ci_field;
    // We have only seen 0x81 0x1d so far.
    int len = 1; // ciFieldLength(nwl_ci);

    if (remaining < len + 1) return expectedMore(__LINE__);

    uchar nwl = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x nwl?", nwl);

    return true;
}


string toStringFromAFLFC(int fc)
{
    string info = "";
    int fid = fc & 0x00ff; // Fragmend id
    info += std::to_string(fid);
    info += " ";
    if (fc & 0x0200) info += "KeyInfoInFragment ";
    if (fc & 0x0400) info += "MACInFragment ";
    if (fc & 0x0800) info += "MessCounterInFragment ";
    if (fc & 0x1000) info += "MessLenInFragment ";
    if (fc & 0x2000) info += "MessControlInFragment ";
    if (fc & 0x4000) info += "MoreFragments ";
    else             info += "LastFragment ";
    if (info.length() > 0) info.pop_back();
    return info;
}


string toStringFromAFLMC(int mc)
{
    string info = "";
    int at = mc & 0x0f;
    AFLAuthenticationType aat = fromIntToAFLAuthenticationType(at);
    info += toString(aat);
    info += " ";
    if (mc & 0x10) info += "KeyInfo ";
    if (mc & 0x20) info += "MessCounter ";
    if (mc & 0x40) info += "MessLen ";
    if (info.length() > 0) info.pop_back();
    return info;
}

bool Telegram::parseAFL(vector<uchar>::iterator& pos)
{
    // 90 0F (len) 002C (fc) 25 (mc) 49EE 0A00 77C1 9D3D 1A08 ABCD --- 729067296179161102F
    // 90 0F (len) 002C (fc) 25 (mc) 0C39 0000 ED17 6BBB B159 1ADB --- 7A1D003007103EA

    int remaining = distance(pos, frame.end());
    if (remaining == 0) return false;

    debug("(wmbus) parseAFL @%d %d", distance(frame.begin(), pos), remaining);

    int ci_field = *pos;
    if (!isCiFieldOfType(ci_field, CI_TYPE::AFL)) return true;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x afl-ci-field (%s)",
        ci_field, ciType(ci_field).c_str());
    afl_ci = ci_field;

    afl_len = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x afl-len (%d)",
        afl_len, afl_len);

    int len = ciFieldLength(afl_ci);
    if (remaining < len) return expectedMore(__LINE__);

    afl_fc_b[0] = *(pos + 0);
    afl_fc_b[1] = *(pos + 1);
    afl_fc = afl_fc_b[1] << 8 | afl_fc_b[0];
    string afl_fc_info = toStringFromAFLFC(afl_fc);
    addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x afl-fc (%s)",
        afl_fc_b[0], afl_fc_b[1], afl_fc_info.c_str());

    bool has_key_info = afl_fc & 0x0200;
    bool has_mac = afl_fc & 0x0400;
    bool has_counter = afl_fc & 0x0800;
    //bool has_len = afl_fc & 0x1000;
    bool has_control = afl_fc & 0x2000;
    //bool has_more_fragments = afl_fc & 0x4000;

    if (has_control)
    {
        afl_mcl = *pos;
        string afl_mcl_info = toStringFromAFLMC(afl_mcl);
        addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x afl-mcl (%s)",
            afl_mcl, afl_mcl_info.c_str());
    }

    if (has_key_info)
    {
        afl_ki_b[0] = *(pos + 0);
        afl_ki_b[1] = *(pos + 1);
        afl_ki = afl_ki_b[1] << 8 | afl_ki_b[0];
        string afl_ki_info = "";
        addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x afl-ki (%s)",
            afl_ki_b[0], afl_ki_b[1], afl_ki_info.c_str());
    }

    if (has_counter)
    {
        afl_counter_b[0] = *(pos + 0);
        afl_counter_b[1] = *(pos + 1);
        afl_counter_b[2] = *(pos + 2);
        afl_counter_b[3] = *(pos + 3);
        afl_counter = afl_counter_b[3] << 24 |
            afl_counter_b[2] << 16 |
            afl_counter_b[1] << 8 |
            afl_counter_b[0];

        addExplanationAndIncrementPos(pos, 4, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x%02x%02x afl-counter (%u)",
            afl_counter_b[0], afl_counter_b[1],
            afl_counter_b[2], afl_counter_b[3],
            afl_counter);
    }

    if (has_mac)
    {
        int at = afl_mcl & 0x0f;
        AFLAuthenticationType aat = fromIntToAFLAuthenticationType(at);
        int len = toLen(aat);
        if (len != 2 &&
            len != 4 &&
            len != 8 &&
            len != 12 &&
            len != 16)
        {
            if (parser_warns_)
            {
                warning("(wmbus) WARNING! bad length of mac");
            }
            return false;
        }
        afl_mac_b.clear();
        for (int i = 0; i < len; ++i)
        {
            afl_mac_b.insert(afl_mac_b.end(), *(pos + i));
        }
        string s = bin2hex(afl_mac_b);
        addExplanationAndIncrementPos(pos, len, KindOfData::PROTOCOL, Understanding::FULL, "%s afl-mac %d bytes", s.c_str(), len);
        must_check_mac = true;
    }

    return true;
}

bool Telegram::parseTPLConfig(std::vector<uchar>::iterator& pos)
{
    uchar cfg1 = *(pos + 0);
    uchar cfg2 = *(pos + 1);
    tpl_cfg = cfg2 << 8 | cfg1;

    if (tpl_cfg & 0x1f00)
    {
        int m = (tpl_cfg >> 8) & 0x1f;
        tpl_sec_mode = fromIntToTPLSecurityMode(m);
    }
    bool has_cfg_ext = false;
    string info = toStringFromTPLConfig(tpl_cfg);
    info += " ";
    if (tpl_sec_mode == TPLSecurityMode::AES_CBC_IV) // Security mode 5
    {
        tpl_num_encr_blocks = (tpl_cfg >> 4) & 0x0f;
    }
    if (tpl_sec_mode == TPLSecurityMode::AES_CBC_NO_IV) // Security mode 7
    {
        tpl_num_encr_blocks = (tpl_cfg >> 4) & 0x0f;
        has_cfg_ext = true;
    }
    addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL,
        "%02x%02x tpl-cfg %04x (%s)", cfg1, cfg2, tpl_cfg, info.c_str());

    if (has_cfg_ext)
    {
        tpl_cfg_ext = *(pos + 0);
        tpl_kdf_selection = (tpl_cfg_ext >> 4) & 3;

        addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL,
            "%02x tpl-cfg-ext (KDFS=%d)", tpl_cfg_ext, tpl_kdf_selection);

        if (tpl_kdf_selection == 1)
        {
            vector<uchar> input;
            vector<uchar> mac;
            mac.resize(16);

            // DC C ID 0x07 0x07 0x07 0x07 0x07 0x07 0x07
            // Derivation Constant DC = 0x00 = encryption from meter.
            //                          0x01 = mac from meter.
            //                          0x10 = encryption from communication partner.
            //                          0x11 = mac from communication partner.
            input.insert(input.end(), 0x00); // DC 00 = generate ephemereal encryption key from meter.
            // If there is a tpl_counter, then use it, else use afl_counter.
            input.insert(input.end(), afl_counter_b, afl_counter_b + 4);
            // If there is a tpl_id, then use it, else use ddl_id.
            if (tpl_id_found)
            {
                input.insert(input.end(), tpl_id_b, tpl_id_b + 4);
            }
            else
            {
                input.insert(input.end(), dll_id_b, dll_id_b + 4);
            }

            // Pad.
            for (int i = 0; i < 7; ++i) input.insert(input.end(), 0x07);

            debug("(wmbus) input to kdf for enc", input);

            if (meter_keys == NULL || meter_keys->confidentiality_key.size() != 16)
            {
                if (isSimulated())
                {
                    debug("(wmbus) simulation without keys, not generating Kmac and Kenc");
                    return true;
                }
                debug("(wmbus) no key, thus cannot execute kdf.");
                return false;
            }
            AES_CMAC(safeButUnsafeVectorPtr(meter_keys->confidentiality_key),
                safeButUnsafeVectorPtr(input), 16,
                safeButUnsafeVectorPtr(mac));
            string s = bin2hex(mac);
            debug("(wmbus) ephemereal Kenc %s", s.c_str());
            tpl_generated_key.clear();
            tpl_generated_key.insert(tpl_generated_key.end(), mac.begin(), mac.end());

            input[0] = 0x01; // DC 01 = generate ephemereal mac key from meter.
            mac.clear();
            mac.resize(16);
            debug("(wmbus) input to kdf for mac", input);
            AES_CMAC(safeButUnsafeVectorPtr(meter_keys->confidentiality_key),
                safeButUnsafeVectorPtr(input), 16,
                safeButUnsafeVectorPtr(mac));
            s = bin2hex(mac);
            debug("(wmbus) ephemereal Kmac %s", s.c_str());
            tpl_generated_mac_key.clear();
            tpl_generated_mac_key.insert(tpl_generated_mac_key.end(), mac.begin(), mac.end());
        }
    }

    return true;
}


string decodeTPLStatusByteOnlyStandardBits(uchar sts)
{
    // Bits 0-4 are standard defined. Bits 5-7 are mfct specific.
    string s;

    if (sts == 0) return "OK";
    if ((sts & 0x03) == 0x01) s += "BUSY ";  // Meter busy, cannot respond.
    if ((sts & 0x03) == 0x02) s += "ERROR "; // E.g. meter failed to understand a message sent to it.
    // More information about the error can be sent using error reporting, EN13757-3:2018 §10
    if ((sts & 0x03) == 0x03) s += "ALARM "; // E.g. an abnormal condition like water is continuously running.

    if ((sts & 0x04) == 0x04) s += "POWER_LOW "; // E.g. battery end of life or external power supply failure
    if ((sts & 0x08) == 0x08) s += "PERMANENT_ERROR "; // E.g. meter needs service to work again.
    if ((sts & 0x10) == 0x10) s += "TEMPORARY_ERROR ";

    while (s.size() > 0 && s.back() == ' ') s.pop_back();
    return s;
}

string decodeTPLStatusByteNoMfct(uchar sts)
{
    string t = "OK";

    if ((sts & 0xe0) != 0)
    {
        t = tostrprintf("UNKNOWN_%02X", sts & 0xe0);
    }

    return t;
}

string decodeTPLStatusByteWithMfct(uchar sts, Translate::Lookup& lookup)
{
    string s = decodeTPLStatusByteOnlyStandardBits(sts);
    string t = "OK";

    if ((sts & 0xe0) != 0)
    {
        // Vendor specific bits are set, lets translate them.
        if (lookup.hasLookups())
        {
            t = lookup.translate(sts & 0xe0);
        }
        else
        {
            t = decodeTPLStatusByteNoMfct(sts & 0xe0);
        }
    }

    if (t == "OK" || t == "") return s;
    if (s == "OK" || s == "") return t;

    return s + " " + t;
}

bool Telegram::parseShortTPL(std::vector<uchar>::iterator& pos)
{
    tpl_acc = *pos;
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL,
        "%02x tpl-acc-field", tpl_acc);

    tpl_sts = *pos;
    tpl_sts_offset = distance(frame.begin(), pos);
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL,
        "%02x tpl-sts-field (%s)", tpl_sts, decodeTPLStatusByteOnlyStandardBits(tpl_sts).c_str());
    bool ok = parseTPLConfig(pos);
    if (!ok) return false;

    return true;
}

bool Telegram::parseLongTPL(std::vector<uchar>::iterator& pos)
{
    CHECK(8);
    addAddressIdFirst(pos);

    tpl_id_found = true;
    tpl_id_b[0] = *(pos + 0);
    tpl_id_b[1] = *(pos + 1);
    tpl_id_b[2] = *(pos + 2);
    tpl_id_b[3] = *(pos + 3);

    tpl_a.resize(6);
    for (int i = 0; i < 4; ++i)
    {
        tpl_a[i] = *(pos + i);
    }

    addExplanationAndIncrementPos(pos, 4, KindOfData::PROTOCOL, Understanding::FULL,
        "%02x%02x%02x%02x tpl-id (%02x%02x%02x%02x)",
        tpl_id_b[0], tpl_id_b[1], tpl_id_b[2], tpl_id_b[3],
        tpl_id_b[3], tpl_id_b[2], tpl_id_b[1], tpl_id_b[0]);

    tpl_mfct_b[0] = *(pos + 0);
    tpl_mfct_b[1] = *(pos + 1);
    tpl_mfct = tpl_mfct_b[1] << 8 | tpl_mfct_b[0];
    string man = manufacturerFlag(tpl_mfct);
    addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x tpl-mfct (%s)", tpl_mfct_b[0], tpl_mfct_b[1], man.c_str());

    tpl_version = *(pos + 0);
    tpl_a[4] = *(pos + 0);
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x tpl-version", tpl_version);

    tpl_type = *(pos + 0);
    tpl_a[5] = *(pos + 0);
    string info = mediaType(tpl_type, tpl_mfct);
    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02x tpl-type (%s)", tpl_type, info.c_str());

    bool ok = parseShortTPL(pos);

    return ok;
}

bool Telegram::checkMAC(std::vector<uchar>& frame,
    std::vector<uchar>::iterator from,
    std::vector<uchar>::iterator to,
    std::vector<uchar>& inmac,
    std::vector<uchar>& mackey)
{
    vector<uchar> input;
    vector<uchar> mac;
    mac.resize(16);

    if (mackey.size() != 16) return false;
    if (inmac.size() == 0) return false;

    // AFL.MAC = CMAC (Kmac/Lmac,
    //                 AFL.MCL || AFL.MCR || {AFL.ML || } NextCI || ... || Last Byte of message)

    input.insert(input.end(), afl_mcl);
    input.insert(input.end(), afl_counter_b, afl_counter_b + 4);
    input.insert(input.end(), from, to);
    string s = bin2hex(input);
    debug("(wmbus) input to mac %s", s.c_str());
    AES_CMAC(safeButUnsafeVectorPtr(mackey),
        safeButUnsafeVectorPtr(input), input.size(),
        safeButUnsafeVectorPtr(mac));
    string calculated = bin2hex(mac);
    debug("(wmbus) calculated mac %s", calculated.c_str());
    string received = bin2hex(inmac);
    debug("(wmbus) received   mac %s", received.c_str());
    string truncated = calculated.substr(0, received.length());
    bool ok = truncated == received;
    if (ok)
    {
        debug("(wmbus) mac ok!");
    }
    else
    {
        debug("(wmbus) mac NOT ok!");
        explainParse("BADMAC", 0);
    }
    return ok;
}

bool loadFormatBytesFromSignature(uint16_t format_signature, vector<uchar>* format_bytes);

bool Telegram::alreadyDecryptedCBC(vector<uchar>::iterator& pos)
{
    if (*(pos + 0) != 0x2f || *(pos + 1) != 0x2f) return false;
    addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL, "%02x%02x already decrypted check bytes", *(pos + 0), *(pos + 1));
    return true;
}

bool Telegram::potentiallyDecrypt(vector<uchar>::iterator& pos)
{
    if (tpl_sec_mode == TPLSecurityMode::AES_CBC_IV)
    {
        if (alreadyDecryptedCBC(pos))
        {
            if (meter_keys && meter_keys->hasConfidentialityKey())
            {
                // Oups! This telegram is already decrypted (but the header still says it should be encrypted)
                // this is probably a replay telegram from --logtelegrams.
                // But since we have specified a key! Do not accept this telegram!
                warning("(wmbus) WARNING!! telegram should have been fully encrypted, but was not! "
                    "id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x",
                    dll_id_b[3], dll_id_b[2], dll_id_b[1], dll_id_b[0],
                    manufacturerFlag(dll_mfct).c_str(),
                    manufacturer(dll_mfct).c_str(),
                    dll_mfct,
                    mediaType(dll_type, dll_mfct).c_str(), dll_type,
                    dll_version);
                return false;
            }
            return true;
        }
        if (!meter_keys) return false;
        if (!meter_keys->hasConfidentialityKey())
        {
            addDefaultManufacturerKeyIfAny(frame, tpl_sec_mode, meter_keys);
        }
        int num_encrypted_bytes = 0;
        int num_not_encrypted_at_end = 0;

        bool ok = decrypt_TPL_AES_CBC_IV(this, frame, pos, meter_keys->confidentiality_key,
            &num_encrypted_bytes, &num_not_encrypted_at_end);
        if (!ok)
        {
            // No key supplied.
            string info = bin2hex(pos, frame.end(), num_encrypted_bytes);
            info += " encrypted";
            addExplanationAndIncrementPos(pos, num_encrypted_bytes, KindOfData::CONTENT, Understanding::ENCRYPTED, info.c_str());
            if (parser_warns_)
            {
                if (!beingAnalyzed() && (isVerboseEnabled() || isDebugEnabled()))
                {
                    // Print this warning only once! Unless you are using verbose or debug.
                    warning("(wmbus) WARNING! no key to decrypt payload! "
                        "Permanently ignoring telegrams from id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x",
                        dll_id_b[3], dll_id_b[2], dll_id_b[1], dll_id_b[0],
                        manufacturerFlag(dll_mfct).c_str(),
                        manufacturer(dll_mfct).c_str(),
                        dll_mfct,
                        mediaType(dll_type, dll_mfct).c_str(), dll_type,
                        dll_version);
                }
            }
            return false;
        }
        // Now the frame from pos and onwards has been decrypted.
        uchar a = *(pos + 0);
        uchar b = *(pos + 1);

        addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL,
            "%02x%02x decrypt check bytes (%s)", *(pos + 0), *(pos + 1),
            (*(pos + 0) == 0x2f && *(pos + 1) == 0x2f) ? "OK" : "ERROR should be 2f2f");

        if ((a != 0x2f || b != 0x2f) && !FUZZING)
        {
            // Wrong key supplied.
            int num_bytes = distance(pos, frame.end());
            string info = bin2hex(pos, frame.end(), num_bytes);
            info += " failed decryption. Wrong key?";
            addExplanationAndIncrementPos(pos, num_bytes, KindOfData::CONTENT, Understanding::ENCRYPTED, info.c_str());

            if (parser_warns_)
            {
                if (!beingAnalyzed() && (isVerboseEnabled() || isDebugEnabled()))
                {
                    // Print this warning only once! Unless you are using verbose or debug.
                    warning("(wmbus) WARNING!! decrypted content failed check, did you use the correct decryption key? "
                        "Permanently ignoring telegrams from id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x",
                        dll_id_b[3], dll_id_b[2], dll_id_b[1], dll_id_b[0],
                        manufacturerFlag(dll_mfct).c_str(),
                        manufacturer(dll_mfct).c_str(),
                        dll_mfct,
                        mediaType(dll_type, dll_mfct).c_str(), dll_type,
                        dll_version);
                }
            }
            return false;
        }
    }
    else if (tpl_sec_mode == TPLSecurityMode::AES_CBC_NO_IV)
    {
        if (alreadyDecryptedCBC(pos))
        {
            if (meter_keys && meter_keys->hasConfidentialityKey())
            {
                // Oups! This telegram is already decrypted (but the header still says it should be encrypted)
                // this is probably a replay telegram from --logtelegrams.
                // But since we have specified a key! Do not accept this telegram!
                warning("(wmbus) WARNING! telegram should have been fully encrypted, but was not! "
                    "id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x",
                    dll_id_b[3], dll_id_b[2], dll_id_b[1], dll_id_b[0],
                    manufacturerFlag(dll_mfct).c_str(),
                    manufacturer(dll_mfct).c_str(),
                    dll_mfct,
                    mediaType(dll_type, dll_mfct).c_str(), dll_type,
                    dll_version);
                return false;
            }
            return true;
        }

        bool mac_ok = checkMAC(frame, tpl_start, frame.end(), afl_mac_b, tpl_generated_mac_key);

        // Do not attempt to decrypt if the mac has failed!
        if (!mac_ok)
        {
            if (parser_warns_)
            {
                if (!beingAnalyzed() && (isVerboseEnabled() || isDebugEnabled()))
                {
                    // Print this warning only once! Unless you are using verbose or debug.
                    warning("(wmbus) WARNING! telegram mac check failed, did you use the correct decryption key? "
                        "Permanently ignoring telegrams from id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x",
                        dll_id_b[3], dll_id_b[2], dll_id_b[1], dll_id_b[0],
                        manufacturerFlag(dll_mfct).c_str(),
                        manufacturer(dll_mfct).c_str(),
                        dll_mfct,
                        mediaType(dll_type, dll_mfct).c_str(), dll_type,
                        dll_version);
                    return false;
                }

                string info = bin2hex(pos, frame.end(), frame.end() - pos);
                info += " encrypted mac failed";
                addExplanationAndIncrementPos(pos, frame.end() - pos, KindOfData::CONTENT, Understanding::ENCRYPTED, info.c_str());
                if (meter_keys->confidentiality_key.size() > 0)
                {
                    // Only fail if we gave an explicit key.
                    return false;
                }
                return true;
            }
            return false;
        }

        int num_encrypted_bytes = 0;
        int num_not_encrypted_at_end = 0;
        bool ok = decrypt_TPL_AES_CBC_NO_IV(this, frame, pos, tpl_generated_key,
            &num_encrypted_bytes,
            &num_not_encrypted_at_end);
        if (!ok)
        {
            addExplanationAndIncrementPos(pos, num_encrypted_bytes, KindOfData::CONTENT, Understanding::FULL,
                "encrypted data");
            return false;
        }

        // Now the frame from pos and onwards has been decrypted.
        uchar a = *(pos + 0);
        uchar b = *(pos + 1);
        addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL,
            "%02x%02x decrypt check bytes (%s)", a, b,
            (a == 0x2f && b == 0x2f) ? "OK" : "ERROR should be 2f2f");

        if ((a != 0x2f || b != 0x2f) && !FUZZING)
        {
            // Wrong key supplied.
            string info = bin2hex(pos, frame.end(), num_encrypted_bytes);
            info += " failed decryption. Wrong key?";
            addExplanationAndIncrementPos(pos, num_encrypted_bytes, KindOfData::CONTENT, Understanding::ENCRYPTED, info.c_str());

            if (parser_warns_)
            {
                if (!beingAnalyzed() && (isVerboseEnabled() || isDebugEnabled()))
                {
                    // Print this warning only once! Unless you are using verbose or debug.
                    warning("(wmbus) WARNING!!! decrypted content failed check, did you use the correct decryption key? "
                        "Permanently ignoring telegrams from id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x",
                        dll_id_b[3], dll_id_b[2], dll_id_b[1], dll_id_b[0],
                        manufacturerFlag(dll_mfct).c_str(),
                        manufacturer(dll_mfct).c_str(),
                        dll_mfct,
                        mediaType(dll_type, dll_mfct).c_str(), dll_type,
                        dll_version);
                }
            }
            return false;
        }
    }
    else if (tpl_sec_mode == TPLSecurityMode::SPECIFIC_16_31)
    {
        debug("(wmbus) non-standard security mode 16_31");
        if (mustDecryptDiehlRealData(frame))
        {
            debug("(diehl) must decode frame");
            if (!meter_keys) return false;
            bool ok = decryptDielhRealData(this, frame, pos, meter_keys->confidentiality_key);
            // If this telegram is simulated, the content might already be decrypted and the
            // decruption will fail. But we can assume all is well anyway!
            if (!ok && isSimulated()) return true;
            if (!ok) return false;
            // Now the frame from pos and onwards has been decrypted.
            debug("(diehl) decryption successful");
        }
    }
    else
        if (meter_keys && meter_keys->hasConfidentialityKey())
        {
            // Oups! This telegram is NOT encrypted, but we have specified a key!
            // Do not accept this telegram!
            warning("(wmbus) WARNING!!! telegram should have been encrypted, but was not! "
                "id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x",
                dll_id_b[3], dll_id_b[2], dll_id_b[1], dll_id_b[0],
                manufacturerFlag(dll_mfct).c_str(),
                manufacturer(dll_mfct).c_str(),
                dll_mfct,
                mediaType(dll_type, dll_mfct).c_str(), dll_type,
                dll_version);
            return false;
        }

    return true;
}

bool Telegram::parse_TPL_72(vector<uchar>::iterator& pos)
{
    bool ok = parseLongTPL(pos);
    if (!ok) return false;

    bool decrypt_ok = potentiallyDecrypt(pos);

    header_size = distance(frame.begin(), pos);
    int remaining = distance(pos, frame.end()) - suffix_size;

    if (decrypt_ok)
    {
        parseDV(this, frame, pos, remaining, &dv_entries);
    }
    else
    {
        decryption_failed = true;
    }

    return true;
}

bool Telegram::parse_TPL_78(vector<uchar>::iterator& pos)
{
    header_size = distance(frame.begin(), pos);
    int remaining = distance(pos, frame.end()) - suffix_size;
    parseDV(this, frame, pos, remaining, &dv_entries);

    return true;
}

bool Telegram::parse_TPL_79(vector<uchar>::iterator& pos)
{
    // Compact frame
    uchar ecrc0 = *(pos + 0);
    uchar ecrc1 = *(pos + 1);
    size_t offset = distance(frame.begin(), pos);
    addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL,
        "%02x%02x format signature", ecrc0, ecrc1);
    format_signature = ecrc1 << 8 | ecrc0;

    vector<uchar> format_bytes;
    bool ok = loadFormatBytesFromSignature(format_signature, &format_bytes);
    if (!ok) {
        // We have not yet seen a long frame, but we know the formats for some
        // meter specific hashes.
        ok = findFormatBytesFromKnownMeterSignatures(&format_bytes);
        if (!ok)
        {
            addMoreExplanation(offset, " (unknown)");
            int num_compressed_bytes = distance(pos, frame.end());
            string info = bin2hex(pos, frame.end(), num_compressed_bytes);
            info += " compressed and signature unknown";
            addExplanationAndIncrementPos(pos, distance(pos, frame.end()), KindOfData::CONTENT, Understanding::COMPRESSED, info.c_str());

            verbose("(wmbus) ignoring compressed telegram since format signature hash 0x%02x is yet unknown.\n"
                "     this is not a problem, since you only need wait for at most 8 telegrams\n"
                "     (8*16 seconds) until an full length telegram arrives and then we know\n"
                "     the format giving this hash and start decoding the telegrams properly.",
                format_signature);
            return false;
        }
    }
    vector<uchar>::iterator format = format_bytes.begin();

    // 2,3 = crc for payload = hash over both DRH and data bytes. Or is it only over the data bytes?
    int ecrc2 = *(pos + 0);
    int ecrc3 = *(pos + 1);
    addExplanationAndIncrementPos(pos, 2, KindOfData::PROTOCOL, Understanding::FULL,
        "%02x%02x data crc", ecrc2, ecrc3);

    header_size = distance(frame.begin(), pos);
    int remaining = distance(pos, frame.end()) - suffix_size;

    parseDV(this, frame, pos, remaining, &dv_entries, &format, format_bytes.size());

    return true;
}

bool Telegram::parse_TPL_7A(vector<uchar>::iterator& pos)
{
    bool ok = parseShortTPL(pos);
    if (!ok) return false;

    bool decrypt_ok = potentiallyDecrypt(pos);

    header_size = distance(frame.begin(), pos);
    int remaining = distance(pos, frame.end()) - suffix_size;

    if (decrypt_ok)
    {
        parseDV(this, frame, pos, remaining, &dv_entries);
    }
    else
    {
        decryption_failed = true;
    }
    return true;
}

bool Telegram::parseTPL(vector<uchar>::iterator& pos)
{
    int remaining = distance(pos, frame.end());
    if (remaining == 0) return false;

    debug("(wmbus) parseTPL @%d %d", distance(frame.begin(), pos), remaining);

    int ci_field = *pos;
    int mfct_specific = isCiFieldManufacturerSpecific(ci_field);

    if (!isCiFieldOfType(ci_field, CI_TYPE::TPL) && !mfct_specific)
    {
        addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::NONE,
            "%02x unknown ci-field",
            ci_field);
        if (parser_warns_)
        {
            warning("(wmbus) Unknown tpl-ci-field %02x", ci_field);
        }
        return false;
    }
    tpl_ci = ci_field;
    tpl_start = pos;

    addExplanationAndIncrementPos(pos, 1, KindOfData::PROTOCOL, Understanding::FULL,
        "%02x tpl-ci-field (%s)",
        tpl_ci, ciType(tpl_ci).c_str());
    int len = ciFieldLength(tpl_ci);

    if (remaining < len + 1 && !mfct_specific) return expectedMore(__LINE__);

    switch (tpl_ci)
    {
    case CI_Field_Values::TPL_72: return parse_TPL_72(pos);
    case CI_Field_Values::TPL_78: return parse_TPL_78(pos);
    case CI_Field_Values::TPL_79: return parse_TPL_79(pos);
    case CI_Field_Values::TPL_7A: return parse_TPL_7A(pos);
    default:
    {
        // A0 to B7 are manufacturer specific.
        header_size = distance(frame.begin(), pos);
        int num_mfct_bytes = frame.end() - pos - suffix_size;
        string info = bin2hex(pos, frame.end(), num_mfct_bytes);
        info += " mfct specific";
        addExplanationAndIncrementPos(pos, num_mfct_bytes, KindOfData::CONTENT, Understanding::NONE, info.c_str());

        return true; // Manufacturer specific telegram payload. Oh well....
    }
    }

    header_size = distance(frame.begin(), pos);
    if (parser_warns_)
    {
        warning("(wmbus) Not implemented tpl-ci %02x", tpl_ci);
    }
    return false;
}

void Telegram::preProcess()
{
    DiehlAddressTransformMethod diehl_method = mustTransformDiehlAddress(frame);
    if (diehl_method != DiehlAddressTransformMethod::NONE)
    {
        debug("(diehl) preprocess necessary %s", toString(diehl_method));
        original = vector<uchar>(frame.begin(), frame.begin() + 10);
        transformDiehlAddress(frame, diehl_method);
    }
}

bool Telegram::parse(vector<uchar>& input_frame, MeterKeys* mk, bool warn)
{
    switch (about.type)
    {
    case FrameType::WMBUS: return parseWMBUS(input_frame, mk, warn);
    case FrameType::MBUS: return parseMBUS(input_frame, mk, warn);
    case FrameType::HAN: return parseHAN(input_frame, mk, warn);
    }
    assert(0);
    return false;
}

bool Telegram::parseHeader(vector<uchar>& input_frame)
{
    switch (about.type)
    {
    case FrameType::WMBUS: return parseWMBUSHeader(input_frame);
    case FrameType::MBUS: return parseMBUSHeader(input_frame);
    case FrameType::HAN: return parseHANHeader(input_frame);
    }
    assert(0);
    return false;
}

bool Telegram::parseWMBUSHeader(vector<uchar>& input_frame)
{
    assert(about.type == FrameType::WMBUS);

    bool ok;
    // Parsing the header is used to extract the ids, so that we can
    // match the telegram towards any known ids and thus keys.
    // No need to warn.
    parser_warns_ = false;
    decryption_failed = false;
    explanations.clear();
    suffix_size = 0;
    frame = input_frame;
    vector<uchar>::iterator pos = frame.begin();
    // Parsed accumulates parsed bytes.
    parsed.clear();
    // Fixes quirks from non-compliant meters to make telegram compatible with the standard
    preProcess();

    ok = parseDLL(pos);
    if (!ok) return false;

    // At the worst, only the DLL is parsed. That is fine.
    ok = parseELL(pos);
    if (!ok) return true;
    // Could not decrypt stop here.
    if (decryption_failed) return true;

    ok = parseNWL(pos);
    if (!ok) return true;

    ok = parseAFL(pos);
    if (!ok) return true;

    ok = parseTPL(pos);
    if (!ok) return true;

    return true;
}

bool Telegram::parseWMBUS(vector<uchar>& input_frame, MeterKeys* mk, bool warn)
{
    assert(about.type == FrameType::WMBUS);

    parser_warns_ = warn;
    decryption_failed = false;
    explanations.clear();
    suffix_size = 0;
    meter_keys = mk;
    assert(meter_keys != NULL);
    bool ok;
    frame = input_frame;
    vector<uchar>::iterator pos = frame.begin();
    // Parsed accumulates parsed bytes.
    parsed.clear();
    // Fixes quirks from non-compliant meters to make telegram compatible with the standard
    preProcess();
    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Parse DLL Data Link Layer for Wireless MBUS. │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseDLL(pos);
    if (!ok) return false;

    printDLL();

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Is this an ELL block?                        │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseELL(pos);
    if (!ok) return false;

    printELL();
    if (decryption_failed) return false;

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Is this an NWL block?                        │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseNWL(pos);
    if (!ok) return false;

    printNWL();

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Is this an AFL block?                        │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseAFL(pos);
    if (!ok) return false;

    printAFL();

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Is this a TPL block? It ought to be!         │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseTPL(pos);
    if (!ok) return false;

    printTPL();
    if (decryption_failed) return false;

    return true;
}

bool Telegram::parseMBUSHeader(vector<uchar>& input_frame)
{
    assert(about.type == FrameType::MBUS);

    bool ok;
    // Parsing the header is used to extract the ids, so that we can
    // match the telegram towards any known ids and thus keys.
    // No need to warn.
    parser_warns_ = false;
    decryption_failed = false;
    explanations.clear();
    suffix_size = 0;
    frame = input_frame;
    vector<uchar>::iterator pos = frame.begin();
    // Parsed accumulates parsed bytes.
    parsed.clear();

    ok = parseMBusDLLandTPL(pos);
    if (!ok) return false;

    return true;
}

bool Telegram::parseMBUS(vector<uchar>& input_frame, MeterKeys* mk, bool warn)
{
    assert(about.type == FrameType::MBUS);

    parser_warns_ = warn;
    decryption_failed = false;
    explanations.clear();
    suffix_size = 0;
    meter_keys = mk;
    assert(meter_keys != NULL);
    bool ok;
    frame = input_frame;
    vector<uchar>::iterator pos = frame.begin();
    // Parsed accumulates parsed bytes.
    parsed.clear();

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Parse DLL Data Link Layer for Wireless MBUS. │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseMBusDLLandTPL(pos);
    if (!ok) return false;

    return true;
}

bool Telegram::parseHANHeader(vector<uchar>& input_frame)
{
    assert(about.type == FrameType::HAN);

    return false;
}

bool Telegram::parseHAN(vector<uchar>& input_frame, MeterKeys* mk, bool warn)
{
    assert(about.type == FrameType::HAN);

    return false;
}

void Telegram::explainParse(string intro, int from)
{
    for (auto& p : explanations)
    {
        // Protocol or content?
        const char* c = p.kind == KindOfData::PROTOCOL ? " " : "C";
        const char* u = "?";
        if (p.understanding == Understanding::FULL) u = "!";
        if (p.understanding == Understanding::PARTIAL) u = "p";
        if (p.understanding == Understanding::ENCRYPTED) u = "E";
        if (p.understanding == Understanding::COMPRESSED) u = "C";

        // Do not print ok for understood protocol, it is implicit.
        // However if a protocol is not full understood then print p or ?.
        if (p.kind == KindOfData::PROTOCOL && p.understanding == Understanding::FULL) u = " ";

        debug("%s %03d %s%s: %s", intro.c_str(), p.pos, c, u, p.info.c_str());
    }
}

string renderAnalysisAsText(vector<Explanation>& explanations, OutputFormat of)
{
    string s;

    const char* green;
    const char* yellow;
    const char* red;
    const char* reset;

    if (of == OutputFormat::TERMINAL)
    {
        green = "\033[0;97m\033[1;42m";
        yellow = "\033[0;97m\033[0;43m";
        red = "\033[0;97m\033[0;41m\033[1;37m";
        reset = "\033[0m";
    }
    else if (of == OutputFormat::HTML)
    {
        green = "<span style=\"color:white;background-color:#008450;\">";
        yellow = "<span style=\"color:white;background-color:#efb700;\">";
        red = "<span style=\"color:white;background-color:#b81d13;\">";
        reset = "</span>";
    }
    else
    {
        green = "";
        yellow = "";
        red = "";
        reset = "";
    }

    for (auto& p : explanations)
    {
        // Protocol or content?
        const char* c = p.kind == KindOfData::PROTOCOL ? " " : "C";
        const char* u = "?";
        if (p.understanding == Understanding::FULL) u = "!";
        if (p.understanding == Understanding::PARTIAL) u = "p";
        if (p.understanding == Understanding::ENCRYPTED) u = "E";
        if (p.understanding == Understanding::COMPRESSED) u = "C";

        // Do not print ok for understood protocol, it is implicit.
        // However if a protocol is not full understood then print p or ?.
        if (p.kind == KindOfData::PROTOCOL && p.understanding == Understanding::FULL) u = " ";

        const char* pre;
        const char* post = reset;

        if (*u == '!')
        {
            pre = green;
        }
        else if (*u == 'p')
        {
            pre = yellow;
        }
        else if (*u == ' ')
        {
            pre = "";
            post = "";
        }
        else
        {
            pre = red;
        }

        s += tostrprintf("%03d %s%s: %s%s%s\n", p.pos, c, u, pre, p.info.c_str(), post);
    }
    return s;
}

string renderAnalysisAsJson(vector<Explanation>& explanations)
{
    return "{ \"TODO\": true }\n";
}

string Telegram::analyzeParse(OutputFormat format, int* content_length, int* understood_content_length)
{
    int u = 0;
    int l = 0;

    sort(explanations.begin(), explanations.end(),
        [](const Explanation& a, const Explanation& b) -> bool { return a.pos < b.pos; });

    // Calculate how much is understood.
    for (auto& e : explanations)
    {
        if (e.kind == KindOfData::CONTENT)
        {
            l += e.len;
            if (e.understanding == Understanding::PARTIAL ||
                e.understanding == Understanding::FULL)
            {
                // Its content and we have at least some understanding.
                u += e.len;
            }
        }
    }
    *content_length = l;
    *understood_content_length = u;

    switch (format)
    {
    case OutputFormat::PLAIN:
    case OutputFormat::HTML:
    case OutputFormat::TERMINAL:
    {
        return renderAnalysisAsText(explanations, format);
        break;
    }
    case OutputFormat::JSON:
        return renderAnalysisAsJson(explanations);
        break;
    case OutputFormat::NONE:
        // Do nothing
        return "";
        break;
    }
    return "ERROR";
}

void detectMeterDrivers(int manufacturer, int media, int version, std::vector<std::string>* drivers);

string Telegram::autoDetectPossibleDrivers()
{
    vector<string> drivers;
    detectMeterDrivers(dll_mfct, dll_type, dll_version, &drivers);
    if (tpl_id_found)
    {
        detectMeterDrivers(tpl_mfct, tpl_type, tpl_version, &drivers);
    }
    string possibles;
    for (string d : drivers) possibles = possibles + d + " ";
    if (possibles != "") possibles.pop_back();
    else possibles = "unknown!";

    return possibles;
}

string cType(int c_field)
{
    string s;
    if (c_field & 0x80)
    {
        s += "relayed ";
    }

    if (c_field & 0x40)
    {
        s += "from meter ";
    }
    else
    {
        s += "to meter ";
    }

    int code = c_field & 0x0f;

    switch (code) {
    case 0x0: s += "SND_NKE"; break; // to meter, link reset
    case 0x3: s += "SND_UD2"; break; // to meter, command = user data
    case 0x4: s += "SND_NR"; break; // from meter, unsolicited data, no response expected
    case 0x5: s += "SND_UD3"; break; // to multiple meters, command = user data, no response expected
    case 0x6: s += "SND_IR"; break; // from meter, installation request/data
    case 0x7: s += "ACC_NR"; break; // from meter, unsolicited offers to access the meter
    case 0x8: s += "ACC_DMD"; break; // from meter, unsolicited demand to access the meter
    case 0xa: s += "REQ_UD1"; break; // to meter, alarm request
    case 0xb: s += "REQ_UD2"; break; // to meter, data request
    }

    return s;
}

bool isValidWMBusCField(int c_field)
{
    // These are the currently seen valid C fields for wmbus telegrams.
    // 0x46 is only from an ei6500 meter.... all else is ox44
    // However in the future we might see relayed telegrams which will perhaps have
    // some other c field.
    return
        c_field == 0x44 ||
        c_field == 0x46;
}

bool isValidMBusCField(int c_field)
{
    return false;
}

string ccType(int cc_field)
{
    string s = "";
    if (cc_field & CC_B_BIDIRECTIONAL_BIT) s += "bidir ";
    if (cc_field & CC_RD_RESPONSE_DELAY_BIT) s += "fast_resp ";
    else s += "slow_resp ";
    if (cc_field & CC_S_SYNCH_FRAME_BIT) s += "sync ";
    if (cc_field & CC_R_RELAYED_BIT) s += "relayed "; // Relayed by a repeater
    if (cc_field & CC_P_HIGH_PRIO_BIT) s += "prio ";

    if (s.size() > 0 && s.back() == ' ') s.pop_back();
    return s;
}
//
//string vifType(int vif)
//{
//    // Remove any remaining 0x80 top bits.
//    vif &= 0x7f7f;
//
//    switch (vif)
//    {
//    case 0x00: return "Energy mWh";
//    case 0x01: return "Energy 10⁻² Wh";
//    case 0x02: return "Energy 10⁻¹ Wh";
//    case 0x03: return "Energy Wh";
//    case 0x04: return "Energy 10¹ Wh";
//    case 0x05: return "Energy 10² Wh";
//    case 0x06: return "Energy kWh";
//    case 0x07: return "Energy 10⁴ Wh";
//
//    case 0x08: return "Energy J";
//    case 0x09: return "Energy 10¹ J";
//    case 0x0A: return "Energy 10² J";
//    case 0x0B: return "Energy kJ";
//    case 0x0C: return "Energy 10⁴ J";
//    case 0x0D: return "Energy 10⁵ J";
//    case 0x0E: return "Energy MJ";
//    case 0x0F: return "Energy 10⁷ J";
//
//    case 0x10: return "Volume cm³";
//    case 0x11: return "Volume 10⁻⁵ m³";
//    case 0x12: return "Volume 10⁻⁴ m³";
//    case 0x13: return "Volume l";
//    case 0x14: return "Volume 10⁻² m³";
//    case 0x15: return "Volume 10⁻¹ m³";
//    case 0x16: return "Volume m³";
//    case 0x17: return "Volume 10¹ m³";
//
//    case 0x18: return "Mass g";
//    case 0x19: return "Mass 10⁻² kg";
//    case 0x1A: return "Mass 10⁻¹ kg";
//    case 0x1B: return "Mass kg";
//    case 0x1C: return "Mass 10¹ kg";
//    case 0x1D: return "Mass 10² kg";
//    case 0x1E: return "Mass t";
//    case 0x1F: return "Mass 10⁴ kg";
//
//    case 0x20: return "On time seconds";
//    case 0x21: return "On time minutes";
//    case 0x22: return "On time hours";
//    case 0x23: return "On time days";
//
//    case 0x24: return "Operating time seconds";
//    case 0x25: return "Operating time minutes";
//    case 0x26: return "Operating time hours";
//    case 0x27: return "Operating time days";
//
//    case 0x28: return "Power mW";
//    case 0x29: return "Power 10⁻² W";
//    case 0x2A: return "Power 10⁻¹ W";
//    case 0x2B: return "Power W";
//    case 0x2C: return "Power 10¹ W";
//    case 0x2D: return "Power 10² W";
//    case 0x2E: return "Power kW";
//    case 0x2F: return "Power 10⁴ W";
//
//    case 0x30: return "Power J/h";
//    case 0x31: return "Power 10¹ J/h";
//    case 0x32: return "Power 10² J/h";
//    case 0x33: return "Power kJ/h";
//    case 0x34: return "Power 10⁴ J/h";
//    case 0x35: return "Power 10⁵ J/h";
//    case 0x36: return "Power MJ/h";
//    case 0x37: return "Power 10⁷ J/h";
//
//    case 0x38: return "Volume flow cm³/h";
//    case 0x39: return "Volume flow 10⁻⁵ m³/h";
//    case 0x3A: return "Volume flow 10⁻⁴ m³/h";
//    case 0x3B: return "Volume flow l/h";
//    case 0x3C: return "Volume flow 10⁻² m³/h";
//    case 0x3D: return "Volume flow 10⁻¹ m³/h";
//    case 0x3E: return "Volume flow m³/h";
//    case 0x3F: return "Volume flow 10¹ m³/h";
//
//    case 0x40: return "Volume flow ext. 10⁻⁷ m³/min";
//    case 0x41: return "Volume flow ext. cm³/min";
//    case 0x42: return "Volume flow ext. 10⁻⁵ m³/min";
//    case 0x43: return "Volume flow ext. 10⁻⁴ m³/min";
//    case 0x44: return "Volume flow ext. l/min";
//    case 0x45: return "Volume flow ext. 10⁻² m³/min";
//    case 0x46: return "Volume flow ext. 10⁻¹ m³/min";
//    case 0x47: return "Volume flow ext. m³/min";
//
//    case 0x48: return "Volume flow ext. mm³/s";
//    case 0x49: return "Volume flow ext. 10⁻⁸ m³/s";
//    case 0x4A: return "Volume flow ext. 10⁻⁷ m³/s";
//    case 0x4B: return "Volume flow ext. cm³/s";
//    case 0x4C: return "Volume flow ext. 10⁻⁵ m³/s";
//    case 0x4D: return "Volume flow ext. 10⁻⁴ m³/s";
//    case 0x4E: return "Volume flow ext. l/s";
//    case 0x4F: return "Volume flow ext. 10⁻² m³/s";
//
//    case 0x50: return "Mass g/h";
//    case 0x51: return "Mass 10⁻² kg/h";
//    case 0x52: return "Mass 10⁻¹ kg/h";
//    case 0x53: return "Mass kg/h";
//    case 0x54: return "Mass 10¹ kg/h";
//    case 0x55: return "Mass 10² kg/h";
//    case 0x56: return "Mass t/h";
//    case 0x57: return "Mass 10⁴ kg/h";
//
//    case 0x58: return "Flow temperature 10⁻³ °C";
//    case 0x59: return "Flow temperature 10⁻² °C";
//    case 0x5A: return "Flow temperature 10⁻¹ °C";
//    case 0x5B: return "Flow temperature °C";
//
//    case 0x5C: return "Return temperature 10⁻³ °C";
//    case 0x5D: return "Return temperature 10⁻² °C";
//    case 0x5E: return "Return temperature 10⁻¹ °C";
//    case 0x5F: return "Return temperature °C";
//
//    case 0x60: return "Temperature difference 10⁻³ K/°C";
//    case 0x61: return "Temperature difference 10⁻² K/°C";
//    case 0x62: return "Temperature difference 10⁻¹ K/°C";
//    case 0x63: return "Temperature difference K/°C";
//
//    case 0x64: return "External temperature 10⁻³ °C";
//    case 0x65: return "External temperature 10⁻² °C";
//    case 0x66: return "External temperature 10⁻¹ °C";
//    case 0x67: return "External temperature °C";
//
//    case 0x68: return "Pressure mbar";
//    case 0x69: return "Pressure 10⁻² bar";
//    case 0x6A: return "Pressure 10⁻¹ bar";
//    case 0x6B: return "Pressure bar";
//
//    case 0x6C: return "Date type G";
//    case 0x6D: return "Date and time type";
//
//    case 0x6E: return "Units for H.C.A.";
//    case 0x6F: return "Third extension 6F of VIF-codes";
//
//    case 0x70: return "Averaging duration seconds";
//    case 0x71: return "Averaging duration minutes";
//    case 0x72: return "Averaging duration hours";
//    case 0x73: return "Averaging duration days";
//
//    case 0x74: return "Actuality duration seconds";
//    case 0x75: return "Actuality duration minutes";
//    case 0x76: return "Actuality duration hours";
//    case 0x77: return "Actuality duration days";
//
//    case 0x78: return "Fabrication no";
//    case 0x79: return "Enhanced identification";
//
//    case 0x7B: return "First extension FB of VIF-codes";
//    case 0x7C: return "VIF in following string (length in first byte)";
//    case 0x7D: return "Second extension FD of VIF-codes";
//
//    case 0x7E: return "Any VIF";
//    case 0x7F: return "Manufacturer specific";
//
//    case 0x7B00: return "Active Energy 0.1 MWh";
//    case 0x7B01: return "Active Energy 1 MWh";
//
//    case 0x7B1A: return "Relative humidity 0.1%";
//    case 0x7B1B: return "Relative humidity 1%";
//
//    default: return "?";
//    }
//}
//
//string vifKey(int vif)
//{
//    int t = vif & 0x7f;
//
//    switch (t) {
//
//    case 0x00:
//    case 0x01:
//    case 0x02:
//    case 0x03:
//    case 0x04:
//    case 0x05:
//    case 0x06:
//    case 0x07: return "energy";
//
//    case 0x08:
//    case 0x09:
//    case 0x0A:
//    case 0x0B:
//    case 0x0C:
//    case 0x0D:
//    case 0x0E:
//    case 0x0F: return "energy";
//
//    case 0x10:
//    case 0x11:
//    case 0x12:
//    case 0x13:
//    case 0x14:
//    case 0x15:
//    case 0x16:
//    case 0x17: return "volume";
//
//    case 0x18:
//    case 0x19:
//    case 0x1A:
//    case 0x1B:
//    case 0x1C:
//    case 0x1D:
//    case 0x1E:
//    case 0x1F: return "mass";
//
//    case 0x20:
//    case 0x21:
//    case 0x22:
//    case 0x23: return "on_time";
//
//    case 0x24:
//    case 0x25:
//    case 0x26:
//    case 0x27: return "operating_time";
//
//    case 0x28:
//    case 0x29:
//    case 0x2A:
//    case 0x2B:
//    case 0x2C:
//    case 0x2D:
//    case 0x2E:
//    case 0x2F: return "power";
//
//    case 0x30:
//    case 0x31:
//    case 0x32:
//    case 0x33:
//    case 0x34:
//    case 0x35:
//    case 0x36:
//    case 0x37: return "power";
//
//    case 0x38:
//    case 0x39:
//    case 0x3A:
//    case 0x3B:
//    case 0x3C:
//    case 0x3D:
//    case 0x3E:
//    case 0x3F: return "volume_flow";
//
//    case 0x40:
//    case 0x41:
//    case 0x42:
//    case 0x43:
//    case 0x44:
//    case 0x45:
//    case 0x46:
//    case 0x47: return "volume_flow_ext";
//
//    case 0x48:
//    case 0x49:
//    case 0x4A:
//    case 0x4B:
//    case 0x4C:
//    case 0x4D:
//    case 0x4E:
//    case 0x4F: return "volume_flow_ext";
//
//    case 0x50:
//    case 0x51:
//    case 0x52:
//    case 0x53:
//    case 0x54:
//    case 0x55:
//    case 0x56:
//    case 0x57: return "mass_flow";
//
//    case 0x58:
//    case 0x59:
//    case 0x5A:
//    case 0x5B: return "flow_temperature";
//
//    case 0x5C:
//    case 0x5D:
//    case 0x5E:
//    case 0x5F: return "return_temperature";
//
//    case 0x60:
//    case 0x61:
//    case 0x62:
//    case 0x63: return "temperature_difference";
//
//    case 0x64:
//    case 0x65:
//    case 0x66:
//    case 0x67: return "external_temperature";
//
//    case 0x68:
//    case 0x69:
//    case 0x6A:
//    case 0x6B: return "pressure";
//
//    case 0x6C: return "date"; // Date type G
//    case 0x6E: return "hca"; // Units for H.C.A.
//    case 0x6F: return "reserved"; // Reserved
//
//    case 0x70:
//    case 0x71:
//    case 0x72:
//    case 0x73: return "average_duration";
//
//    case 0x74:
//    case 0x75:
//    case 0x76:
//    case 0x77: return "actual_duration";
//
//    case 0x78: return "fabrication_no"; // Fabrication no
//    case 0x79: return "enhanced_identification"; // Enhanced identification
//
//    case 0x7C: // VIF in following string (length in first byte)
//    case 0x7E: // Any VIF
//    case 0x7F: // Manufacturer specific
//
//    default: warning("(wmbus) warning: generic type %d cannot be scaled!", t);
//        return "unknown";
//    }
//}

string vifUnit(int vif)
{
    int t = vif & 0x7f;

    switch (t) {

    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07: return "kwh";

    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F: return "MJ";

    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17: return "m3";

    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F: return "kg";

    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27: return "h";

    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F: return "kw";

    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37: return "MJ";

    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F: return "m3/h";

    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47: return "m3/h";

    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F: return "m3/h";

    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57: return "kg/h";

    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B: return "c";

    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x5F: return "c";

    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63: return "k";

    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67: return "c";

    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B: return "bar";

    case 0x6C: return ""; // Date type G
    case 0x6D: return ""; // ??
    case 0x6E: return ""; // Units for H.C.A.
    case 0x6F: return ""; // Reserved

    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73: return "h";

    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77: return "h";

    case 0x78: return ""; // Fabrication no
    case 0x79: return ""; // Enhanced identification

    case 0x7C: // VIF in following string (length in first byte)
    case 0x7E: // Any VIF
    case 0x7F: // Manufacturer specific

    default: warning("(wmbus) warning: generic type %d cannot be scaled!", t);
        return "unknown";
    }
}

double toDoubleFromBytes(vector<uchar>& bytes, int len) {
    double d = 0;
    for (int i = 0; i < len; ++i) {
        double x = bytes[i];
        d += x * (256 ^ i);
    }
    return d;
}

double toDoubleFromBCD(vector<uchar>& bytes, int len) {
    double d = 0;
    for (int i = 0; i < len; ++i) {
        double x = bytes[i] & 0xf;
        d += x * (10 ^ (i * 2));
        double xx = bytes[i] >> 4;
        d += xx * (10 ^ (1 + i * 2));
    }
    return d;
}

double dataAsDouble(int dif, int vif, int vife, string data)
{
    vector<uchar> bytes;
    hex2bin(data, &bytes);

    int t = dif & 0x0f;
    switch (t) {
    case 0x0: return 0.0;
    case 0x1: return toDoubleFromBytes(bytes, 1);
    case 0x2: return toDoubleFromBytes(bytes, 2);
    case 0x3: return toDoubleFromBytes(bytes, 3);
    case 0x4: return toDoubleFromBytes(bytes, 4);
    case 0x5: return -1;  //  How is REAL stored?
    case 0x6: return toDoubleFromBytes(bytes, 6);
        // Note that for 64 bit data, storing it into a double might lose precision
        // since the mantissa is less than 64 bit. It is unlikely that anyone
        // really needs true 64 bit precision in their measurements from a physical meter though.
    case 0x7: return toDoubleFromBytes(bytes, 8);
    case 0x8: return -1; // Selection for Readout?
    case 0x9: return toDoubleFromBCD(bytes, 1);
    case 0xA: return toDoubleFromBCD(bytes, 2);
    case 0xB: return toDoubleFromBCD(bytes, 3);
    case 0xC: return toDoubleFromBCD(bytes, 4);
    case 0xD: return -1; // variable length
    case 0xE: return toDoubleFromBCD(bytes, 6);
    case 0xF: return -1; // Special Functions
    }
    return -1;
}

uint64_t dataAsUint64(int dif, int vif, int vife, string data)
{
    vector<uchar> bytes;
    hex2bin(data, &bytes);

    int t = dif & 0x0f;
    switch (t) {
    case 0x0: return 0.0;
    case 0x1: return toDoubleFromBytes(bytes, 1);
    case 0x2: return toDoubleFromBytes(bytes, 2);
    case 0x3: return toDoubleFromBytes(bytes, 3);
    case 0x4: return toDoubleFromBytes(bytes, 4);
    case 0x5: return -1;  //  How is REAL stored?
    case 0x6: return toDoubleFromBytes(bytes, 6);
        // Note that for 64 bit data, storing it into a double might lose precision
        // since the mantissa is less than 64 bit. It is unlikely that anyone
        // really needs true 64 bit precision in their measurements from a physical meter though.
    case 0x7: return toDoubleFromBytes(bytes, 8);
    case 0x8: return -1; // Selection for Readout?
    case 0x9: return toDoubleFromBCD(bytes, 1);
    case 0xA: return toDoubleFromBCD(bytes, 2);
    case 0xB: return toDoubleFromBCD(bytes, 3);
    case 0xC: return toDoubleFromBCD(bytes, 4);
    case 0xD: return -1; // variable length
    case 0xE: return toDoubleFromBCD(bytes, 6);
    case 0xF: return -1; // Special Functions
    }
    return -1;
}


bool Telegram::findFormatBytesFromKnownMeterSignatures(vector<uchar>* format_bytes)
{
    bool ok = true;
    if (format_signature == 0xa8ed)
    {
        hex2bin("02FF2004134413615B6167", format_bytes);
        debug("(wmbus) using hard coded format for hash a8ed");
    }
    else if (format_signature == 0xc412)
    {
        hex2bin("02FF20041392013BA1015B8101E7FF0F", format_bytes);
        debug("(wmbus) using hard coded format for hash c412");
    }
    else if (format_signature == 0x61eb)
    {
        hex2bin("02FF2004134413A1015B8101E7FF0F", format_bytes);
        debug("(wmbus) using hard coded format for hash 61eb");
    }
    else if (format_signature == 0xd2f7)
    {
        hex2bin("02FF2004134413615B5167", format_bytes);
        debug("(wmbus) using hard coded format for hash d2f7");
    }
    else if (format_signature == 0xdd34)
    {
        hex2bin("02FF2004134413", format_bytes);
        debug("(wmbus) using hard coded format for hash dd34");
    }
    else if (format_signature == 0x7c0e)
    {
        hex2bin("02FF200413523B", format_bytes);
        debug("(wmbus) using hard coded format for hash 7c0e");
    }
    else
    {
        ok = false;
    }
    return ok;
}

bool handleTelegram(AboutTelegram& about, vector<uchar> frame)
{
    verbose("(wmbus) incide wmbus.cc");
    bool handled = false;

    assert(frame.size() > 0);

    if (about.type == FrameType::MBUS && frame.size() == 1)
    {
        if (frame[0] == 0xe5)
        {
            // Ack from meter, currently ignored.
            return true;
        }
        // Something else that we currently do not understand.
        return false;
    }

    if (about.type == FrameType::WMBUS)
    {
        size_t expected_len = frame[0] + 1;
        if (frame.size() > 0 && expected_len != frame.size())
        {
            warning("(wmbus) telegram length byte (the first) 0x%02x (%d) is probably wrong. Expected 0x%02x (%zu) based on the length of the telegram.",
                frame[0], frame[0], frame.size() - 1, frame.size() - 1);
        }
    }

   /* for (auto f : telegram_listeners_)
    {
        if (f)
        {
            bool h = f(about, frame);
            if (h) handled = true;
        }
    }*/

    return handled;
}

int toInt(TPLSecurityMode tsm)
{
    switch (tsm) {

#define X(name,nr) case TPLSecurityMode::name : return nr;
        LIST_OF_TPL_SECURITY_MODES
#undef X
    }

    return 16;
}

int toInt(ELLSecurityMode esm)
{
    switch (esm) {

#define X(name,nr) case ELLSecurityMode::name : return nr;
        LIST_OF_ELL_SECURITY_MODES
#undef X
    }

    return 2;
}


void Telegram::extractMfctData(vector<uchar>* pl)
{
    pl->clear();
    if (mfct_0f_index == -1) return;

    vector<uchar>::iterator from = frame.begin() + header_size + mfct_0f_index;
    vector<uchar>::iterator to = frame.end() - suffix_size;
    pl->insert(pl->end(), from, to);
}

void Telegram::extractPayload(vector<uchar>* pl)
{
    pl->clear();
    vector<uchar>::iterator from = frame.begin() + header_size;
    vector<uchar>::iterator to = frame.end() - suffix_size;
    pl->insert(pl->end(), from, to);
}

void Telegram::extractFrame(vector<uchar>* fr)
{
    *fr = frame;
}

int toInt(AFLAuthenticationType aat)
{
    switch (aat) {

#define X(name,nr,len) case AFLAuthenticationType::name : return nr;
        LIST_OF_AFL_AUTH_TYPES
#undef X
    }

    return 16;
}

bool trimCRCsFrameFormatAInternal(std::vector<uchar>& payload, bool fail_is_ok)
{
    if (payload.size() < 12) {
        if (!fail_is_ok)
        {
            debug("(wmbus) not enough bytes! expected at least 12 but got (%zu)!", payload.size());
        }
        return false;
    }
    size_t len = payload.size();
    if (!fail_is_ok)
    {
        debug("(wmbus) trimming frame A", payload);
    }

    vector<uchar> out;

    uint16_t calc_crc = crc16_EN13757(safeButUnsafeVectorPtr(payload), 10);
    uint16_t check_crc = payload[10] << 8 | payload[11];

    if (calc_crc != check_crc && !FUZZING)
    {
        if (!fail_is_ok)
        {
            debug("(wmbus) ff a dll crc first (calculated %04x) did not match (expected %04x) for bytes 0-%zu!", calc_crc, check_crc, 10);
        }
        return false;
    }
    out.insert(out.end(), payload.begin(), payload.begin() + 10);
    if (!fail_is_ok)
    {
        debug("(wmbus) ff a dll crc 0-%zu %04x ok", 10 - 1, calc_crc);
    }

    size_t pos = 12;
    for (pos = 12; pos + 18 <= len; pos += 18)
    {
        size_t to = pos + 16;
        calc_crc = crc16_EN13757(&payload[pos], 16);
        check_crc = payload[to] << 8 | payload[to + 1];
        if (calc_crc != check_crc && !FUZZING)
        {
            if (!fail_is_ok)
            {
                debug("(wmbus) ff a dll crc mid (calculated %04x) did not match (expected %04x) for bytes %zu-%zu!",
                    calc_crc, check_crc, pos, to - 1);
            }
            return false;
        }
        out.insert(out.end(), payload.begin() + pos, payload.begin() + pos + 16);
        if (!fail_is_ok)
        {
            debug("(wmbus) ff a dll crc mid %zu-%zu %04x ok", pos, to - 1, calc_crc);
        }
    }

    if (pos < len - 2)
    {
        size_t tto = len - 2;
        size_t blen = (tto - pos);
        calc_crc = crc16_EN13757(&payload[pos], blen);
        check_crc = payload[tto] << 8 | payload[tto + 1];
        if (calc_crc != check_crc && !FUZZING)
        {
            if (!fail_is_ok)
            {
                debug("(wmbus) ff a dll crc final (calculated %04x) did not match (expected %04x) for bytes %zu-%zu!",
                    calc_crc, check_crc, pos, tto - 1);
            }
            return false;
        }
        out.insert(out.end(), payload.begin() + pos, payload.begin() + tto);
        if (!fail_is_ok)
        {
            debug("(wmbus) ff a dll crc final %zu-%zu %04x ok", pos, tto - 1, calc_crc);
        }
    }

    debug("(wmbus) trimming frame A", payload);

    out[0] = out.size() - 1;
    size_t new_len = out[0] + 1;
    size_t old_size = payload.size();
    payload = out;
    size_t new_size = payload.size();

    debug("(wmbus) trimmed %zu dll crc bytes from frame a and ignored %zu suffix bytes.", (len - new_len), (old_size - new_size) - (len - new_len));
    debug("(wmbus) trimmed frame A", payload);

    return true;
}

bool trimCRCsFrameFormatBInternal(std::vector<uchar>& payload, bool fail_is_ok)
{
    if (payload.size() < 12) {
        if (!fail_is_ok)
        {
            debug("(wmbus) not enough bytes! expected at least 12 but got (%zu)!", payload.size());
        }
        return false;
    }
    size_t len = payload.size();
    if (!fail_is_ok)
    {
        debug("(wmbus) trimming frame B", payload);
    }

    vector<uchar> out;
    size_t crc1_pos, crc2_pos;
    if (len <= 128)
    {
        crc1_pos = len - 2;
        crc2_pos = 0;
    }
    else
    {
        crc1_pos = 126;
        crc2_pos = len - 2;
    }

    uint16_t calc_crc = crc16_EN13757(safeButUnsafeVectorPtr(payload), crc1_pos);
    uint16_t check_crc = payload[crc1_pos] << 8 | payload[crc1_pos + 1];

    if (calc_crc != check_crc && !FUZZING)
    {
        if (!fail_is_ok)
        {
            debug("(wmbus) ff b dll crc (calculated %04x) did not match (expected %04x) for bytes 0-%zu!", calc_crc, check_crc, crc1_pos);
        }
        return false;
    }

    out.insert(out.end(), payload.begin(), payload.begin() + crc1_pos);
    if (!fail_is_ok)
    {
        debug("(wmbus) ff b dll crc first 0-%zu %04x ok", crc1_pos, calc_crc);
    }

    if (crc2_pos > 0)
    {
        calc_crc = crc16_EN13757(&payload[crc1_pos + 2], crc2_pos);
        check_crc = payload[crc2_pos] << 8 | payload[crc2_pos + 1];

        if (calc_crc != check_crc && !FUZZING)
        {
            if (!fail_is_ok)
            {
                debug("(wmbus) ff b dll crc (calculated %04x) did not match (expected %04x) for bytes %zu-%zu!",
                    calc_crc, check_crc, crc1_pos + 2, crc2_pos);
            }
            return false;
        }

        out.insert(out.end(), payload.begin() + crc1_pos + 2, payload.begin() + crc2_pos);
        if (!fail_is_ok)
        {
            debug("(wmbus) ff b dll crc final %zu-%zu %04x ok", crc1_pos + 2, crc2_pos, calc_crc);
        }
    }

    debug("(wmbus) trimming frame B", payload);

    out[0] = out.size() - 1;
    size_t new_len = out[0] + 1;
    size_t old_size = payload.size();
    payload = out;
    size_t new_size = payload.size();

    debug("(wmbus) trimmed %zu dll crc bytes from frame b and ignored %zu suffix bytes.", (len - new_len), (old_size - new_size) - (len - new_len));
    debug("(wmbus) trimmed frame B", payload);

    return true;
}

void removeAnyDLLCRCs(std::vector<uchar>& payload)
{
    bool trimmed = trimCRCsFrameFormatAInternal(payload, true);
    if (!trimmed) trimCRCsFrameFormatBInternal(payload, true);
}

bool trimCRCsFrameFormatA(std::vector<uchar>& payload)
{
    return trimCRCsFrameFormatAInternal(payload, false);
}

bool trimCRCsFrameFormatB(std::vector<uchar>& payload)
{
    return trimCRCsFrameFormatBInternal(payload, false);
}

FrameStatus checkWMBusFrame(vector<uchar>& data,
    size_t* frame_length,
    int* payload_len_out,
    int* payload_offset,
    bool only_test)
{
    // Nice clean: 2A442D2C998734761B168D2021D0871921|58387802FF2071000413F81800004413F8180000615B
    // Ugly: 00615B2A442D2C998734761B168D2021D0871921|58387802FF2071000413F81800004413F8180000615B
    // Here the frame is prefixed with some random data.

    debug("(wmbus) checkWMBUSFrame", data);

    if (data.size() < 11)
    {
        debug("(wmbus) less than 11 bytes, partial frame");
        return PartialFrame;
    }
    int payload_len = data[0];
    int type = data[1];
    int offset = 1;

    if (data[0] == 0x68 && data[3] == 0x68 && data[1] == data[2])
    {
        // Ooups this is not a wmbus frame. Its clearly an mbus frame.
        return PartialFrame;
    }
    if (!isValidWMBusCField(type))
    {
        // Ouch, we are out of sync with the wmbus frames that we expect!
        // Since we currently do not handle any other type of frame, we can
        // look for a valid c field (ie 0x44 0x46 etc) in the buffer.
        // If we find such a byte and the length byte before maps to the end
        // of the buffer, then we have found a valid telegram.
        bool found = false;
        for (size_t i = 0; i < data.size() - 2; ++i)
        {
            if (isValidWMBusCField(data[i + 1]))
            {
                payload_len = data[i];
                size_t remaining = data.size() - i;
                if (data[i] + 1 == (uchar)remaining && data[i + 1] == 0x44)
                {
                    found = true;
                    offset = i + 1;
                    verbose("(wmbus) out of sync, skipping %d bytes.", (int)i);
                    break;
                }
            }
        }
        if (!found)
        {
            // No sensible telegram in the buffer. Flush it!
            if (!only_test)
            {
                verbose("(wmbus) no sensible telegram found, clearing buffer.");
                data.clear();
            }
            else
            {
                debug("(wmbus) not a proper wmbus frame.");
            }
            return ErrorInFrame;
        }
    }
    *payload_len_out = payload_len;
    *payload_offset = offset;
    *frame_length = payload_len + offset;
    if (data.size() < *frame_length)
    {
        // Not enough bytes for this payload_len....
        if (only_test)
        {
            // This is used from simulate files and hex in command line and analyze.
            // Lets be lax and just adjust the length to what is available.
            payload_len = data.size() - offset;
            *payload_len_out = payload_len;
            *frame_length = payload_len + offset;
            warning("(wmbus) not enough bytes, frame length byte changed from %d(%02x) to %d(%02x)!",
                data[offset - 1], data[offset - 1],
                payload_len, payload_len);
            data[offset - 1] = payload_len;

            return FullFrame;
        }
        debug("(wmbus) not enough bytes, partial frame %d %d", data.size(), *frame_length);
        return PartialFrame;
    }

    if (!only_test)
    {
        debug("(wmbus) received full frame.");
    }
    return FullFrame;
}

bool is_command(string b, string* cmd)
{
    // Check if CMD(.)
    if (b.length() < 6) return false;
    if (b.rfind("CMD(", 0) != 0) return false;
    if (b.back() != ')') return false;
    *cmd = b.substr(4, b.length() - 5);
    return true;
}

const char* toString(TelegramFormat format)
{
    if (format == TelegramFormat::WMBUS_C_FIELD) return "wmbus_c_field";
    if (format == TelegramFormat::WMBUS_CI_FIELD) return "wmbus_ci_field";
    if (format == TelegramFormat::MBUS_SHORT_FRAME) return "mbus_short_frame";
    if (format == TelegramFormat::MBUS_LONG_FRAME) return "mbus_long_frame";

    return "unknown";
}

TelegramFormat toTelegramFormat(const char* s)
{
    if (!strcmp(s, "wmbus_c_field")) return TelegramFormat::WMBUS_C_FIELD;
    if (!strcmp(s, "wmbus_ci_field")) return TelegramFormat::WMBUS_CI_FIELD;
    if (!strcmp(s, "mbus_short_frame")) return TelegramFormat::MBUS_SHORT_FRAME;
    if (!strcmp(s, "mbus_long_frame")) return TelegramFormat::MBUS_LONG_FRAME;

    return TelegramFormat::UNKNOWN;
}

const char* toString(FrameType ft)
{
    switch (ft) {
    case FrameType::WMBUS: return "wmbus";
    case FrameType::MBUS: return "mbus";
    case FrameType::HAN: return "han";
    }
    return "?";
}

int genericifyMedia(int media)
{
    if (media == 0x06 || // Warm Water (30°C-90°C) meter
        media == 0x07 || // Water meter
        media == 0x15 || // Hot water (>=90°C) meter
        media == 0x16 || // Cold water meter
        media == 0x28)   // Waste water
    {
        return 0x07; // Return plain water
    }
    return media;
}

bool isCloseEnough(int media1, int media2)
{
    return genericifyMedia(media1) == genericifyMedia(media2);
}
