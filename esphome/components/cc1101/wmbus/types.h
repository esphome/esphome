#pragma once
#include <set>
#include <string>
typedef unsigned char uchar;
typedef unsigned int uint;

enum class TestBit
{
    Set,
    NotSet
};

enum class CI_TYPE
{
    ELL, NWL, AFL, TPL
};

enum class TPL_LENGTH
{
    NONE, SHORT, LONG
};

// Mark understood bytes as either PROTOCOL, ie dif vif, acc and other header bytes.
// Or CONTENT, ie the value fields found inside the transport layer.
enum class KindOfData
{
    PROTOCOL, CONTENT
};

// Content can be not understood at all NONE, partially understood PARTIAL when typically bitsets have
// been partially decoded, or FULL when the volume or energy field is by itself complete.
// Encrypted if it yet decrypted. Compressed and no format signature is known.
enum class Understanding
{
    NONE, ENCRYPTED, COMPRESSED, PARTIAL, FULL
};

enum class FrameType
{
    WMBUS,
    MBUS,
    HAN
};

#define LIST_OF_ELL_SECURITY_MODES \
    X(NoSecurity, 0) \
    X(AES_CTR, 1) \
    X(RESERVED, 2)

enum class ELLSecurityMode {
#define X(name,nr) name,
    LIST_OF_ELL_SECURITY_MODES
#undef X
};

#define LIST_OF_TPL_SECURITY_MODES \
    X(NoSecurity, 0) \
    X(MFCT_SPECIFIC, 1) \
    X(DES_NO_IV_DEPRECATED, 2) \
    X(DES_IV_DEPRECATED, 3) \
    X(SPECIFIC_4, 4) \
    X(AES_CBC_IV, 5) \
    X(RESERVED_6, 6) \
    X(AES_CBC_NO_IV, 7) \
    X(AES_CTR_CMAC, 8) \
    X(AES_CGM, 9) \
    X(AES_CCM, 10) \
    X(RESERVED_11, 11) \
    X(RESERVED_12, 12) \
    X(SPECIFIC_13, 13) \
    X(RESERVED_14, 14) \
    X(SPECIFIC_15, 15) \
    X(SPECIFIC_16_31, 16)


enum class TPLSecurityMode {
#define X(name,nr) name,
    LIST_OF_TPL_SECURITY_MODES
#undef X
};


enum class VifScaling
{
    Auto, // Scale to normalized VIF unit (ie kwh, m3, m3h etc)
    None, // No auto scaling.
    Unknown
};

enum class DifSignedness
{
    Signed,   // By default the binary values are interpreted as signed.
    Unsigned, // We can override for non-compliant meters.
    Unknown
};

namespace wmbusmeters {

enum PrintProperty
{
    REQUIRED = 1, // If no data has arrived, then print this field anyway with NaN or null.
    DEPRECATED = 2, // This field is about to be removed or changed in a newer driver, which will have a new name.
    STATUS = 4, // This is >the< status field and it should read OK of not error flags are set.
    INCLUDE_TPL_STATUS = 8, // This text field also includes the tpl status decoding. multiple OK:s collapse to a single OK.
    INJECT_INTO_STATUS = 16, // This text field is injected into the already defined status field. multiple OK:s collapse.
    HIDE = 32, // This field is only used in calculations, do not print it!
    Unknown = 1024
};

}

#define LIST_OF_METER_TYPES \
    X(AutoMeter) \
    X(UnknownMeter) \
    X(DoorWindowDetector) \
    X(ElectricityMeter) \
    X(GasMeter) \
    X(HeatCoolingMeter) \
    X(HeatCostAllocationMeter) \
    X(HeatMeter) \
    X(PressureSensor)  \
    X(PulseCounter) \
    X(Repeater) \
    X(SmokeDetector) \
    X(TempHygroMeter) \
    X(WaterMeter)  \

enum class MeterType {
#define X(name) name,
    LIST_OF_METER_TYPES
#undef X
};

