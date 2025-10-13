
#include "meters.h"
/*
 Copyright (C) 2017-2023 Fredrik �hrstr�m (gpl-3.0-or-later)

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

#include"meters_common_implementation.h"
#include"units.h"


#include<algorithm>
#include<cmath>
#include<limits>
#include<memory.h>
#include<numeric>
#include<stdexcept>
#include<time.h>
#include "dvparser.h"
#include "types.h"


 std::map<string, DriverInfo>* registered_drivers_ = NULL;
vector<DriverInfo*>* registered_drivers_list_ = NULL;

void verifyDriverLookupCreated()
{
    if (registered_drivers_ == NULL)
    {
        registered_drivers_ = new  std::map<string, DriverInfo>;
    }
    if (registered_drivers_list_ == NULL)
    {
        registered_drivers_list_ = new vector<DriverInfo*>;
    }
}

DriverInfo* lookupDriver(string name)
{
    verifyDriverLookupCreated();

    // Check if we have a compiled/loaded driver available.
    if (registered_drivers_->count(name) == 1)
    {
        return &(*registered_drivers_)[name];
    }

    // No, ok lets look for driver aliases.
    for (DriverInfo* di : *registered_drivers_list_)
    {
        for (DriverName& dn : di->nameAliases())
        {
            if (dn.str() == name)
            {
                return di;
            }
        }
    }

    return NULL;
}

vector<DriverInfo*>& allDrivers()
{
    return *registered_drivers_list_;
}

void addRegisteredDriver(DriverInfo di)
{
    verifyDriverLookupCreated();
    if (registered_drivers_->count(di.name().str()) != 0)
    {
        error("Two drivers trying to register the name \"%s\"", di.name().str().c_str());
        exit(1);
    }

    (*registered_drivers_)[di.name().str()] = di;
    // The list elements points into the map.
    (*registered_drivers_list_).push_back(lookupDriver(di.name().str()));
}

bool DriverInfo::detect(uint16_t mfct, uchar type, uchar version)
{
    for (auto& dd : detect_)
    {
        if (dd.mfct == 0 && dd.type == 0 && dd.version == 0) continue; // Ignore drivers with no detection.
        if (dd.mfct == mfct && dd.type == type && dd.version == version) return true;
    }
    return false;
}

bool DriverInfo::isValidMedia(uchar type)
{
    for (auto& dd : detect_)
    {
        if (dd.type == type) return true;
    }
    return false;
}

bool DriverInfo::isCloseEnoughMedia(uchar type)
{
    for (auto& dd : detect_)
    {
        if (isCloseEnough(dd.type, type)) return true;
    }
    return false;
}

bool forceRegisterDriver(function<void(DriverInfo&)> setup)
{
    DriverInfo di;
    setup(di);

    // Check that the driver name has not been registered before!
    assert(lookupDriver(di.name().str()) == NULL);

    // Check that no other driver also triggers on the same detection values.
    for (auto& d : di.detect())
    {
        for (DriverInfo* p : allDrivers())
        {
            bool foo = p->detect(d.mfct, d.type, d.version);
            if (foo)
            {
                error("Internal error: driver %s tried to register the same auto detect combo as driver %s alread has taken!",
                    di.name().str().c_str(), p->name().str().c_str());
            }
        }
    }

    // Everything looks, good install this driver.
    addRegisteredDriver(di);

    // This code is invoked from the static initializers of DriverInfos when starting
    // wmbusmeters. Thus we do not yet know if the user has supplied --debug or similar setting.
    // To debug this you have to uncomment the printf below.
    // fprintf(stderr, "(STATIC) added driver: %s\n", n.c_str());
    return true;
}

bool registerDriver(function<void(DriverInfo&)> setup)
{
    DriverInfo di;
    setup(di);

    // Check that the driver name has not been registered before!
    assert(lookupDriver(di.name().str()) == NULL);

    // Check that no other driver also triggers on the same detection values.
    for (auto& d : di.detect())
    {
        for (DriverInfo* p : allDrivers())
        {
            bool foo = p->detect(d.mfct, d.type, d.version);
            if (foo)
            {
                error("Internal error: driver %s tried to register the same auto detect combo as driver %s alread has taken!",
                    di.name().str().c_str(), p->name().str().c_str());
            }
        }
    }

    // Everything looks, good install this driver.
    addRegisteredDriver(di);

    // This code is invoked from the static initializers of DriverInfos when starting
    // wmbusmeters. Thus we do not yet know if the user has supplied --debug or similar setting.
    // To debug this you have to uncomment the printf below.
    // fprintf(stderr, "(STATIC) added driver: %s\n", n.c_str());
    return true;
}

MeterCommonImplementation::MeterCommonImplementation(MeterInfo& mi,
    DriverInfo& di) :
    type_(di.type()),
    driver_name_(di.name()),
    name_(mi.name),
    mfct_tpl_status_bits_(di.mfctTPLStatusBits()),
    has_process_content_(di.hasProcessContent())
{
    address_expressions_ = mi.address_expressions;
    link_modes_ = mi.link_modes;

    if (mi.key.length() > 0)
    {
        hex2bin(mi.key, &meter_keys_.confidentiality_key);
    }
    for (auto s : mi.shells)
    {
        addShellMeterUpdated(s);
    }
    for (auto s : mi.meter_shells)
    {
        addShellMeterAdded(s);
    }
    for (auto j : mi.extra_constant_fields)
    {
        addExtraConstantField(j);
    }

    link_modes_.unionLinkModeSet(di.linkModes());
    force_mfct_index_ = di.forceMfctIndex();
}

void MeterCommonImplementation::addShellMeterAdded(string cmdline)
{
    shell_cmdlines_added_.push_back(cmdline);
}

void MeterCommonImplementation::addShellMeterUpdated(string cmdline)
{
    shell_cmdlines_updated_.push_back(cmdline);
}

void MeterCommonImplementation::addExtraConstantField(string ecf)
{
    extra_constant_fields_.push_back(ecf);
}

void MeterCommonImplementation::addExtraCalculatedField(string ecf)
{
    verbose("(meter) Adding calculated field: %s", ecf.c_str());

    vector<string> parts = splitString(ecf, '=');

    if (parts.size() != 2)
    {
        warning("Invalid formula for calculated field. %s", ecf.c_str());
        return;
    }

    string vname;
    Unit unit;

    bool ok = extractUnit(parts[0], &vname, &unit);
    if (!ok)
    {
        warning("Could not extract a valid unit from calculated field name %s", parts[0].c_str());
        return;
    }

    Quantity quantity = toQuantity(unit);

    FieldInfo* existing = findFieldInfo(vname, quantity);
    if (existing != NULL)
    {
        if (!canConvert(unit, existing->displayUnit()))
        {
            warning("Warning! Cannot add the calculated field: %s since it would conflict with the already declared field %s for quantity %s.",
                parts[0].c_str(), vname.c_str(), toString(quantity));
            return;
        }
    }

    addNumericFieldWithCalculator(
        vname,
        "Calculated: " + ecf,
        DEFAULT_PRINT_PROPERTIES,
        quantity,
        parts[1],
        unit
    );
}

vector<string>& MeterCommonImplementation::shellCmdlinesMeterAdded()
{
    return shell_cmdlines_added_;
}

vector<string>& MeterCommonImplementation::shellCmdlinesMeterUpdated()
{
    return shell_cmdlines_updated_;
}

vector<string>& MeterCommonImplementation::meterExtraConstantFields()
{
    return extra_constant_fields_;
}

DriverName MeterCommonImplementation::driverName()
{
    return driver_name_;
}

void MeterCommonImplementation::setMeterType(MeterType mt)
{
    type_ = mt;
}

void MeterCommonImplementation::addLinkMode(LinkMode lm)
{
    link_modes_.addLinkMode(lm);
}

void MeterCommonImplementation::setMfctTPLStatusBits(Translate::Lookup& lookup)
{
    mfct_tpl_status_bits_ = lookup;
}

void MeterCommonImplementation::addNumericFieldWithExtractor(string vname,
    string help,
    PrintProperties print_properties,
    Quantity vquantity,
    VifScaling vif_scaling,
    DifSignedness dif_signedness,
    FieldMatcher matcher,
    Unit display_unit,
    double scale)
{
    field_infos_.emplace_back(
        FieldInfo(field_infos_.size(),
            vname,
            vquantity,
            display_unit == Unit::Unknown ? defaultUnitForQuantity(vquantity) : display_unit,
            vif_scaling,
            dif_signedness,
            scale,
            matcher,
            help,
            print_properties,
            NULL,
            NULL,
            NULL,
            NULL,
            NoLookup, /* Lookup table */
            NULL /* Formula */
        ));
}

void MeterCommonImplementation::addNumericFieldWithCalculator(string vname,
    string help,
    PrintProperties print_properties,
    Quantity vquantity,
    string formula,
    Unit display_unit)
{
    Formula* f = newFormula();
    bool ok = f->parse(this, formula);
    if (!ok)
    {
        string err = f->errors();
        warning("Warning! Ignoring calculated field %s because parse failed:\n%s",
            vname.c_str(),
            err.c_str());
        delete f;
        return;
    }
    assert(ok);

    field_infos_.push_back(
        FieldInfo(field_infos_.size(),
            vname,
            vquantity,
            display_unit == Unit::Unknown ? defaultUnitForQuantity(vquantity) : display_unit,
            VifScaling::Auto,
            DifSignedness::Signed,
            1.0,
            FieldMatcher::noMatcher(),
            help,
            print_properties,
            NULL,
            NULL,
            NULL,
            NULL,
            NoLookup, /* Lookup table */
            f /* Formula */
        ));
}

