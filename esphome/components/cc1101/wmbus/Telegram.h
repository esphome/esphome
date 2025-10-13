#pragma once
#include <vector>
#include <string>
#include "types.h"
#include <set>
#include <map>
#include <string>
#include "utils.h"
#include"address.h"

using namespace wmbusmeters;

struct AboutTelegram
{
    // wmbus device used to receive this telegram.
    std::string device;
    // The device's opinion of the rssi, best effort conversion into the dbm scale.
    // -100 dbm = 0.1 pico Watt to -20 dbm = 10 micro W
    // Measurements smaller than -100 and larger than -10 are unlikely.
    int rssi_dbm{};
    // WMBus or MBus
    FrameType type{};
    // time the telegram was received
    time_t timestamp;

    AboutTelegram(std::string dv, int rs, FrameType t, time_t ts = 0) : device(dv), rssi_dbm(rs), type(t), timestamp(ts) {}
    AboutTelegram() {}
};

struct MeterKeys
{
    std::vector<uchar> confidentiality_key;
   std::vector<uchar> authentication_key;

    bool hasConfidentialityKey() { return confidentiality_key.size() > 0; }
    bool hasAuthenticationKey() { return authentication_key.size() > 0; }
};


struct Explanation
{
    int pos{};
    int len{};
    std::string info;
    KindOfData kind{};
    Understanding understanding{};

    Explanation(int p, int l, const std::string& i, KindOfData k, Understanding u) :
        pos(p), len(l), info(i), kind(k), understanding(u) {}
};

enum class OutputFormat
{
    NONE, PLAIN, TERMINAL, JSON, HTML
};


struct Telegram
{
private:
    Telegram(Telegram& t) { }

public:
    Telegram() = default;

    AboutTelegram about;

    // If set to true then this telegram should be trigger updates.
    bool discard{};

    // If a warning is printed mark this.
    bool triggered_warning{};

    // The different addresses found,
    // the first is the dll_id_mvt, ell_id_mvt, nwl_id_mvt, and the last is the tpl_id_mvt.
    std::vector<Address> addresses;

    // If decryption failed, set this to true, to prevent further processing.
    bool decryption_failed{};

    // DLL
    int dll_len{}; // The length of the telegram, 1 byte.
    int dll_c{};   // 1 byte control code, SND_NR=0x44

    uchar dll_mfct_b[2]; //  2 bytes
    int dll_mfct{};

    uchar mbus_primary_address; // Single byte address 0-250 for mbus devices.
    uchar mbus_ci; // MBus control information field.

    std::vector<uchar> dll_a; // A field 6 bytes
    // The 6 a field bytes are composed of 4 id bytes, version and type.
    uchar dll_id_b[4]{};    // 4 bytes, address in BCD = 8 decimal 00000000...99999999 digits.
    std::vector<uchar> dll_id; // 4 bytes, human readable order.
    uchar dll_version{}; // 1 byte
    uchar dll_type{}; // 1 byte

    // ELL
    uchar ell_ci{}; // 1 byte
    uchar ell_cc{}; // 1 byte
    uchar ell_acc{}; // 1 byte
    uchar ell_sn_b[4]{}; // 4 bytes
    int   ell_sn{}; // 4 bytes
    uchar ell_sn_session{}; // 4 bits
    int   ell_sn_time{}; // 25 bits
    uchar ell_sn_sec{}; // 3 bits
    ELLSecurityMode ell_sec_mode{}; // Based on 3 bits from above.
    uchar ell_pl_crc_b[2]{}; // 2 bytes
    uint16_t ell_pl_crc{}; // 2 bytes

    uchar ell_mfct_b[2]{}; // 2 bytes;
    int   ell_mfct{};
    bool  ell_id_found{};
    uchar ell_id_b[6]{}; // 4 bytes;
    uchar ell_version{}; // 1 byte
    uchar ell_type{};  // 1 byte

    // NWL
    int nwl_ci{}; // 1 byte

    // AFL
    uchar afl_ci{}; // 1 byte
    uchar afl_len{}; // 1 byte
    uchar afl_fc_b[2]{}; // 2 byte fragmentation control
    uint16_t afl_fc{};
    uchar afl_mcl{}; // 1 byte message control

    bool afl_ki_found{};
    uchar afl_ki_b[2]{}; // 2 byte key information
    uint16_t afl_ki{};

    bool afl_counter_found{};
    uchar afl_counter_b[4]{}; // 4 bytes
    uint32_t afl_counter{};