#define LIST_OF_LINK_MODES \
    X(Any,any,--anylinkmode,(~0UL)) \
    X(MBUS,mbus,--mbus,(1UL<<1))    \
    X(S1,s1,--s1,      (1UL<<2))    \
    X(S1m,s1m,--s1m,   (1UL<<3))    \
    X(S2,s2,--s2,      (1UL<<4))    \
    X(T1,t1,--t1,      (1UL<<5))    \
    X(T2,t2,--t2,      (1UL<<6))    \
    X(C1,c1,--c1,      (1UL<<7))    \
    X(C2,c2,--c2,      (1UL<<8))    \
    X(UNKNOWN,unknown,----,0x0UL)

enum class LinkMode {
#define X(name,lcname,option,val) name,
    LIST_OF_LINK_MODES
#undef X
};


#define X(name,lcname,option,val) const uint64_t name##_bit = val;
LIST_OF_LINK_MODES
#undef X

struct LinkModeInfo
{
    LinkMode mode;
    const char* name;
    const char* lcname;
    const char* option;
    uint64_t val;
};


struct LinkModeSet
{
    // Add the link mode to the set of link modes.
    LinkModeSet& addLinkMode(LinkMode lm);
    void unionLinkModeSet(LinkModeSet lms);
    void disjunctionLinkModeSet(LinkModeSet lms);
    // Does this set support listening to the given link mode set?
    // If this set is C1 and T1 and the supplied set contains just C1,
    // then supports returns true.
    // Or if this set is just T1 and the supplied set contains just C1,
    // then supports returns false.
    // Or if this set is just C1 and the supplied set contains C1 and T1,
    // then supports returns true.
    // Or if this set is S1 and T1, and the supplied set contains C1 and T1,
    // then supports returns true.
    //
    // It will do a bitwise and of the linkmode bits. If the result
    // of the and is not zero, then support returns true.
    bool supports(LinkModeSet lms);
    // Check if this set contains the given link mode.
    bool has(LinkMode lm);
    // Check if all link modes are supported.
    bool hasAll(LinkModeSet lms);
    // Check if any link mode has been set.
    bool empty() { return set_ == 0; }
    // Clear the set to empty.
    void clear() { set_ = 0; }
    // Mark set as all linkmodes!
    void setAll() { set_ = (int)LinkMode::Any; }
    // For bit counting etc.
    int asBits() { return set_; }

    // Return a human readable string.
    std::string hr();

    LinkModeSet() { }
    LinkModeSet(uint64_t s) : set_(s) {}

private:

    uint64_t set_{};
};