void MeterCommonImplementation::addNumericFieldWithCalculatorAndMatcher(string vname,
    string help,
    PrintProperties print_properties,
    Quantity vquantity,
    string formula,
    FieldMatcher matcher,
    Unit display_unit)
{
    Formula* f = newFormula();
    bool ok = f->parse(this, formula);
    if (!ok)
    {
        string err = f->errors();
        warning("Warning! Ignoring calculated field %s because parse failed:\n%s",
            vname.c_str(),
            err.c_str());
        delete f;
        return;
    }
    assert(ok);

    field_infos_.push_back(
        FieldInfo(field_infos_.size(),
            vname,
            vquantity,
            display_unit == Unit::Unknown ? defaultUnitForQuantity(vquantity) : display_unit,
            VifScaling::Auto,
            DifSignedness::Signed,
            1.0,
            matcher,
            help,
            print_properties,
            NULL,
            NULL,
            NULL,
            NULL,
            NoLookup, /* Lookup table */
            f /* Formula */
        ));
}


void MeterCommonImplementation::addNumericField(
    string vname,
    Quantity vquantity,
    PrintProperties print_properties,
    string help,
    Unit display_unit)
{
    field_infos_.emplace_back(
        FieldInfo(field_infos_.size(),
            vname,
            vquantity,
            display_unit == Unit::Unknown ? defaultUnitForQuantity(vquantity) : display_unit,
            VifScaling::None,
            DifSignedness::Signed,
            1.0,
            FieldMatcher::noMatcher(),
            help,
            print_properties,
            NULL, // getValueFunc,
            NULL,
            NULL, // setValueFunc
            NULL,
            NoLookup, /* Lookup table */
            NULL /* Formula */
        ));
}

void MeterCommonImplementation::addStringFieldWithExtractor(string vname,
    string help,
    PrintProperties print_properties,
    FieldMatcher matcher)
{
    field_infos_.emplace_back(
        FieldInfo(field_infos_.size(),
            vname,
            Quantity::Text,
            defaultUnitForQuantity(Quantity::Text),
            VifScaling::None,
            DifSignedness::Signed,
            1.0,
            matcher,
            help,
            print_properties,
            NULL,
            NULL,
            NULL,
            NULL,
            NoLookup, /* Lookup table */
            NULL /* Formula */
        ));
}

void MeterCommonImplementation::addStringFieldWithExtractorAndLookup(string vname,
    string help,
    PrintProperties print_properties,
    FieldMatcher matcher,
    Translate::Lookup lookup)
{
    field_infos_.emplace_back(
        FieldInfo(field_infos_.size(),
            vname,
            Quantity::Text,
            defaultUnitForQuantity(Quantity::Text),
            VifScaling::None,
            DifSignedness::Signed,
            1.0,
            matcher,
            help,
            print_properties,
            NULL,
            NULL,
            NULL,
            NULL,
            lookup,
            NULL /* Formula */
        ));
}

void MeterCommonImplementation::addStringField(string vname,
    string help,
    PrintProperties print_properties)
{
    field_infos_.emplace_back(
        FieldInfo(field_infos_.size(),
            vname,
            Quantity::Text,
            defaultUnitForQuantity(Quantity::Text),
            VifScaling::None,
            DifSignedness::Signed,
            1.0,
            FieldMatcher(),
            help,
            print_properties,
            NULL,
            NULL,
            NULL,
            NULL,
            NoLookup, /* Lookup table */
            NULL /* Formula */
        ));
}


vector<AddressExpression>& MeterCommonImplementation::addressExpressions()
{
    return address_expressions_;
}

vector<FieldInfo>& MeterCommonImplementation::fieldInfos()
{
    return field_infos_;
}

vector<string>& MeterCommonImplementation::extraConstantFields()
{
    return extra_constant_fields_;
}

string MeterCommonImplementation::name()
{
    return name_;
}

void MeterCommonImplementation::onUpdate(function<void(Telegram*, Meter*)> cb)
{
    on_update_.push_back(cb);
}

int MeterCommonImplementation::numUpdates()
{
    return num_updates_;
}

string MeterCommonImplementation::datetimeOfUpdateHumanReadable()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    strftime(datetime, 20, "%Y-%m-%d %H:%M:%S", localtime(&datetime_of_update_));
    return string(datetime);
}

string MeterCommonImplementation::datetimeOfUpdateRobot()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    // This is the date time in the Greenwich timezone (Zulu time), dont get surprised!
    time_t d = datetime_of_update_;
    struct tm ts;
    gmtime_r( &d,&ts);
    strftime(datetime, sizeof(datetime), "%FT%TZ", &ts);
    return string(datetime);
}

string MeterCommonImplementation::unixTimestampOfUpdate()
{
    char ut[40];
    memset(ut, 0, sizeof(ut));
    snprintf(ut, sizeof(ut) - 1, "%lu", datetime_of_update_);
    return string(ut);
}

const char* toString(MeterType type)
{
#define X(tname) if (type == MeterType::tname) return #tname;
    LIST_OF_METER_TYPES
#undef X
        return "unknown";
}

MeterType toMeterType(string type)
{
#define X(tname) if (type == #tname) return MeterType::tname;
    LIST_OF_METER_TYPES
#undef X
        return MeterType::UnknownMeter;
}

string toString(DriverInfo& di)
{
    return di.name().str();
}

bool MeterCommonImplementation::isTelegramForMeter(Telegram* t, Meter* meter, MeterInfo* mi)
{
    string name;
    vector<AddressExpression> address_expressions;
    string driver_name;

    assert((meter && !mi) ||
           (!meter && mi));

    if (meter)
    {
        name = meter->name();
        address_expressions = meter->addressExpressions();
        driver_name = meter->driverName().str();
    }
    else
    {
        name = mi->name;
        address_expressions = mi->address_expressions;
        driver_name = mi->driver_name.str();
    }

    if (isDebugEnabled())
    {
        // Telegram addresses
        string t_idsc = Address::concat(t->addresses);
        // Meter/MeterInfo address expressions
        string m_idsc = AddressExpression::concat(address_expressions);
        debug("(meter) %s: for me? %s in %s", name.c_str(), t_idsc.c_str(), m_idsc.c_str());
    }

    bool used_wildcard = false;
    bool id_match = doesTelegramMatchExpressions(t->addresses, address_expressions, &used_wildcard);

    if (!id_match) {
        // The id must match.
        debug("(meter) %s: not for me: not my id", name.c_str());
        return false;
    }

    bool valid_driver = isMeterDriverValid(driver_name, t->dll_mfct, t->dll_type, t->dll_version);
    if (!valid_driver && t->tpl_id_found)
    {
        valid_driver = isMeterDriverValid(driver_name, t->tpl_mfct, t->tpl_type, t->tpl_version);
    }

    if (!valid_driver)
    {
        // Are we using the right driver? Perhaps not since
        // this particular driver, mfct, media, version combo
        // is not registered in the METER_DETECTION list in meters.h

        
        if (used_wildcard)
        {
            // The match for the id was not exact, thus the user is listening using a wildcard
            // to many meters and some received matched meter telegrams are not from the right meter type,
            // ie their driver does not match. Lets just ignore telegrams that probably cannot be decoded properly.
            verbose("(meter) ignoring telegram from %s since it matched a wildcard id rule but driver (%s) does not match.",
                    t->addresses.back().str().c_str(), driver_name.c_str());
            return false;
            }

            // The match was exact, ie the user has actually specified 12345678 and foo as driver even
            // though they do not match. Lets warn and then proceed. It is common that a user tries a
            // new version of a meter with the old driver, thus it might not be a real error.
        if (isVerboseEnabled() || isDebugEnabled())
        {
            string possible_drivers = t->autoDetectPossibleDrivers();
            if (t->beingAnalyzed() == false && driver_name != "auto")
            {
                debug("(meter) %s: meter detection did not match the selected driver '%s', detected driver is: '%s'",
                    name.c_str(),
                    driver_name.c_str(),
                    possible_drivers.c_str());
            }
        }
    }

    debug("(meter) %s: yes for me", name.c_str());
    return true;
}

MeterKeys* MeterCommonImplementation::meterKeys()
{
    return &meter_keys_;
}

int MeterCommonImplementation::index()
{
    return index_;
}

void MeterCommonImplementation::setIndex(int i)
{
    index_ = i;
}

string MeterCommonImplementation::bus()
{
    return bus_;
}

void MeterCommonImplementation::triggerUpdate(Telegram* t)
{
    // Check if processContent has discarded this telegram.
    if (t->discard) return;

    datetime_of_poll_ = time(NULL);
    datetime_of_update_ = t->about.timestamp ? t->about.timestamp : datetime_of_poll_;
    num_updates_++;
    for (auto& cb : on_update_) if (cb) cb(t, this);
    t->handled = true;
    if (NULL == findFieldInfo("timestamp", Quantity::Text)) {
      addStringField("timestamp", "TimeStamp", PrintProperty::HIDE);
    }
    setStringValue("timestamp", datetimeOfUpdateRobot());
}

string findField(string key, vector<string>* extra_constant_fields)
{
    key = key + "=";
    for (string ecf : *extra_constant_fields)
    {
        if (startsWith(ecf, key))
        {
            return ecf.substr(key.length());
        }
    }
    return "";
}

