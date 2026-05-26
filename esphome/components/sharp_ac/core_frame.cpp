#include <cstring>
#include "core_types.h"
#include "core_frame.h"
#include "core_state.h"

namespace esphome {
namespace sharp_ac {

SharpFrame::SharpFrame(char c) : size_(1)
{
    data_ = new uint8_t[size_];
    data_[0] = static_cast<uint8_t>(c);
}
SharpFrame::SharpFrame() : data_(nullptr), size_(0)
{
}

SharpFrame::SharpFrame(const uint8_t *arr, size_t sz) : size_(sz)
{
    data_ = new uint8_t[sz];
    memcpy(data_, arr, sz);
}

SharpFrame::~SharpFrame()
{
    delete[] data_;
}

SharpFrame::SharpFrame(const SharpFrame &other) : size_(other.size_)
{
    data_ = new uint8_t[size_];
    memcpy(data_, other.data_, size_);
}

uint8_t *SharpFrame::get_data() const
{
    return data_;
}

size_t SharpFrame::get_size() const
{
    return size_;
}

int SharpFrame::set_size(size_t sz)
{
    if (this->size_ == 0)
    {
        this->size_ = sz;
        this->data_ = new uint8_t[this->size_];
        return 1;
    }
    return 0;
}

void SharpFrame::print()
{
}

void SharpFrame::set_checksum()
{
    this->data_[size_ - 1] = calc_checksum();
}

bool SharpFrame::validate_checksum()
{
    return this->data_[size_ - 1] == calc_checksum();
}

uint8_t SharpFrame::calc_checksum()
{
    uint16_t sum = 0;
    for (size_t i = 1; i < this->size_ - 1; i++)
    {
        sum += this->data_[i];
        sum &= 0xFF;
    }
    uint8_t checksum = (uint8_t)((256 - sum) & 0xFF);
    return checksum;
}

SharpStatusFrame::SharpStatusFrame(const uint8_t *arr) : SharpFrame(arr, 18)
{
}

int SharpStatusFrame::get_temperature()
{
    return this->data_[7];
}

SharpModeFrame::SharpModeFrame(const uint8_t *arr) : SharpFrame(arr, 14)
{
}

int SharpModeFrame::get_temperature()
{
    // Temperature is encoded in lower nibble + 16 offset
    return (this->data_[4] & 0x0F) + 16;
}

bool SharpModeFrame::get_state()
{
    // Only check Bit 7 (0x80) for actual power state
    return (this->data_[8] & 0x80) != 0;
}

Preset SharpModeFrame::get_preset()
{
    // Response frames (0xFC): byte[7] bit-based (0x40=ECO, 0x80=FULLPOWER)
    // Command frames (0xFB): byte[7]=0x10 for ECO, byte[10]=0x01 for FULLPOWER
    if (this->data_[2] == 0xFC) {
        // Response frame format 
        if ((this->data_[7] & 0x40) == 0x40)
            return Preset::ECO; 
        else if ((this->data_[7] & 0x80) == 0x80)
            return Preset::FULLPOWER; 
    } else {
        // Command frame format (0xFB) 
        if (this->data_[10] == 0x01)
            return Preset::FULLPOWER;
        else if (this->data_[7] == 0x10)
            return Preset::ECO; 
    }
    
    return Preset::NONE;  
}


SwingVertical SharpModeFrame::get_swing_vertical()
{
    // Response frames (0xFC)
    // Command frames (0xFB)
    if (this->data_[2] == 0xFC)
        return static_cast<SwingVertical>(this->data_[6] & 0x0F);
    else  // 0xFB command frames
        return static_cast<SwingVertical>(this->data_[8] & 0x0F);
}

SwingHorizontal SharpModeFrame::get_swing_horizontal()
{
    // Response frames (0xFC)
    // Command frames (0xFB)
    if (this->data_[2] == 0xFC)
        return static_cast<SwingHorizontal>((this->data_[6] & 0xF0) >> 4);
    else  // 0xFB command frames
        return static_cast<SwingHorizontal>((this->data_[8] & 0xF0) >> 4);
}

FanMode SharpModeFrame::get_fan_mode()
{
    // Response frames (0xFC)
    // Command frames (0xFB)
    if (this->data_[2] == 0xFC)
        return static_cast<FanMode>((this->data_[5] & 0xF0) >> 4);
    else  // 0xFB command frames
        return static_cast<FanMode>((this->data_[6] & 0xF0) >> 4);
}

PowerMode SharpModeFrame::get_power_mode()
{
    // Response frames (0xFC)
    // Command frames (0xFB)
    if (this->data_[2] == 0xFC)
        return static_cast<PowerMode>(this->data_[5] & 0x0F);
    else  // 0xFB command frames
        return static_cast<PowerMode>(this->data_[6] & 0x0F);
}

bool SharpModeFrame::get_ion()
{
    // Check Bit 2 (0x04) for Ion/Plasmacluster state
    // 0x84, 0x94, 0x04 all have Ion ON
    return (this->data_[8] & 0x04) != 0;
}

SharpCommandFrame::SharpCommandFrame() : SharpFrame()
{
    this->set_size(14);

    this->data_[0] = 0xdd;
    this->data_[1] = 0x0b;
    this->data_[2] = 0xfb;
    this->data_[3] = 0x60;
    this->data_[7] = 0x00;
    this->data_[9] = 0x00;
    this->data_[10] = 0x00;
    this->data_[11] = 0xe4;
}

void SharpCommandFrame::set_data(SharpState *state)
{
    switch (state->mode)
    {
    case PowerMode::FAN:
    {
        this->data_[4] = 0x01;
        break;
    }
    case PowerMode::DRY:
    {
        this->data_[4] = 0x00;
        break;
    }
    case PowerMode::COOL:
    {
        this->data_[4] = 0xC0 | (state->temperature - 15);
        break;
    }
    case PowerMode::HEAT:
    {
        this->data_[4] = 0xC0 | (state->temperature - 15);
        break;
    }
    case PowerMode::heat: {
      this->data[4] = 0xC0 | (state->temperature - 15);
      break;
    }
  }

    // Byte 6: Mode (lower nibble) + Fan (upper nibble)
    this->data_[6] = (uint8_t) state->mode;
    if (state->mode == PowerMode::FAN && state->fan == FanMode::FAN_AUTO)
        this->data_[6] |= (uint8_t) FanMode::FAN_LOW << 4;
    else if (state->preset == Preset::FULLPOWER)
        this->data_[6] |= (uint8_t) FanMode::FAN_AUTO << 4;
    else
        this->data_[6] |= (uint8_t) state->fan << 4;

    // Byte 5: State indicator
    if (state->state) {
        if (state->preset == Preset::NONE)
            this->data_[5] = 0x31;
        else
            this->data_[5] = 0x61;
    }
    else
        this->data_[5] = 0x21;

    // Ion mode
    if (state->ion)
    {
        this->data_[11] = 0xE4;
    }
    else
    {
        this->data_[11] = 0x10;
    }
    
    // Preset handling
    // From working example
    if (state->preset == Preset::FULLPOWER) {
        this->data_[10] = 0x01;
    } else if (state->preset == Preset::ECO) {
        this->data_[7] = 0x10;
    }

    // Swing data in byte 8
    // From working example
    this->data_[8] = ((uint8_t) state->swingH << 4) | (uint8_t) state->swingV;
}

void SharpCommandFrame::set_checksum()
{
    command_checksum_();
    SharpFrame::set_checksum();
}

void SharpCommandFrame::command_checksum_()
{
    uint8_t checksum = 0x3;

    for (int i = 4; i < 12; i++)
    {
        checksum ^= this->data_[i] & 0x0F;
        checksum ^= (data_[i] >> 4) & 0x0F;
    }
    checksum = 0xF - (checksum & 0x0F);
    this->data_[12] = (checksum << 4) | 0x01;
}

SharpACKFrame::SharpACKFrame() : SharpFrame(0x06) {}

void SharpACKFrame::set_checksum()
{
}

}  // namespace sharp_ac
}  // namespace esphome