#define LIST_OF_VIF_RANGES \
    X(Volume,0x10,0x17,Quantity::Volume,Unit::M3) \
    X(OnTime,0x20,0x23, Quantity::Time, Unit::Hour)  \
    X(OperatingTime,0x24,0x27, Quantity::Time, Unit::Hour)  \
    X(VolumeFlow,0x38,0x3F, Quantity::Flow, Unit::M3H) \
    X(FlowTemperature,0x58,0x5B, Quantity::Temperature, Unit::C) \
    X(ReturnTemperature,0x5C,0x5F, Quantity::Temperature, Unit::C) \
    X(TemperatureDifference,0x60,0x63, Quantity::Temperature, Unit::C) \
    X(ExternalTemperature,0x64,0x67, Quantity::Temperature, Unit::C) \
    X(Pressure,0x68,0x6B, Quantity::Pressure, Unit::BAR) \
    X(HeatCostAllocation,0x6E,0x6E, Quantity::HCA, Unit::HCA) \
    X(Date,0x6C,0x6C, Quantity::PointInTime, Unit::DateTimeLT) \
    X(DateTime,0x6D,0x6D, Quantity::PointInTime, Unit::DateTimeLT) \
    X(EnergyMJ,0x08,0x0F, Quantity::Energy, Unit::MJ) \
    X(EnergyWh,0x00,0x07, Quantity::Energy, Unit::KWH) \
    X(PowerW,0x28,0x2f, Quantity::Power, Unit::KW) \
    X(ActualityDuration,0x74,0x77, Quantity::Time, Unit::Hour) \
    X(FabricationNo,0x78,0x78, Quantity::Text, Unit::TXT) \
    X(EnhancedIdentification,0x79,0x79, Quantity::Text, Unit::TXT) \
    X(EnergyMWh,0x7B00,0x7B01, Quantity::Energy, Unit::KWH) \
    X(RelativeHumidity,0x7B1A,0x7B1B, Quantity::RH, Unit::RH) \
    X(AccessNumber,0x7D08,0x7D08, Quantity::Counter, Unit::COUNTER) \
    X(Medium,0x7D09,0x7D09, Quantity::Text, Unit::TXT) \
    X(Manufacturer,0x7D0A,0x7D0A, Quantity::Text, Unit::TXT) \
    X(ParameterSet,0x7D0B,0x7D0B, Quantity::Text, Unit::TXT) \
    X(ModelVersion,0x7D0C,0x7D0C, Quantity::Text, Unit::TXT) \
    X(HardwareVersion,0x7D0D,0x7D0D, Quantity::Text, Unit::TXT) \
    X(FirmwareVersion,0x7D0E,0x7D0E, Quantity::Text, Unit::TXT) \
    X(SoftwareVersion,0x7D0F,0x7D0F, Quantity::Text, Unit::TXT) \
    X(Location,0x7D10,0x7D10, Quantity::Text, Unit::TXT) \
    X(Customer,0x7D11,0x7D11, Quantity::Text, Unit::TXT) \
    X(ErrorFlags,0x7D17,0x7D17, Quantity::Text, Unit::TXT) \
    X(DigitalOutput,0x7D1A,0x7D1A, Quantity::Text, Unit::TXT) \
    X(DigitalInput,0x7D1B,0x7D1B, Quantity::Text, Unit::TXT) \
    X(DurationSinceReadout,0x7D2c,0x7D2f, Quantity::Time, Unit::Hour) \
    X(DurationOfTariff,0x7D31,0x7D33, Quantity::Time, Unit::Hour) \
    X(Dimensionless,0x7D3A,0x7D3A, Quantity::Counter, Unit::COUNTER) \
    X(Voltage,0x7D40,0x7D4F, Quantity::Voltage, Unit::Volt) \
    X(Amperage,0x7D50,0x7D5F, Quantity::Amperage, Unit::Ampere) \
    X(ResetCounter,0x7D60,0x7D60, Quantity::Counter, Unit::COUNTER) \
    X(CumulationCounter,0x7D61,0x7D61, Quantity::Counter, Unit::COUNTER) \
    X(SpecialSupplierInformation,0x7D67,0x7D67, Quantity::Text, Unit::TXT) \
    X(RemainingBattery,0x7D74,0x7D74, Quantity::Time, Unit::Day) \
    X(AnyVolumeVIF,0x00,0x00, Quantity::Volume, Unit::Unknown) \
    X(AnyEnergyVIF,0x00,0x00, Quantity::Energy, Unit::Unknown)  \
    X(AnyPowerVIF,0x00,0x00, Quantity::Power, Unit::Unknown)


enum class VIFRange
{
    None,
    Any,
#define X(name,from,to,quantity,unit) name,
    LIST_OF_VIF_RANGES
#undef X
};

enum FrameStatus { PartialFrame, FullFrame, ErrorInFrame, TextAndNotFrame };
enum class TelegramFormat
{
    UNKNOWN,
    WMBUS_C_FIELD, // The payload begins with the c-field
    WMBUS_CI_FIELD, // The payload begins with the ci-field (ie the c-field + dll is auto-prefixed.)
    MBUS_SHORT_FRAME, // Short mbus frame (ie ack etc)
    MBUS_LONG_FRAME // Long mbus frame (ie data frame)
};

#define LIST_OF_AFL_AUTH_TYPES \
    X(NoAuth, 0, 0)             \
    X(Reserved1, 1, 0)          \
    X(Reserved2, 2, 0)          \
    X(AES_CMAC_128_2, 3, 2)     \
    X(AES_CMAC_128_4, 4, 4)     \
    X(AES_CMAC_128_8, 5, 8)     \
    X(AES_CMAC_128_12, 6, 12)   \
    X(AES_CMAC_128_16, 7, 16)   \
    X(AES_GMAC_128_12, 8, 12)