// Is the desired field one of the fields common to all meters and telegrams?
bool checkCommonField(string* buf, string desired_field, Meter* m, Telegram* t, char c, bool human_readable)
{
    if (desired_field == "name")
    {
        *buf += m->name() + c;
        return true;
    }
    if (desired_field == "id")
    {
        *buf += t->addresses.back().id + c;
        return true;
    }
    if (desired_field == "timestamp")
    {
        *buf += m->datetimeOfUpdateHumanReadable() + c;
        return true;
    }
    if (desired_field == "timestamp_lt")
    {
        *buf += m->datetimeOfUpdateHumanReadable() + c;
        return true;
    }
    if (desired_field == "timestamp_utc")
    {
        *buf += m->datetimeOfUpdateRobot() + c;
        return true;
    }
    if (desired_field == "timestamp_ut")
    {
        *buf += m->unixTimestampOfUpdate() + c;
        return true;
    }
    if (desired_field == "device")
    {
        *buf += t->about.device + c;
        return true;
    }
    if (desired_field == "rssi_dbm")
    {
        *buf += std::to_string(t->about.rssi_dbm) + c;
        return true;
    }

    return false;
}

// Is the desired field one of the meter printable fields?
bool checkPrintableField(string* buf, string desired_field, Meter* m, Telegram* t, char c,
    vector<FieldInfo>& fields, bool human_readable)
{

    for (FieldInfo& fi : fields)
    {
        if (fi.xuantity() == Quantity::Text)
        {
            // Strings are simply just print them.
            if (desired_field == fi.vname())
            {
                *buf += m->getStringValue(&fi) + c;
                return true;
            }
        }
        else
        {
            string display_unit_s = unitToStringLowerCase(fi.displayUnit());
            string var = fi.vname() + "_" + display_unit_s;
            if (desired_field != var) continue;

            // We have the correc field.
            if (fi.displayUnit() == Unit::DateLT)
            {
                double d = m->getNumericValue(&fi, Unit::DateLT);
                *buf += strdate(d);
                *buf += c;
                return true;
            }
            else if (fi.displayUnit() == Unit::DateTimeLT)
            {
                double d = m->getNumericValue(&fi, Unit::DateTimeLT);
                *buf += strdatetime(d);
                *buf += c;
                return true;
            }
            else if (fi.displayUnit() == Unit::DateTimeUTC)
            {
                double d = m->getNumericValue(&fi, Unit::DateTimeUTC);
                *buf += strTimestampUTC(d);
                *buf += c;
                return true;
            }
            else
            {
                // Default unit.
                *buf += valueToString(m->getNumericValue(&fi, fi.displayUnit()), fi.displayUnit());
                if (human_readable)
                {
                    *buf += " ";
                    *buf += unitToStringHR(fi.displayUnit());
                }
                *buf += c;
                return true;
            }
        }
    }

    return false;
}

// Is the desired field one of the constant fields?
bool checkConstantField(string* buf, string field, char c, vector<string>* extra_constant_fields)
{
    // Ok, lets look for extra constant fields and print any such static information.
    string v = findField(field, extra_constant_fields);
    if (v != "")
    {
        *buf += v + c;
        return true;
    }

    return false;
}

string concatFields(Meter* m, Telegram* t, char c, vector<FieldInfo>& prints, bool human_readable,
    vector<string>* selected_fields, vector<string>* extra_constant_fields)
{
    if (selected_fields == NULL || selected_fields->size() == 0)
    {
        selected_fields = &m->selectedFields();
    }

    string buf = "";

    for (string field : *selected_fields)
    {
        bool handled = checkCommonField(&buf, field, m, t, c, human_readable);
        if (handled) continue;

        handled = checkPrintableField(&buf, field, m, t, c, prints, human_readable);
        if (handled) continue;

        handled = checkConstantField(&buf, field, c, extra_constant_fields);
        if (handled) continue;

        if (!handled)
        {
            buf += "?" + field + "?" + c;
        }
    }
    if (buf.back() == c) buf.pop_back();
    return buf;
}

bool MeterCommonImplementation::handleTelegram(AboutTelegram &about, vector<uchar> input_frame,
                                               bool simulated, vector<Address> *addresses,
                                               bool *id_match, Telegram *out_analyzed)
{
    out_analyzed->about = about;
    bool ok = out_analyzed->parseHeader(input_frame);

    *addresses = out_analyzed->addresses;

    if (!ok || !isTelegramForMeter(out_analyzed, this, NULL))
    {
        // This telegram is not intended for this meter.
        *id_match = false;
        return false;
    }

    *id_match = true;
    verbose("(meter) %s(%d) %s  handling telegram from %s",
            name().c_str(),
            index(),
            driverName().str().c_str(),
            out_analyzed->addresses.back().str().c_str());

    // For older meters with manufacturer specific data without a nice 0f dif marker.
    if (force_mfct_index_ != -1)
    {
        out_analyzed->force_mfct_index = force_mfct_index_;
    }

    ok = out_analyzed->parse(input_frame, &meter_keys_, true);
    if (!ok)
    {
        // Ignoring telegram since it could not be parsed.
        return false;
    }

    // Invoke standardized field extractors!
    processFieldExtractors(out_analyzed);
    if (hasProcessContent())
    {
        // Invoke tailor made meter specific parsing!
        processContent(out_analyzed);
    }
    // Invoke any calculators working on the extracted fields.
    processFieldCalculators();

    // All done....
    triggerUpdate(out_analyzed);
    return true;
}

void MeterCommonImplementation::processFieldExtractors(Telegram* t)
{
    // Multiple dventries can be matched against a single wildcard FieldInfo.
     std::map<FieldInfo*, set<DVEntry*>> founds;

    // Sort the dv_entries based on their offset in the telegram.
    // I.e. restore the ordering that was implicit in the telegram.
    vector<DVEntry*> sorted_entries;

    for (auto& p : t->dv_entries)
    {
        sorted_entries.push_back(&p.second.second);
    }
    sort(sorted_entries.begin(), sorted_entries.end(),
        [](const DVEntry* a, const DVEntry* b) -> bool { return a->offset < b->offset; });

    // Now go through each field_info defined by the driver.
    for (FieldInfo& fi : field_infos_)
    {
        int current_match_nr = 0;

        if (!fi.hasMatcher())
        {
            // This field_info has not been matched to a dv_entry before!
            debug("(meters) skipping field without matcher %s(%s)[%d]...",
                fi.vname().c_str(),
                toString(fi.xuantity()),
                fi.index());
            continue;
        }

        debug("(meters) trying field info %s(%s)[%d]...",
            fi.vname().c_str(),
            toString(fi.xuantity()),
            fi.index());

        // Iterate through dv_entries in the telegram in the same order the telegram presented them.
        for (DVEntry* dve : sorted_entries)
        {
            if (fi.hasMatcher() && fi.matches(dve))
            {
                current_match_nr++;

                if (fi.matcher().index_nr != IndexNr(current_match_nr) &&
                    !fi.matcher().expectedToMatchAgainstMultipleEntries())
                {
                    // This field info did match, but requires another index nr!
                    // Increment the current index nr and look for the next match.
                }
                else if (founds[&fi].count(dve) == 0 || fi.matcher().expectedToMatchAgainstMultipleEntries())
                {
                    debug("(meters) using field info %s(%s)[%d] to extract %s at offset %d",
                        fi.vname().c_str(),
                        toString(fi.xuantity()),
                        fi.index(),
                        dve->dif_vif_key.str().c_str(),
                        dve->offset);

                    dve->addFieldInfo(&fi);
                    fi.performExtraction(this, t, dve);
                    founds[&fi].insert(dve);
                }
                else
                {
                    if (isVerboseEnabled())
                    {
                        set<DVEntry*> old = founds[&fi];
                        string olds;
                        for (DVEntry* dve : old)
                        {
                            olds += std::to_string(dve->offset) + ",";
                        }
                        olds.pop_back();

                        verbose("(meter) while processing field extractors ignoring dventry %s at offset %d matching since "
                            "field %s was already matched against offsets %s !",
                            dve->dif_vif_key.str().c_str(),
                            dve->offset,
                            fi.vname().c_str(),
                            olds.c_str());
                    }
                }
            }
        }
    }

    // Iterate over the fields that has no matcher rule. Ie the field
    // itself does the searching and matching.
    for (FieldInfo& fi : field_infos_)
    {
        if (!fi.hasMatcher())
        {
            fi.performExtraction(this, t, NULL);
        }
        else if (founds.count(&fi) == 0 && fi.printProperties().hasINCLUDETPLSTATUS())
        {
            // This is a status field and it joins the tpl status but it also
            // has a potential dve match, which did not trigger. Now
            // force extraction to get the tpl status.
            fi.performExtraction(this, t, NULL);
        }
    }
}

void MeterCommonImplementation::processFieldCalculators()
{
    // Iterate over the fields with formulas but no matcher.
    for (FieldInfo& fi : field_infos_)
    {
        if (fi.hasFormula() && !fi.hasMatcher())
        {
            debug("(meters) calculating field %s(%s)[%d]",
                fi.vname().c_str(),
                toString(fi.xuantity()),
                fi.index());
            fi.performCalculation(this);
        }
    }
}