    bool afl_mlen_found{};
    int afl_mlen{};

    bool must_check_mac{};
    std::vector<uchar> afl_mac_b;

    // TPL
    std::vector<uchar>::iterator tpl_start;
    int tpl_ci{}; // 1 byte
    int tpl_acc{}; // 1 byte
    int tpl_sts{}; // 1 byte
    int tpl_sts_offset{}; // Remember where the sts field is in the telegram, so
    // that we can add more vendor specific decodings to it.
    int tpl_cfg{}; // 2 bytes
    TPLSecurityMode tpl_sec_mode{}; // Based on 5 bits extracted from cfg.
    int tpl_num_encr_blocks{};
    int tpl_cfg_ext{}; // 1 byte
    int tpl_kdf_selection{}; // 1 byte
    std::vector<uchar> tpl_generated_key; // 16 bytes
    std::vector<uchar> tpl_generated_mac_key; // 16 bytes

    bool  tpl_id_found{}; // If set to true, then tpl_id_b contains valid values.
    std::vector<uchar> tpl_a; // A field 6 bytes
    // The 6 a field bytes are composed of 4 id bytes, version and type.
    uchar tpl_id_b[4]{}; // 4 bytes
    uchar tpl_mfct_b[2]{}; // 2 bytes
    int   tpl_mfct{};
    uchar tpl_version{}; // 1 bytes
    uchar tpl_type{}; // 1 bytes

    // The format signature is used for compact frames.
    int format_signature{};

    std::vector<uchar> frame; // Content of frame, potentially decrypted.
    std::vector<uchar> parsed;  // Parsed bytes with explanations.
    int header_size{}; // Size of headers before the APL content.
    int suffix_size{}; // Size of suffix after the APL content. Usually empty, but can be MACs!
    int mfct_0f_index = -1; // -1 if not found, else index of the 0f byte, if found, inside the difvif data after the header.
    int force_mfct_index = -1; // Force all data after this offset to be mfct specific. Used for meters not using 0f.
    void extractFrame(std::vector<uchar>* fr); // Extract to full frame.
    void extractPayload(std::vector<uchar>* pl); // Extract frame data containing the measurements, after the header and not the suffix.
    void extractMfctData(std::vector<uchar>* pl); // Extract frame data after the DIF 0x0F.

    bool handled{}; // Set to true, when a meter has accepted the telegram.

    bool parseHeader(std::vector<uchar>& input_frame);
    bool parse(std::vector<uchar>& input_frame, MeterKeys* mk, bool warn);

    bool parseMBUSHeader(std::vector<uchar>& input_frame);
    bool parseMBUS(std::vector<uchar>& input_frame, MeterKeys* mk, bool warn);

    bool parseWMBUSHeader(std::vector<uchar>& input_frame);
    bool parseWMBUS(std::vector<uchar>& input_frame, MeterKeys* mk, bool warn);

    bool parseHANHeader(std::vector<uchar>& input_frame);
    bool parseHAN(std::vector<uchar>& input_frame, MeterKeys* mk, bool warn);

    void addAddressMfctFirst(const std::vector<uchar>::iterator &pos);
    void addAddressIdFirst(const std::vector<uchar>::iterator &pos);

    void print();

    // A vector of indentations and explanations, to be printed
    // below the raw data bytes to explain the telegram content.
    std::vector<Explanation> explanations;
    void addExplanationAndIncrementPos(std::vector<uchar>::iterator& pos, int len, KindOfData k, Understanding u, const char* fmt, ...);
    void setExplanation(std::vector<uchar>::iterator& pos, int len, KindOfData k, Understanding u, const char* fmt, ...);
    void addMoreExplanation(int pos, const char* fmt, ...);
    void addMoreExplanation(int pos, std::string json);

    // Add an explanation of data inside manufacturer specific data.
    void addSpecialExplanation(int offset, int len, KindOfData k, Understanding u, const char* fmt, ...);
    void explainParse(std::string intro, int from);
    std::string analyzeParse(OutputFormat o, int* content_length, int* understood_content_length);

    bool parserWarns() { return parser_warns_; }
    bool isSimulated() { return is_simulated_; }
    bool beingAnalyzed() { return being_analyzed_; }
    void markAsSimulated() { is_simulated_ = true; }
    void markAsBeingAnalyzed() { being_analyzed_ = true; }