enum class AFLAuthenticationType {
#define X(name,nr,len) name,
    LIST_OF_AFL_AUTH_TYPES
#undef X
};

#ifndef FUZZING
#define FUZZING false
#endif

#define CC_B_BIDIRECTIONAL_BIT 0x80
#define CC_RD_RESPONSE_DELAY_BIT 0x40
#define CC_S_SYNCH_FRAME_BIT 0x20
#define CC_R_RELAYED_BIT 0x10
#define CC_P_HIGH_PRIO_BIT 0x08


struct VIFRaw {
    VIFRaw(uint16_t v) : value(v) {}
    uint16_t value;
};

#define LIST_OF_VIF_COMBINABLES \
    X(Reserved,0x00,0x11) \
    X(Average,0x12,0x12) \
    X(InverseCompactProfile,0x13,0x13) \
    X(RelativeDeviation,0x14,0x14) \
    X(RecordErrorCodeMeterToController,0x15,0x1c) \
    X(StandardConformantDataContent,0x1d,0x1d) \
    X(CompactProfileWithRegister,0x1e,0x1e) \
    X(CompactProfile,0x1f,0x1f) \
    X(PerSecond,0x20,0x20) \
    X(PerMinute,0x21,0x21) \
    X(PerHour,0x22,0x22) \
    X(PerDay,0x23,0x23) \
    X(PerWeek,0x24,0x24) \
    X(PerMonth,0x25,0x25) \
    X(PerYear,0x26,0x26) \
    X(PerRevolutionMeasurement,0x27,0x27) \
    X(IncrPerInputPulseChannel0,0x28,0x28) \
    X(IncrPerInputPulseChannel1,0x29,0x29) \
    X(IncrPerOutputPulseChannel0,0x2a,0x2a) \
    X(IncrPerOutputPulseChannel1,0x2b,0x2b) \
    X(PerLitre,0x2c,0x2c) \
    X(PerM3,0x2d,0x2d) \
    X(PerKg,0x2e,0x2e) \
    X(PerKelvin,0x2f,0x2f) \
    X(PerKWh,0x30,0x30) \
    X(PerGJ,0x31,0x31) \
    X(PerKW,0x32,0x32) \
    X(PerKelvinLitreW,0x33,0x33) \
    X(PerVolt,0x34,0x34) \
    X(PerAmpere,0x35,0x35) \
    X(MultipliedByS,0x36,0x36) \
    X(MultipliedBySDivV,0x37,0x37) \
    X(MultipliedBySDivA,0x38,0x38) \
    X(StartDateTimeOfAB,0x39,0x39) \
    X(UncorrectedMeterUnit,0x3a,0x3a) \
    X(ForwardFlow,0x3b,0x3b) \
    X(BackwardFlow,0x3c,0x3c) \
    X(ReservedNonMetric,0x3d,0x3d) \
    X(ValueAtBaseCondC,0x3e,0x3e) \
    X(ObisDeclaration,0x3f,0x3f) \
    X(LowerLimit,0x40,0x40) \
    X(ExceedsLowerLimit,0x41,0x41) \
    X(DateTimeExceedsLowerFirstBegin, 0x42,0x42) \
    X(DateTimeExceedsLowerFirstEnd, 0x43,0x43) \
    X(DateTimeExceedsLowerLastBegin, 0x46,0x46) \
    X(DateTimeExceedsLowerLastEnd, 0x47,0x47) \
    X(UpperLimit,0x48,0x48) \
    X(ExceedsUpperLimit,0x49,0x49) \
    X(DateTimeExceedsUpperFirstBegin, 0x4a,0x4a) \
    X(DateTimeExceedsUpperFirstEnd, 0x4b,0x4b) \
    X(DateTimeExceedsUpperLastBegin, 0x4d,0x4d) \
    X(DateTimeExceedsUpperLastEnd, 0x4e,0x4e) \
    X(DurationExceedsLowerFirst,0x50,0x53) \
    X(DurationExceedsLowerLast,0x54,0x57) \
    X(DurationExceedsUpperFirst,0x58,0x5b) \
    X(DurationExceedsUpperLast,0x5c,0x5f) \
    X(DurationOfDFirst,0x60,0x63) \
    X(DurationOfDLast,0x64,0x67) \
    X(ValueDuringLowerLimitExceeded,0x68,0x68) \
    X(LeakageValues,0x69,0x69) \
    X(OverflowValues,0x6a,0x6a) \
    X(ValueDuringUpperLimitExceeded,0x6c,0x6c) \
    X(DateTimeOfDEFirstBegin,0x6a,0x6a) \
    X(DateTimeOfDEFirstEnd,0x6b,0x6b) \
    X(DateTimeOfDELastBegin,0x6e,0x6e) \
    X(DateTimeOfDELastEnd,0x6f,0x6f) \
    X(MultiplicativeCorrectionFactorForValue,0x70,0x77) \
    X(AdditiveCorrectionConstant,0x78,0x7b) \
    X(CombinableVIFExtension,0x7c,0x7c) \
    X(MultiplicativeCorrectionFactorForValue103,0x7d,0x7d) \
    X(FutureValue,0x7e,0x7e) \
    X(MfctSpecific,0x7f,0x7f) \
    X(AtPhase1,0x7c01,0x7c01) \
    X(AtPhase2,0x7c02,0x7c02) \
    X(AtPhase3,0x7c03,0x7c03) \
    X(AtNeutral,0x7c04,0x7c04) \
    X(BetweenPhaseL1AndL2,0x7c05,0x7c05) \
    X(BetweenPhaseL2AndL3,0x7c06,0x7c06) \
    X(BetweenPhaseL3AndL1,0x7c07,0x7c07) \
    X(AtQuadrantQ1,0x7c08,0x7c08) \
    X(AtQuadrantQ2,0x7c09,0x7c09) \
    X(AtQuadrantQ3,0x7c0a,0x7c0a) \
    X(AtQuadrantQ4,0x7c0b,0x7c0b) \
    X(DeltaBetweenImportAndExport,0x7c0c,0x7c0c) \
    X(AccumulationOfAbsoluteValue,0x7c10,0x7c10) \
    X(DataPresentedWithTypeC,0x7c11,0x7c11) \
    X(DataPresentedWithTypeD,0x7c12,0x7c12) \
    X(Mfct00,0x7f00,0x7f00) \
    X(Mfct01,0x7f01,0x7f01) \
    X(Mfct02,0x7f02,0x7f02) \
    X(Mfct03,0x7f03,0x7f03) \
    X(Mfct04,0x7f04,0x7f04) \
    X(Mfct05,0x7f05,0x7f05) \
    X(Mfct06,0x7f06,0x7f06) \
    X(Mfct07,0x7f07,0x7f07) \
    X(Mfct21,0x7f21,0x7f21) \