string MeterCommonImplementation::getStatusField(FieldInfo* fi)
{
    string field_name_no_unit = fi->vname();
    if (string_values_.count(field_name_no_unit) == 0)
    {
        return "null"; // This is translated to a real(non-string) null in the json.
    }
    StringField& sf = string_values_[field_name_no_unit];
    string value = sf.value;

    // This is >THE< status field, only one is allowed.
    // Look for other fields with the JOIN_INTO_STATUS marker.
    // These other fields will not be printed, instead
    // joined into this status field.
    for (FieldInfo& f : field_infos_)
    {
        if (f.printProperties().hasINJECTINTOSTATUS())
        {
            //printf("NOW >%s<\n", value.c_str());
            string more = getStringValue(&f);
            //printf("MORE >%s<\n", more.c_str());
            string joined = joinStatusOKStrings(value, more);
            //printf("JOINED >%s<\n", joined.c_str());
            value = joined;
        }
    }
    // Sort all found flags and remove any duplicates. A well designed meter decoder
    // should not be able to generate duplicates.
    value = sortStatusString(value);
    // If it is empty, then translate to OK!
    if (value == "") value = "OK";
    return value;
}

void MeterCommonImplementation::processContent(Telegram* t)
{
}

bool MeterCommonImplementation::hasProcessContent()
{
    return has_process_content_;
}

void MeterCommonImplementation::setNumericValue(FieldInfo* fi, DVEntry* dve, Unit u, double v)
{
    string field_name_no_unit;
    if (dve == NULL)
    {
        string field_name_no_unit = fi->vname();
        numeric_values_[pair<string, Unit>(field_name_no_unit, fi->displayUnit())] = NumericField(u, v, fi);
    }
    else
    {
        field_name_no_unit = fi->generateFieldNameNoUnit(dve);
        numeric_values_[pair<string, Unit>(field_name_no_unit, fi->displayUnit())] = NumericField(u, v, fi, *dve);
    }
}

void MeterCommonImplementation::setNumericValue(string vname, Unit u, double v)
{
    Quantity q = toQuantity(u);
    FieldInfo* fi = findFieldInfo(vname, q);

    if (fi == NULL)
    {
        warning("(meter) cannot set numeric value %g %s for non-existant field \"%s\" %s", v, unitToStringLowerCase(u).c_str(), vname.c_str(), toString(q));
        return;
    }
    setNumericValue(fi, NULL, u, v);
}

bool MeterCommonImplementation::hasValue(FieldInfo* fi)
{
    return hasStringValue(fi) || hasNumericValue(fi);
}

bool MeterCommonImplementation::hasNumericValue(FieldInfo* fi)
{
    pair<string, Unit> key(fi->vname(), fi->displayUnit());

    return numeric_values_.count(key) != 0;
}

bool MeterCommonImplementation::hasStringValue(FieldInfo* fi)
{
    return string_values_.count(fi->vname()) != 0;
}

bool MeterCommonImplementation::hasStringValue(string name)
{
    return string_values_.count(name) != 0;
}

string MeterCommonImplementation::getMyStringValue(string name)
{
    for (auto& p : string_values_)
    {
        string vname = p.first;
        StringField& sf = p.second;
        
        if (vname == name) {
            return sf.value;
        }
    }
    return "";
}

double MeterCommonImplementation::getMyNumericValue(string vname, Unit to)
{
    Unit from;
    for (auto& p : numeric_values_)
    {
        string nvname = p.first.first;
        if (nvname == vname) {
            from = p.first.second;
            break;
        }
    }
    pair<string, Unit> key(vname, from);
    if (numeric_values_.count(key) == 0)
    {
        return std::numeric_limits<double>::quiet_NaN(); // This is translated into a null in the json.
    }
    NumericField& nf = numeric_values_[key];
    return convert(nf.value, nf.unit, to);
}

double MeterCommonImplementation::getNumericValue(FieldInfo* fi, Unit to)
{
    string field_name_no_unit = fi->vname();
    pair<string, Unit> key(field_name_no_unit, fi->displayUnit());
    if (numeric_values_.count(key) == 0)
    {
        return std::numeric_limits<double>::quiet_NaN(); // This is translated into a null in the json.
    }
    NumericField& nf = numeric_values_[key];
    return convert(nf.value, nf.unit, to);
}

double MeterCommonImplementation::getNumericValue(string vname, Unit to)
{
    pair<string, Unit> key(vname, to);
    if (numeric_values_.count(key) == 0)
    {
        return std::numeric_limits<double>::quiet_NaN(); // This is translated into a null in the json.
    }
    NumericField& nf = numeric_values_[key];
    return convert(nf.value, nf.unit, to);
}

void MeterCommonImplementation::setStringValue(FieldInfo* fi, string v, DVEntry* dve)
{
    string field_name_no_unit;

    if (dve == NULL)
    {
        string field_name_no_unit = fi->vname();
        string_values_[field_name_no_unit] = StringField(v, fi);
    }
    else
    {
        field_name_no_unit = fi->generateFieldNameNoUnit(dve);
        string_values_[field_name_no_unit] = StringField(v, fi);
    }
}

void MeterCommonImplementation::setStringValue(string vname, string v, DVEntry* dve)
{
    FieldInfo* fi = findFieldInfo(vname, Quantity::Text);

    if (fi == NULL)
    {
        warning("(meter) cannot set string value %s for non-existant field \"%s\"", v.c_str(), vname.c_str());
        return;
    }
    setStringValue(fi, v, dve);
}

string MeterCommonImplementation::getStringValue(FieldInfo* fi)
{
    string field_name_no_unit = fi->vname();
    if (string_values_.count(field_name_no_unit) == 0)
    {
        return "null"; // This is translated to a real(non-string) null in the json.
    }
    StringField& sf = string_values_[field_name_no_unit];
    string value = sf.value;

    if (fi->printProperties().hasSTATUS())
    {
        // This is >THE< status field, only one is allowed.
        // Look for other fields with the JOIN_INTO_STATUS marker.
        // These other fields will not be printed, instead
        // joined into this status field.
        for (FieldInfo& f : field_infos_)
        {
            if (f.printProperties().hasINJECTINTOSTATUS())
            {
                string more = getStringValue(&f);
                string joined = joinStatusOKStrings(value, more);
                value = joined;
            }
        }
        // Sort all found flags and remove any duplicates. A well designed meter decoder
        // should not be able to generate duplicates.
        value = sortStatusString(value);
        // If it is empty, then translate to OK!
        if (value == "") value = "OK";
    }

    return value;
}

string MeterCommonImplementation::decodeTPLStatusByte(uchar sts)
{
    return ::decodeTPLStatusByteWithMfct(sts, mfct_tpl_status_bits_);
}

FieldInfo* MeterCommonImplementation::findFieldInfo(string vname, Quantity xuantity)
{
    FieldInfo* found = NULL;
    for (FieldInfo& p : field_infos_)
    {
        if (p.vname() == vname &&
            p.xuantity() == xuantity)
        {
            found = &p;
            break;
        }
    }

    return found;
}

string MeterCommonImplementation::renderJsonOnlyDefaultUnit(string vname, Quantity xuantity)
{
    FieldInfo* fi = findFieldInfo(vname, xuantity);

    if (fi == NULL) return "unknown field " + vname;
    return fi->renderJsonOnlyDefaultUnit(this);
}

string MeterCommonImplementation::debugValues()
{
    string s;

    for (auto& p : numeric_values_)
    {
        string vname = p.first.first;
        string us = unitToStringLowerCase(p.first.second);
        NumericField& nf = p.second;

        s += tostrprintf("  %s_%s = %f\n", vname.c_str(), us.c_str(), nf.value);
    }

    for (auto& p : string_values_)
    {
        string vname = p.first;
        StringField& nf = p.second;

        s += tostrprintf("  %s = \"%s\"\n", vname.c_str(), nf.value.c_str());
    }

    return s;
}

FieldInfo::~FieldInfo()
{
}

FieldInfo::FieldInfo(int index,
    string vname,
    Quantity xuantity,
    Unit display_unit,
    VifScaling vif_scaling,
    DifSignedness dif_signedness,
    double scale,
    FieldMatcher matcher,
    string help,
    PrintProperties print_properties,
    function<double(Unit)> get_numeric_value_override,
    function<string()> get_string_value_override,
    function<void(Unit, double)> set_numeric_value_override,
    function<void(string)> set_string_value_override,
    Translate::Lookup lookup,
    Formula* formula
) :
    index_(index),
    vname_(vname),
    xuantity_(xuantity),
    display_unit_(display_unit),
    vif_scaling_(vif_scaling),
    dif_signedness_(dif_signedness),
    scale_(scale),
    matcher_(matcher),
    help_(help),
    print_properties_(print_properties),
    get_numeric_value_override_(get_numeric_value_override),
    get_string_value_override_(get_string_value_override),
    set_numeric_value_override_(set_numeric_value_override),
    set_string_value_override_(set_string_value_override),
    lookup_(lookup),
    formula_(formula),
    field_name_(newStringInterpolator()),
    valid_field_name_(field_name_->parse(vname))
{
    if (!valid_field_name_)
    {
        warning("(meter) field template \"%s\" could not be parsed!", vname.c_str());
    }
}


bool lookupDriverInfo(const string& driver_name, DriverInfo* out_di)
{
    DriverInfo* di = lookupDriver(driver_name);
   
    if (out_di != NULL)
    {
        *out_di = *di;
    }

    return true;
}