    // The actual content of the (w)mbus telegram. The DifVif entries.
    // Mapped from their key for quick access to their offset and content.
    std::map<std::string, std::pair<int, DVEntry>> dv_entries;

    std::string autoDetectPossibleDrivers();

    // part of original telegram bytes, only filled if pre-processing modifies it
    std::vector<uchar> original;

private:

    bool is_simulated_{};
    bool being_analyzed_{};
    bool parser_warns_ = true;
    MeterKeys* meter_keys{};

    // Fixes quirks from non-compliant meters to make telegram compatible with the standard
    void preProcess();

    bool parseMBusDLLandTPL(std::vector<uchar>::iterator& pos);

    bool parseDLL(std::vector<uchar>::iterator& pos);
    bool parseELL(std::vector<uchar>::iterator& pos);
    bool parseNWL(std::vector<uchar>::iterator& pos);
    bool parseAFL(std::vector<uchar>::iterator& pos);
    bool parseTPL(std::vector<uchar>::iterator& pos);

    void printDLL();
    void printELL();
    void printNWL();
    void printAFL();
    void printTPL();

    bool parse_TPL_72(std::vector<uchar>::iterator& pos);
    bool parse_TPL_78(std::vector<uchar>::iterator& pos);
    bool parse_TPL_79(std::vector<uchar>::iterator& pos);
    bool parse_TPL_7A(std::vector<uchar>::iterator& pos);
    bool alreadyDecryptedCBC(std::vector<uchar>::iterator& pos);
    bool potentiallyDecrypt(std::vector<uchar>::iterator& pos);
    bool parseTPLConfig(std::vector<uchar>::iterator& pos);


    bool parseShortTPL(std::vector<uchar>::iterator& pos);
    bool parseLongTPL(std::vector<uchar>::iterator& pos);
    bool checkMAC(std::vector<uchar>& frame,
        std::vector<uchar>::iterator from,
        std::vector<uchar>::iterator to,
        std::vector<uchar>& mac,
        std::vector<uchar>& mackey);
    bool findFormatBytesFromKnownMeterSignatures(std::vector<uchar>* format_bytes);
};

std::string manufacturer(int m_field);
std::string mediaType(int a_field_device_type, int m_field);
std::string mediaTypeJSON(int a_field_device_type, int m_field);
bool isCiFieldOfType(int ci_field, CI_TYPE type);
int ciFieldLength(int ci_field);
bool isCiFieldManufacturerSpecific(int ci_field);
std::string ciType(int ci_field);
std::string cType(int c_field);
bool isValidWMBusCField(int c_field);
bool isValidMBusCField(int c_field);
std::string ccType(int cc_field);

std::string vifKey(int vif); // E.g. temperature energy power mass_flow volume_flow
std::string vifUnit(int vif); // E.g. m3 c kwh kw MJ MJh

bool isCloseEnough(int media1, int media2);
LinkModeInfo* getLinkModeInfo(LinkMode lm);
LinkModeInfo* getLinkModeInfoFromBit(int bit);
std::string tostringFromELLSN(int sn);
std::string tostringFromTPLConfig(int cfg);
std::string tostringFromAFLFC(int fc);
std::string tostringFromAFLMC(int mc);
bool decrypt_ELL_AES_CTR(Telegram* t, std::vector<uchar>& frame, std::vector<uchar>::iterator& pos, std::vector<uchar>& aeskey);
bool decrypt_TPL_AES_CBC_IV(Telegram* t, std::vector<uchar>& frame, std::vector<uchar>::iterator& pos, std::vector<uchar>& aeskey,
    int* num_encrypted_bytes,
    int* num_not_encrypted_at_end);
bool decrypt_TPL_AES_CBC_NO_IV(Telegram* t, std::vector<uchar>& frame, std::vector<uchar>::iterator& pos, std::vector<uchar>& aeskey,
    int* num_encrypted_bytes,
    int* num_not_encrypted_at_end);
void incrementIV(uchar* iv, size_t len);
std::string frameTypeKamstrupC1(int ft);
void AES_CMAC(uchar* key, uchar* input, int length, uchar* mac);
void xorit(uchar* srca, uchar* srcb, uchar* dest, int len);
void shiftLeft(uchar* srca, uchar* srcb, int len);
// Decode only the standard defined bits in the tpl status byte. Ignore the top 3 bits.
// Return "OK" if sts == 0
std::string decodeTPLStatusByteOnlyStandardBits(uchar sts);