enum class VIFCombinable
{
    None,
    Any,
#define X(name,from,to) name,
    LIST_OF_VIF_COMBINABLES
#undef X
};

struct VIFCombinableRaw {
    VIFCombinableRaw(uint16_t v) : value(v) {}
    uint16_t value;
};

VIFCombinable toVIFCombinable(int i);
const char* toString(VIFCombinable v);

enum class MeasurementType
{
    Any,
    Instantaneous,
    Minimum,
    Maximum,
    AtError,
    Unknown
};

const char* toString(MeasurementType mt);
MeasurementType toMeasurementType(const char* s);
MeasurementType difMeasurementType(int dif);
std::string measurementTypeName(MeasurementType mt);

void extractDV(std::string& s, uchar* dif, int* vif, bool* has_difes, bool* has_vifes);



struct DifVifKey
{
    DifVifKey(std::string key) : key_(key) {
        extractDV(key, &dif_, &vif_, &has_difes_, &has_vifes_);
    }
    std::string str() { return key_; }
    bool operator==(DifVifKey& dvk) { return key_ == dvk.key_; }
    uchar dif() { return dif_; }
    int vif() { return vif_; }
    bool hasDifes() { return has_difes_; }
    bool hasVifes() { return has_vifes_; }

private:

    std::string key_;
    uchar dif_;
    int vif_;
    bool has_difes_;
    bool has_vifes_;
};