bool is_driver_and_extras(const string& t, DriverName* out_driver_name, string* out_extras)
{
    // piigth(jump=foo)
    // multical21
    DriverInfo di;
    size_t ps = t.find('(');
    size_t pe = t.find(')');

    size_t te = 0; // Position after type end.

    bool found_parentheses = (ps != string::npos && pe != string::npos);

    if (!found_parentheses)
    {
        if (lookupDriverInfo(t, &di))
        {
            *out_driver_name = di.name();
            // We found a registered driver.
            *out_extras = "";
            return true;
        }
        *out_extras = "";
        return true;
    }

    // Parentheses must be last.
    if (!(ps > 0 && ps < pe && pe == t.length() - 1)) return false;
    te = ps;

    string type = t.substr(0, te);

    bool found = lookupDriverInfo(type, &di);

    if (found)
    {
        *out_driver_name = di.name();
    }

    string extras = t.substr(ps + 1, pe - ps - 1);
    *out_extras = extras;

    return true;
}

bool isValidLinkModes(string m)
{
    LinkModeSet lms;
    char buf[50];
    strcpy(buf, m.c_str());
    char* saveptr{};
    const char* tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL)
    {
        LinkMode lm = toLinkMode(tok);
        if (lm == LinkMode::UNKNOWN)
        {
            return false;
        }
        lms.addLinkMode(lm);
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return true;
}

bool MeterInfo::parse(string n, string d, string aes, string k)
{
    clear();

    name = n;
    address_expressions = splitAddressExpressions(aes);
    key = k;

    // Example driver ( extras )

    if (!is_driver_and_extras(d, &driver_name, &extras))
    {
        return false;
    }

    return true;
}


string FieldInfo::renderJson(Meter* m, DVEntry* dve)
{
    string s;

    string display_unit_s = unitToStringLowerCase(displayUnit());
    string field_name = generateFieldNameNoUnit(dve);

    if (xuantity() == Quantity::Text)
    {
        string v = m->getStringValue(this);
        if (v == "null")
        {
            // Yes, right now a meter cannot send a string value "something":"null" it will
            // be translated into "something":null in the json, indicating that there is no value.
            // This should not be a problem for now. Lets deal with it when a meter decides to send "null"
            // as its version string for example.
            s += "\"" + field_name + "\":null";
        }
        else
        {
            // Normally the string values are quoted in json. TODO quote the value properly.
            // A well crafted meter could send a version string with " and break the json format.
            s += "\"" + field_name + "\":\"" + v + "\"";
        }
    }
    else
    {
        if (displayUnit() == Unit::DateLT)
        {
            s += "\"" + field_name + "_" + display_unit_s + "\":\"" + strdate(m->getNumericValue(field_name, Unit::DateLT)) + "\"";
        }
        else if (displayUnit() == Unit::DateTimeLT)
        {
            s += "\"" + field_name + "_" + display_unit_s + "\":\"" + strdatetime(m->getNumericValue(field_name, Unit::DateTimeLT)) + "\"";
        }
        else if (displayUnit() == Unit::DateTimeUTC)
        {
            s += "\"" + field_name + "_" + display_unit_s + "\":\"" + strTimestampUTC(m->getNumericValue(field_name, Unit::DateTimeUTC)) + "\"";
        }
        else
        {
            // All numeric values.
            s += "\"" + field_name + "_" + display_unit_s + "\":" + valueToString(m->getNumericValue(field_name, displayUnit()), displayUnit());
        }
    }

    return s;
}

string FieldInfo::renderJsonOnlyDefaultUnit(Meter* m)
{
    return renderJson(m, NULL);
}

string FieldInfo::renderJsonText(Meter* m, DVEntry* dve)
{
    return renderJson(m, dve);
}

string FieldInfo::generateFieldNameNoUnit(DVEntry* dve)
{
    if (!valid_field_name_) return "bad_field_name";

    return field_name_->apply(dve);
}

string FieldInfo::generateFieldNameWithUnit(DVEntry* dve)
{
    if (!valid_field_name_) return "bad_field_name";

    if (xuantity_ == Quantity::Text)
    {
        return field_name_->apply(dve);
    }

    string display_unit_s = unitToStringLowerCase(displayUnit());
    string var = field_name_->apply(dve);

    return var + "_" + display_unit_s;
}

void MeterCommonImplementation::createMeterEnv(string id,
    vector<string>* envs,
    vector<string>* extra_constant_fields)
{
    envs->push_back(string("METER_ID=" + id));
    envs->push_back(string("METER_NAME=") + name());
    envs->push_back(string("METER_TYPE=") + driverName().str());

    // If the configuration has supplied json_address=Roodroad 123
    // then the env variable METER_address will available and have the content "Roodroad 123"
    for (string add_json : meterExtraConstantFields())
    {
        envs->push_back(string("METER_") + add_json);
    }
    for (string extra_field : *extra_constant_fields)
    {
        envs->push_back(string("METER_") + extra_field);
    }
}

void MeterCommonImplementation::printJsonMeter(Telegram* t,
    string* json,
    bool pretty_print_json)
{
    string media;
    if (t->tpl_id_found)
    {
        media = mediaTypeJSON(t->tpl_type, t->tpl_mfct);
    }
    else if (t->ell_id_found)
    {
        media = mediaTypeJSON(t->ell_type, t->ell_mfct);
    }
    else
    {
        media = mediaTypeJSON(t->dll_type, t->dll_mfct);
    }

    string id = "";
    if (t->addresses.size() > 0)
    {
        id = t->addresses.back().id;
    }

    string indent = "";
    string newline = "";

    if (pretty_print_json)
    {
        indent = "    ";
        newline = "\n";
    }

    string s;
    s += "{" + newline;
    s += indent + "\"meter\":\"" + driverName().str() + "\"," + newline;
    s += indent + "\"media\":\"" + media + "\"," + newline;
    // s += indent + "\"name\":\"" + name() + "\"," + newline;
    s += indent + "\"id\":\"" + id + "\"," + newline;
    s += indent + "\"rssi_dbm\":" + std::to_string(t->about.rssi_dbm) + "," + newline;

    // Iterate over the meter field infos...
     std::map<FieldInfo*, set<DVEntry*>> founds; // Multiple dventries can match to a single field info.
    set<string> found_vnames;

    for (auto& p : numeric_values_)
    {
        string vname = p.first.first;
        NumericField& nf = p.second;
        if (nf.field_info->printProperties().hasHIDE()) continue;

        string out = nf.field_info->renderJson(this, &nf.dv_entry);
        s += indent + out + "," + newline;
    }

    for (auto& p : string_values_)
    {
        string vname = p.first;
        StringField& sf = p.second;

        if (sf.field_info->printProperties().hasHIDE()) continue;
        if (sf.field_info->printProperties().hasSTATUS())
        {
            string in = getStatusField(sf.field_info);
            string out = tostrprintf("\"%s\":\"%s\"", vname.c_str(), in.c_str());
            s += indent + out + "," + newline;
        }
        else
        {
            if (sf.value == "null")
            {
                // The string "null" translates to actual json null.
                string out = tostrprintf("\"%s\":null", vname.c_str());
                s += indent + out + "," + newline;
            }
            else
            {
                string out = tostrprintf("\"%s\":\"%s\"", vname.c_str(), sf.value.c_str());
                s += indent + out + "," + newline;
            }
        }
    }

    s += indent + "\"timestamp\":\"" + datetimeOfUpdateRobot() + "\"" + "," + newline;
    s += indent + "\"device\":\"" + t->about.device + "\"";
    s += "}";
    *json = s;
}


void MeterCommonImplementation::printMeter(Telegram* t,
    string* human_readable,
    string* fields, char separator,
    string* json,
    vector<string>* envs,
    vector<string>* extra_constant_fields,
    vector<string>* selected_fields,
    bool pretty_print_json)
{
    *human_readable = concatFields(this, t, '\t', field_infos_, true, selected_fields, extra_constant_fields);
    *fields = concatFields(this, t, separator, field_infos_, false, selected_fields, extra_constant_fields);

    string media;
    if (t->tpl_id_found)
    {
        media = mediaTypeJSON(t->tpl_type, t->tpl_mfct);
    }
    else if (t->ell_id_found)
    {
        media = mediaTypeJSON(t->ell_type, t->ell_mfct);
    }
    else
    {
        media = mediaTypeJSON(t->dll_type, t->dll_mfct);
    }

    string id = "";
    if (t->addresses.size() > 0)
    {
        id = t->addresses.back().id;
    }

    string indent = "";
    string newline = "";

    if (pretty_print_json)
    {
        indent = "    ";
        newline = "\n";
    }

    string s;
    s += "{" + newline;
    s += indent + "\"media\":\"" + media + "\"," + newline;
    s += indent + "\"meter\":\"" + driverName().str() + "\"," + newline;
    s += indent + "\"name\":\"" + name() + "\"," + newline;
    s += indent + "\"id\":\"" + id + "\"," + newline;

    // Iterate over the meter field infos...
     std::map<FieldInfo*, set<DVEntry*>> founds; // Multiple dventries can match to a single field info.
    set<string> found_vnames;

    for (auto& p : numeric_values_)
    {
        string vname = p.first.first;
        NumericField& nf = p.second;
        if (nf.field_info->printProperties().hasHIDE()) continue;

        string out = nf.field_info->renderJson(this, &nf.dv_entry);
        s += indent + out + "," + newline;
    }

    for (auto& p : string_values_)
    {
        string vname = p.first;
        StringField& sf = p.second;

        if (sf.field_info->printProperties().hasHIDE()) continue;
        if (sf.field_info->printProperties().hasSTATUS())
        {
            string in = getStatusField(sf.field_info);
            string out = tostrprintf("\"%s\":\"%s\"", vname.c_str(), in.c_str());
            s += indent + out + "," + newline;
        }
        else
        {
            if (sf.value == "null")
            {
                // The string "null" translates to actual json null.
                string out = tostrprintf("\"%s\":null", vname.c_str());
                s += indent + out + "," + newline;
            }
            else
            {
                string out = tostrprintf("\"%s\":\"%s\"", vname.c_str(), sf.value.c_str());
                s += indent + out + "," + newline;
            }
        }
    }
    /*
    for (FieldInfo& fi : field_infos_)
    {
        if (fi.printProperties().hasHIDE()) continue;

        // The field should be printed in the json. (Most usually should.)
        for (auto& i : t->dv_entries)
        {
            // Check each telegram dv entry.
            DVEntry *dve = &i.second.second;
            // Has the entry been matches to this field, then print it as json.
            if (dve->hasFieldInfo(&fi))
            {
                assert(founds[&fi].count(dve) == 0);

                founds[&fi].insert(dve);
                string field_name = fi.generateFieldNameNoUnit(dve);
                found_vnames.insert(field_name);
            }
        }
    }

    for (FieldInfo& fi : field_infos_)
    {
        if (fi.printProperties().hasHIDE()) continue;

        if (founds.count(&fi) != 0)
        {
            // This field info has matched against some dventries.
            for (DVEntry *dve : founds[&fi])
            {
                debug("(meters) render field %s(%s %s)[%d] with dventry @%d key %s data %s",
                      fi.vname().c_str(), toString(fi.xuantity()), unitToStringLowerCase(fi.displayUnit()).c_str(), fi.index(),
                      dve->offset,
                      dve->dif_vif_key.str().c_str(),
                      dve->value.c_str());
                string out = fi.renderJson(this, dve);
                debug("(meters)             %s", out.c_str());
                s += indent+out+","+newline;
            }
        }
        else
        {
            // Ok, no value found in received telegram.
            // Print field anyway if it is required,
            // or if a value has been received before and this field has not been received using a different rule.
            // Why this complicated rule?
            // E.g. the minmoess mbus seems to use storage 1 for target_m3 but the wmbus version uses storage 8.
            // I.e. we have two rules that store into target_m3, this check will prevent target_m3 from being printed twice.
            if (fi.printProperties().hasREQUIRED() ||
                (hasValue(&fi) && (
                    found_vnames.count(fi.vname()) == 0 ||
                    fi.hasFormula()))) // TODO! Fix so a new field total_l does not overwrite total_m3 in mem.
            {
                // No telegram entries found, but this field should be printed anyway.
                // It will be printed with any value received from a previous telegram.
                // Or if no value has been received, null.
                debug("(meters) render field %s(%s)[%d] without dventry",
                      fi.vname().c_str(), toString(fi.xuantity()), fi.index());
                string out = fi.renderJson(this, NULL);
                debug("(meters)             %s", out.c_str());
                s += indent+out+","+newline;
            }
        }
    }
    */
    s += indent + "\"timestamp\":\"" + datetimeOfUpdateRobot() + "\"";

    if (t->about.device != "")
    {
        s += "," + newline;
        s += indent + "\"device\":\"" + t->about.device + "\"," + newline;
        s += indent + "\"rssi_dbm\":" + std::to_string(t->about.rssi_dbm);
    }
    for (string extra_field : meterExtraConstantFields())
    {
        s += "," + newline;
        s += indent + makeQuotedJson(extra_field);
    }
    for (string extra_field : *extra_constant_fields)
    {
        s += "," + newline;
        s += indent + makeQuotedJson(extra_field);
    }
    s += newline;
    s += "}";
    *json = s;

    createMeterEnv(id, envs, extra_constant_fields);

    envs->push_back(string("METER_JSON=") + *json);
    envs->push_back(string("METER_MEDIA=") + media);
    envs->push_back(string("METER_TIMESTAMP=") + datetimeOfUpdateRobot());
    envs->push_back(string("METER_TIMESTAMP_UTC=") + datetimeOfUpdateRobot());
    envs->push_back(string("METER_TIMESTAMP_UT=") + unixTimestampOfUpdate());
    envs->push_back(string("METER_TIMESTAMP_LT=") + datetimeOfUpdateHumanReadable());

    for (FieldInfo& fi : field_infos_)
    {
        if (fi.printProperties().hasHIDE()) continue;

        string display_unit_s = unitToStringUpperCase(fi.displayUnit());
        string var = fi.vname();
        std::transform(var.begin(), var.end(), var.begin(), ::toupper);
        if (fi.xuantity() == Quantity::Text)
        {
            string envvar = "METER_" + var + "=" + getStringValue(&fi);
            envs->push_back(envvar);
        }
        else
        {
            string envvar = "METER_" + var + "_" + display_unit_s + "=" + valueToString(getNumericValue(&fi, fi.displayUnit()), fi.displayUnit());
            envs->push_back(envvar);
        }
    }

    if (t->about.device != "")
    {
        envs->push_back(string("METER_DEVICE=") + t->about.device);
        envs->push_back(string("METER_RSSI_DBM=") + std::to_string(t->about.rssi_dbm));
    }

}

void MeterCommonImplementation::setExpectedTPLSecurityMode(TPLSecurityMode tsm)
{
    expected_tpl_sec_mode_ = tsm;
}

void MeterCommonImplementation::setExpectedELLSecurityMode(ELLSecurityMode dsm)
{
    expected_ell_sec_mode_ = dsm;
}

TPLSecurityMode MeterCommonImplementation::expectedTPLSecurityMode()
{
    return expected_tpl_sec_mode_;
}

ELLSecurityMode MeterCommonImplementation::expectedELLSecurityMode()
{
    return expected_ell_sec_mode_;
}

void detectMeterDrivers(int manufacturer, int media, int version, vector<string>* drivers)
{
    for (DriverInfo* p : allDrivers())
    {
        if (p->detect(manufacturer, media, version))
        {
            drivers->push_back(p->name().str());
        }
    }
}

bool isMeterDriverValid(DriverName driver_name, int manufacturer, int media, int version)
{
    for (DriverInfo* p : allDrivers())
    {
        if (p->detect(manufacturer, media, version))
        {
            if (p->hasDriverName(driver_name)) return true;
        }
    }

    return false;
}

bool isMeterDriverReasonableForMedia(string driver_name, int media)
{
    if (media == 0x37) return false;  // Skip converter meter side since they do not give any useful information.

    for (DriverInfo* p : allDrivers())
    {
        if (p->name().str() == driver_name && p->isValidMedia(media))
        {
            return true;
        }
    }

    return false;
}

DriverInfo driver_unknown_;

DriverInfo pickMeterDriver(Telegram* t)
{
    int manufacturer = t->dll_mfct;
    int media = t->dll_type;
    int version = t->dll_version;

    if (t->tpl_id_found)
    {
        manufacturer = t->tpl_mfct;
        media = t->tpl_type;
        version = t->tpl_version;
    }

    for (DriverInfo* p : allDrivers())
    {
        if (p->detect(manufacturer, media, version))
        {
            return *p;
        }
    }

    return driver_unknown_;
}

shared_ptr<Meter> createMeter(MeterInfo* mi)
{
    shared_ptr<Meter> newm;

    const char* keymsg = (mi->key[0] == 0) ? "not-encrypted" : "encrypted";

    DriverInfo* di = lookupDriver(mi->driver_name.str());

    if (di != NULL)
    {
        shared_ptr<Meter> newm = di->construct(*mi);
        for (string& j : mi->extra_calculated_fields)
        {
            newm->addExtraCalculatedField(j);
        }
      
        if (mi->selected_fields.size() > 0)
        {
            newm->setSelectedFields(mi->selected_fields);
        }
        else
        {
            newm->setSelectedFields(di->defaultFields());
        }
        string aesc = AddressExpression::concat(mi->address_expressions);
        verbose("(meter) created %s %s %s %s",
                mi->name.c_str(),
                di->name().str().c_str(),
                aesc.c_str(),
                keymsg);
        return newm;
    }

    return newm;
}

string MeterInfo::str()
{
    string r;
    r += driver_name.str();
    if (r.size() > 0) r.pop_back();

    return r;
}

bool isValidKey(const string& key, MeterInfo& mi)
{
    if (key.length() == 0) return true;
    if (key == "NOKEY") {
        return true;
    }
    if (mi.driver_name.str() == "izar" ||
        mi.driver_name.str() == "hydrus")
    {
        // These meters can either be OMS compatible 128 bit key (32 hex).
        // Or using an older proprietary encryption with 64 bit keys (16 hex)
        if (key.length() != 16 && key.length() != 32) return false;
    }
    else
    {
        // OMS compliant meters have 128 bit AES keys (32 hex).
        // There is a deprecated DES mode, but I have not yet
        // seen any telegram using that mode.
        if (key.length() != 32) return false;
    }
    vector<uchar> tmp;
    return hex2bin(key, &tmp);
}

void FieldInfo::performExtraction(Meter* m, Telegram* t, DVEntry* dve)
{
    if (xuantity_ == Quantity::Text)
    {
        // Extract a string.
        extractString(m, t, dve);
    }
    else if (hasFormula())
    {
        double value = formula_->calculate(displayUnit(), dve, m);
        m->setNumericValue(this, dve, displayUnit(), value);
    }
    else
    {
        // Extract a numeric.
        extractNumeric(m, t, dve);
    }
}