void extractDV(DifVifKey& s, uchar* dif, int* vif, bool* has_difes, bool* has_vifes);

static DifVifKey NoDifVifKey = DifVifKey("");

struct Vif
{
    Vif(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(Vif s) { return nr_ == s.nr_; }

private:
    int nr_;
};

enum class DVEntryCounterType
{
    UNKNOWN,
    STORAGE_COUNTER,
    TARIFF_COUNTER,
    SUBUNIT_COUNTER
};

DVEntryCounterType toDVEntryCounterType(const std::string& s);
const char* toString(DVEntryCounterType ct);

struct StorageNr
{
    StorageNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(StorageNr s) { return nr_ == s.nr_; }
    bool operator!=(StorageNr s) { return nr_ != s.nr_; }
    bool operator>=(StorageNr s) { return nr_ >= s.nr_; }
    bool operator<=(StorageNr s) { return nr_ <= s.nr_; }

private:
    int nr_;
};

static StorageNr AnyStorageNr = StorageNr(-1);

struct TariffNr
{
    TariffNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(TariffNr s) { return nr_ == s.nr_; }
    bool operator!=(TariffNr s) { return nr_ != s.nr_; }
    bool operator>=(TariffNr s) { return nr_ >= s.nr_; }
    bool operator<=(TariffNr s) { return nr_ <= s.nr_; }

private:
    int nr_;
};

static TariffNr AnyTariffNr = TariffNr(-1);

struct SubUnitNr
{
    SubUnitNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(SubUnitNr s) { return nr_ == s.nr_; }
    bool operator!=(SubUnitNr s) { return nr_ != s.nr_; }
    bool operator>=(SubUnitNr s) { return nr_ >= s.nr_; }
    bool operator<=(SubUnitNr s) { return nr_ <= s.nr_; }

private:
    int nr_;
};

struct IndexNr
{
    IndexNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(IndexNr s) { return nr_ == s.nr_; }
    bool operator!=(IndexNr s) { return nr_ != s.nr_; }

private:
    int nr_;
};

struct FieldInfo;

struct DVEntry
{
    int offset{}; // Where in the telegram this dventry was found.
    DifVifKey dif_vif_key;
    MeasurementType measurement_type;
    Vif vif;
    std::set<VIFCombinable> combinable_vifs;
    std::set<uint16_t> combinable_vifs_raw;
    StorageNr storage_nr;
    TariffNr tariff_nr;
    SubUnitNr subunit_nr;
    std::string value;

    DVEntry(int off,
        DifVifKey dvk,
        MeasurementType mt,
        Vif vi,
        std::set<VIFCombinable> vc,
        std::set<uint16_t> vc_raw,
        StorageNr st,
        TariffNr ta,
        SubUnitNr su,
        std::string& val) :
        offset(off),
        dif_vif_key(dvk),
        measurement_type(mt),
        vif(vi),
        combinable_vifs(vc),
        combinable_vifs_raw(vc_raw),
        storage_nr(st),
        tariff_nr(ta),
        subunit_nr(su),
        value(val)
    {
    }

    DVEntry() :
        offset(999999),
        dif_vif_key("????"),
        measurement_type(MeasurementType::Instantaneous),
        vif(0),
        storage_nr(0),
        tariff_nr(0),
        subunit_nr(0),
        value("x")
    {
    }

    bool extractDouble(double* out, bool auto_scale, bool force_unsigned);
    bool extractLong(uint64_t* out);
    bool extractDate(struct tm* out);
    bool extractReadableString(std::string* out);
    void addFieldInfo(FieldInfo* fi) { field_infos_.insert(fi); }
    bool hasFieldInfo(FieldInfo* fi) { return field_infos_.count(fi) > 0; }
    std::string str();

    double getCounter(DVEntryCounterType ct);

private:
    std::set<FieldInfo*> field_infos_; // The field infos selected to decode this entry.
};
struct Telegram;