void FieldInfo::performCalculation(Meter* m)
{
    assert(hasFormula());

    double value = formula_->calculate(displayUnit());
    m->setNumericValue(this, NULL, displayUnit(), value);
}

bool FieldInfo::hasMatcher()
{
    return matcher_.active == true;
}

bool FieldInfo::hasFormula()
{
    return formula_ != NULL;
}

bool FieldInfo::matches(DVEntry* dve)
{
    return matcher_.matches(*dve);
}

string FieldInfo::str()
{
    return tostrprintf("%d %s_%s (%s) %s [%s] \"%s\"",
        index_,
        vname_.c_str(),
        unitToStringLowerCase(display_unit_).c_str(),
        toString(xuantity_),
        toString(vif_scaling_),
        matcher_.str().c_str(),
        help_.c_str());
}

DriverName MeterInfo::driverName()
{
    return driver_name;
}

bool FieldInfo::extractNumeric(Meter* m, Telegram* t, DVEntry* dve)
{
    bool found = false;
    string key = matcher_.dif_vif_key.str();

    if (dve == NULL)
    {
        if (key == "")
        {
            // Search for key.
            bool ok = findKeyWithNr(matcher_.measurement_type,
                matcher_.vif_range,
                matcher_.storage_nr_from.intValue(),
                matcher_.tariff_nr_from.intValue(),
                matcher_.index_nr.intValue(),
                &key,
                &t->dv_entries);
            // No entry was found.
            if (!ok) return false;
        }
        // No entry with this key was found.
        if (t->dv_entries.count(key) == 0) return false;
        dve = &t->dv_entries[key].second;
    }
    assert(dve != NULL);
    assert(key == "" || dve->dif_vif_key.str() == key);

    string field_name;
    if (isDebugEnabled())
    {
        field_name = generateFieldNameWithUnit(dve);
    }

    double extracted_double_value = NAN;

    bool auto_vif_scaling = vifScaling() == VifScaling::Auto;
    bool force_unsigned = difSignedness() == DifSignedness::Unsigned;

    if (dve->extractDouble(&extracted_double_value, auto_vif_scaling, force_unsigned))
    {
        Unit decoded_unit = displayUnit();
        if (matcher_.vif_range == VIFRange::DateTime)
        {
            struct tm datetime;
            dve->extractDate(&datetime);
            time_t tmp = mktime(&datetime);
            string bbb = strdatetime(tmp);
            extracted_double_value = tmp;
        }
        else if (matcher_.vif_range == VIFRange::Date)
        {
            struct tm date;
            dve->extractDate(&date);
            time_t tmp = mktime(&date);
            extracted_double_value = tmp;
        }
        else if (matcher_.vif_range == VIFRange::AnyEnergyVIF ||
            matcher_.vif_range == VIFRange::AnyVolumeVIF ||
            matcher_.vif_range == VIFRange::AnyPowerVIF)
        {
            // Find the actual unit used in the telegram.
            decoded_unit = toDefaultUnit(dve->vif);
        }
        else if (matcher_.vif_range != VIFRange::Any &&
            matcher_.vif_range != VIFRange::None)
        {
            // Pick the default unit for this range.
            decoded_unit = toDefaultUnit(matcher_.vif_range);
        }

        debug("(meter) %s %s decoded %s default %s value %g (scale %g)",
            toString(matcher_.vif_range),
            field_name.c_str(),
            unitToStringLowerCase(decoded_unit).c_str(),
            unitToStringLowerCase(display_unit_).c_str(),
            extracted_double_value,
            scale());

        if (scale() != 1.0)
        {
            // Hardcoded scale factor for this field used for manufacturer specific values without vif units.
            extracted_double_value *= scale();
        }
        if (overrideConversion(decoded_unit, display_unit_))
        {
            // Special case! Transform the decoded unit into the display unit. I.e. kwh was replaced with kvarh.
            decoded_unit = display_unit_;
        }
        m->setNumericValue(this, dve, display_unit_, convert(extracted_double_value, decoded_unit, display_unit_));
        t->addMoreExplanation(dve->offset, renderJson(m, dve));
        found = true;
    }
    return found;
}

static string add_tpl_status(string existing_status, Meter* m, Telegram* t)
{
    string status = m->decodeTPLStatusByte(t->tpl_sts);
    t->addMoreExplanation(t->tpl_sts_offset, "(%s)", status.c_str());
    if (status != "OK")
    {
        if (existing_status != "OK")
        {
            // Join the statuses.
            if (existing_status != "")
            {
                existing_status += " ";
            }
            existing_status += status;
        }
        else
        {
            // Overwrite OK.
            existing_status = status;
        }
    }
    else
    {
        // No change to the existing_status
    }

    return existing_status;
}

bool FieldInfo::extractString(Meter* m, Telegram* t, DVEntry* dve)
{
    bool found = false;
    string key = matcher_.dif_vif_key.str();

    if (dve == NULL)
    {
        if (key == "")
        {
            if (!hasMatcher())
            {
                // There is no matcher, only use case is to capture JOIN_TPL_STATUS.
                if (print_properties_.hasINCLUDETPLSTATUS())
                {
                    string status = add_tpl_status("OK", m, t);
                    m->setStringValue(this, status, dve);
                    return true;
                }
            }
            else
            {
                // Search for key.
                bool ok = findKeyWithNr(matcher_.measurement_type,
                    matcher_.vif_range,
                    matcher_.storage_nr_from.intValue(),
                    matcher_.tariff_nr_from.intValue(),
                    matcher_.index_nr.intValue(),
                    &key,
                    &t->dv_entries);
                // No entry was found.
                if (!ok) {
                    // Nothing found, however check if capturing JOIN_TPL_STATUS.
                    if (print_properties_.hasINCLUDETPLSTATUS())
                    {
                        string status = add_tpl_status("OK", m, t);
                        m->setStringValue(this, status, dve);
                        return true;
                    }
                    return false;
                }
            }
        }
        // No entry with this key was found.
        if (t->dv_entries.count(key) == 0)
        {
            // Nothing found, however check if capturing JOIN_TPL_STATUS.
            if (print_properties_.hasINCLUDETPLSTATUS())
            {
                string status = add_tpl_status("OK", m, t);
                m->setStringValue(this, status, dve);
                return true;
            }
            return false;
        }
        dve = &t->dv_entries[key].second;
    }
    assert(dve != NULL);
    assert(key == "" || dve->dif_vif_key.str() == key);

    // Generate the json field name:
    string field_name = generateFieldNameNoUnit(dve);

    uint64_t extracted_bits{};
    if (lookup_.hasLookups() || (print_properties_.hasINCLUDETPLSTATUS()))
    {
        string translated_bits = "";
        // The field has lookups, or the print property JOIN_TPL_STATUS is set,
        // this means that we should create a string.
        if (lookup_.hasLookups() && dve->extractLong(&extracted_bits))
        {
            translated_bits = lookup().translate(extracted_bits);
            found = true;
        }

        if (print_properties_.hasINCLUDETPLSTATUS())
        {
            translated_bits = add_tpl_status(translated_bits, m, t);
        }

        if (found)
        {
            m->setStringValue(this, translated_bits, dve);
            t->addMoreExplanation(dve->offset, renderJsonText(m, dve));
        }
    }
    else if (matcher_.vif_range == VIFRange::DateTime)
    {
        struct tm datetime;
        dve->extractDate(&datetime);
        string extracted_device_date_time;

        if (dve->value.size() == 12)
        {
            // A long date time sec + timezone field. TODO add timezone data.
            extracted_device_date_time = strdatetimesec(&datetime);
        }
        else
        {
            extracted_device_date_time = strdatetime(&datetime);
        }
        m->setStringValue(this, extracted_device_date_time, dve);
        t->addMoreExplanation(dve->offset, renderJsonText(m, dve));
        found = true;
    }
    else if (matcher_.vif_range == VIFRange::Date)
    {
        struct tm date;
        dve->extractDate(&date);
        string extracted_device_date = strdate(&date);
        m->setStringValue(this, extracted_device_date, dve);
        t->addMoreExplanation(dve->offset, renderJsonText(m, dve));
        found = true;
    }
    else if (matcher_.vif_range == VIFRange::Any ||
        matcher_.vif_range == VIFRange::EnhancedIdentification ||
        matcher_.vif_range == VIFRange::FabricationNo ||
        matcher_.vif_range == VIFRange::HardwareVersion ||
        matcher_.vif_range == VIFRange::FirmwareVersion ||
        matcher_.vif_range == VIFRange::Medium ||
        matcher_.vif_range == VIFRange::Manufacturer ||
        matcher_.vif_range == VIFRange::ModelVersion ||
        matcher_.vif_range == VIFRange::SoftwareVersion ||
        matcher_.vif_range == VIFRange::Customer ||
        matcher_.vif_range == VIFRange::Location ||
        matcher_.vif_range == VIFRange::SpecialSupplierInformation ||
        matcher_.vif_range == VIFRange::ParameterSet)
    {
        string extracted_id;
        dve->extractReadableString(&extracted_id);
        m->setStringValue(this, extracted_id, dve);
        t->addMoreExplanation(dve->offset, renderJsonText(m, dve));
        found = true;
    }
    else
    {
        error("Internal error: Cannot extract text string from vif %s in %s:%d",
            toString(matcher_.vif_range),
            __FILE__, __LINE__);

    }
    return found;
}

bool checkIf(set<string>& fields, const char* s)
{
    if (fields.count(s) > 0)
    {
        fields.erase(s);
        return true;
    }

    return false;
}

void checkFieldsEmpty(set<string>& fields, string name)
{
    if (fields.size() > 0)
    {
        string info;
        for (auto& s : fields) { info += s + " "; }

        warning("(meter) when adding common fields to driver %s, these fields were not found: %s",
            name.c_str(),
            info.c_str());
    }
}

void MeterCommonImplementation::addOptionalLibraryFields(string field_names)
{
    set<string> fields = splitStringIntoSet(field_names, ',');

    if (checkIf(fields, "actuality_duration_s"))
    {
        addNumericFieldWithExtractor(
            "actuality_duration",
            "Lapsed time between measurement and transmission.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ActualityDuration),
            Unit::Second
        );
    }

    if (checkIf(fields, "actuality_duration_h"))
    {
        addNumericFieldWithExtractor(
            "actuality_duration",
            "Lapsed time between measurement and transmission.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ActualityDuration)
        );
    }

    if (checkIf(fields, "fabrication_no"))
    {
        addStringFieldWithExtractor(
            "fabrication_no",
            "Fabrication number.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FabricationNo)
        );
    }

    if (checkIf(fields, "enhanced_id"))
    {
        addStringFieldWithExtractor(
            "enhanced_id",
            "Enhanced identification number.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::EnhancedIdentification)
        );
    }

    if (checkIf(fields, "software_version"))
    {
        addStringFieldWithExtractor(
            "software_version",
            "Software version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::SoftwareVersion)
        );
    }

    if (checkIf(fields, "manufacturer"))
    {
        addStringFieldWithExtractor(
            "manufacturer",
            "Meter manufacturer.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Manufacturer)
        );
    }

    if (checkIf(fields, "model_version"))
    {
        addStringFieldWithExtractor(
            "model_version",
            "Meter model version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)
        );
    }

    if (checkIf(fields, "firmware_version"))
    {
        addStringFieldWithExtractor(
            "firmware_version",
            "Meter firmware version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FirmwareVersion)
        );
    }

    if (checkIf(fields, "parameter_set"))
    {
        addStringFieldWithExtractor(
            "parameter_set",
            "Parameter set for this meter.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ParameterSet)
        );
    }

    if (checkIf(fields, "customer"))
    {
        addStringFieldWithExtractor(
            "customer",
            "Customer name.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Customer)
        );
    }

    if (checkIf(fields, "location"))
    {
        addStringFieldWithExtractor(
            "location",
            "Meter installed at this customer location.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Location)
        );
    }

    if (checkIf(fields, "operating_time_h"))
    {
        addNumericFieldWithExtractor(
            "operating_time",
            "How long the meter has been collecting data.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::OperatingTime)
        );
    }

    if (checkIf(fields, "on_time_h"))
    {
        addNumericFieldWithExtractor(
            "on_time",
            "How long the meter has been powered up.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::OnTime)
        );
    }

    if (checkIf(fields, "on_time_at_error_h"))
    {
        addNumericFieldWithExtractor(
            "on_time_at_error",
            "How long the meter has been in an error state while powered up.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::OnTime)
        );
    }

    if (checkIf(fields, "meter_date"))
    {
        addStringFieldWithExtractor(
            "meter_date",
            "Date when the meter sent the telegram.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
        );
    }

    if (checkIf(fields, "meter_date_at_error"))
    {
        addStringFieldWithExtractor(
            "meter_date_at_error",
            "Date when the meter was in error.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date)
        );
    }

    if (checkIf(fields, "meter_datetime"))
    {
        addStringFieldWithExtractor(
            "meter_datetime",
            "Date and time when the meter sent the telegram.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
        );
    }

    if (checkIf(fields, "meter_datetime_at_error"))
    {
        addStringFieldWithExtractor(
            "meter_datetime_at_error",
            "Date and time when the meter was in error.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::DateTime)
        );
    }

    if (checkIf(fields,"total_m3"))
    {
        addNumericFieldWithExtractor(
            "total",
            "The total media volume consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
        );
    }

    if (checkIf(fields, "target_m3"))
    {
        addNumericFieldWithExtractor(
            "target",
            "The volume recorded by this meter at the target date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
        );
    }

    if (checkIf(fields, "target_date"))
    {
        addNumericFieldWithExtractor(
            "target",
            "The target date. Usually the end of the previous billing period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
        );
    }

    if (checkIf(fields, "total_forward_m3"))
    {
        addNumericFieldWithExtractor(
            "total_forward",
            "The total media volume flowing forward.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .add(VIFCombinable::ForwardFlow)
        );
    }

    if (checkIf(fields, "total_backward_m3"))
    {
        addNumericFieldWithExtractor(
            "total_backward",
            "The total media volume flowing backward.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .add(VIFCombinable::BackwardFlow)
        );
    }

    if (checkIf(fields, "flow_temperature_c"))
    {
        addNumericFieldWithExtractor(
            "flow_temperature",
            "Forward media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
        );
    }

    if (checkIf(fields, "external_temperature_c"))
    {
        addNumericFieldWithExtractor(
            "external_temperature",
            "Temperature outside of meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
        );
    }

    if (checkIf(fields, "return_temperature_c"))
    {
        addNumericFieldWithExtractor(
            "return_temperature",
            "Return media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
        );
    }

    if (checkIf(fields, "flow_return_temperature_difference_c"))
    {
        addNumericFieldWithExtractor(
            "flow_return_temperature_difference",
            "The difference between flow and return media temperatures.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::TemperatureDifference)
        );
    }

    if (checkIf(fields, "volume_flow_m3h"))
    {
        addNumericFieldWithExtractor(
            "volume_flow",
            "Media volume flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
        );
    }

    if (checkIf(fields, "access_counter"))
    {
        addNumericFieldWithExtractor(
            "access",
            "Meter access counter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            DifSignedness::Unsigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AccessNumber)
        );
    }

    if (checkIf(fields,"consumption_hca"))
    {
        addNumericFieldWithExtractor(
            "consumption",
            "The current heat cost allocation for this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
        );
    }
}

const char* toString(VifScaling s)
{
    switch (s)
    {
    case VifScaling::Auto: return "Auto";
    case VifScaling::None: return "None";
    case VifScaling::Unknown: return "Unknown";
    }
    return "?";
}

VifScaling toVifScaling(const char* s)
{
    if (!s) return VifScaling::Unknown;
    if (!strcmp(s, "Auto")) return VifScaling::Auto;
    if (!strcmp(s, "None")) return VifScaling::None;
    if (!strcmp(s, "Unknown")) return VifScaling::Unknown;
    return VifScaling::Unknown;
}

const char *toString(DifSignedness s)
{
    switch (s)
    {
    case DifSignedness::Signed: return "Signed";
    case DifSignedness::Unsigned: return "Unsigned";
    case DifSignedness::Unknown: return "Unknown";
    }
    return "?";
}

DifSignedness toDifSignedness(const char *s)
{
    if (!s) return DifSignedness::Unknown;
    if (!strcmp(s, "Signed")) return DifSignedness::Signed;
    if (!strcmp(s, "Unsigned")) return DifSignedness::Unsigned;
    if (!strcmp(s, "Unknown")) return DifSignedness::Unknown;
    return DifSignedness::Unknown;
}

const char* toString(PrintProperty p)
{
    switch (p)
    {
    case PrintProperty::REQUIRED: return "REQUIRED";
    case PrintProperty::DEPRECATED: return "DEPRECATED";
    case PrintProperty::STATUS: return "STATUS";
    case PrintProperty::INCLUDE_TPL_STATUS: return "INCLUDE_TPL_STATUS";
    case PrintProperty::INJECT_INTO_STATUS: return "INJECT_INTO_STATUS";
    case PrintProperty::HIDE: return "HIDE";
    case PrintProperty::Unknown: return "Unknown";
    }

    return "Unknown";
}

PrintProperty toPrintProperty(const char* s)
{
    if (!strcmp(s, "REQUIRED")) return PrintProperty::REQUIRED;
    if (!strcmp(s, "DEPRECATED")) return PrintProperty::DEPRECATED;
    if (!strcmp(s, "STATUS")) return PrintProperty::STATUS;
    if (!strcmp(s, "INCLUDE_TPL_STATUS")) return PrintProperty::INCLUDE_TPL_STATUS;
    if (!strcmp(s, "INJECT_INTO_STATUS")) return PrintProperty::INJECT_INTO_STATUS;
    if (!strcmp(s, "HIDE")) return PrintProperty::HIDE;
    if (!strcmp(s, "Unknown")) return PrintProperty::Unknown;

    return PrintProperty::Unknown;
}

PrintProperties toPrintProperties(string s)
{
    auto fields = splitString(s, ',');

    int bits = 0;
    for (auto p : fields)
    {
        bits |= toPrintProperty(p.c_str());
    }

    return bits;
}

char available_meter_types_[2048];

const char* availableMeterTypes()
{
    if (available_meter_types_[0]) return available_meter_types_;

#define X(m) if (MeterType::m != MeterType::AutoMeter && MeterType::m != MeterType::UnknownMeter) {  \
        strcat(available_meter_types_, #m); strcat(available_meter_types_, "\n"); \
        assert(strlen(available_meter_types_) < 1024); }
    LIST_OF_METER_TYPES
#undef X

        // Remove last ,
        available_meter_types_[strlen(available_meter_types_) - 1] = 0;
    return available_meter_types_;
}
