#include "bl0906.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bl0906 {

static const char *const TAG = "bl0906";
uint8_t USR_WRPROT_Witable[6] = {0xCA, 0x9E, 0x55, 0x55, 0x00, 0xB7};
uint8_t USR_WRPROT_Onlyread[6] = {0xCA, 0x9E, 0x00, 0x00, 0x00, 0x61};

static const uint8_t BL0906_READ_COMMAND = 0x35;
static const uint8_t BL0906_WRITE_COMMAND = 0xCA;

// Register address
// Voltage
static const uint8_t BL0906_V_RMS = 0x16;

// Total power
static const uint8_t BL0906_WATT_SUM = 0X2C;

// Current1~6
static const uint8_t BL0906_I_1_RMS = 0x0D;  // current_1
static const uint8_t BL0906_I_2_RMS = 0x0E;
static const uint8_t BL0906_I_3_RMS = 0x0F;
static const uint8_t BL0906_I_4_RMS = 0x10;
static const uint8_t BL0906_I_5_RMS = 0x13;
static const uint8_t BL0906_I_6_RMS = 0x14;  // current_6

// Power1~6
static const uint8_t BL0906_WATT_1 = 0X23;  // power_1
static const uint8_t BL0906_WATT_2 = 0X24;
static const uint8_t BL0906_WATT_3 = 0X25;
static const uint8_t BL0906_WATT_4 = 0X26;
static const uint8_t BL0906_WATT_5 = 0X29;
static const uint8_t BL0906_WATT_6 = 0X2A;  // power_6

// Active pulse count, unsigned
static const uint8_t BL0906_CF_1_CNT = 0X30;  // Channel_1
static const uint8_t BL0906_CF_2_CNT = 0X31;
static const uint8_t BL0906_CF_3_CNT = 0X32;
static const uint8_t BL0906_CF_4_CNT = 0X33;
static const uint8_t BL0906_CF_5_CNT = 0X36;
static const uint8_t BL0906_CF_6_CNT = 0X37;  // Channel_6

// Total active pulse count, unsigned
static const uint8_t BL0906_CF_SUM_CNT = 0X39;

// Voltage frequency cycle
static const uint8_t BL0906_FREQUENCY = 0X4E;

// Internal temperature
static const uint8_t BL0906_TEMPERATURE = 0X5E;

// Calibration register
// RMS gain adjustment register
static const uint8_t BL0906_RMSGN_1 = 0x6D;  // Channel_1
static const uint8_t BL0906_RMSGN_2 = 0x6E;
static const uint8_t BL0906_RMSGN_3 = 0x6F;
static const uint8_t BL0906_RMSGN_4 = 0x70;
static const uint8_t BL0906_RMSGN_5 = 0x73;
static const uint8_t BL0906_RMSGN_6 = 0x74;  // Channel_6

// RMS offset correction register
static const uint8_t BL0906_RMSOS_1 = 0x78;  // Channel_1
static const uint8_t BL0906_RMSOS_2 = 0x79;
static const uint8_t BL0906_RMSOS_3 = 0x7A;
static const uint8_t BL0906_RMSOS_4 = 0x7B;
static const uint8_t BL0906_RMSOS_5 = 0x7E;
static const uint8_t BL0906_RMSOS_6 = 0x7F;  // Channel_6

// Active power gain adjustment register
static const uint8_t BL0906_WATTGN_1 = 0xB7;  // Channel_1
static const uint8_t BL0906_WATTGN_2 = 0xB8;
static const uint8_t BL0906_WATTGN_3 = 0xB9;
static const uint8_t BL0906_WATTGN_4 = 0xBA;
static const uint8_t BL0906_WATTGN_5 = 0xBD;
static const uint8_t BL0906_WATTGN_6 = 0xBE;  // Channel_6

// User write protection setting register,
// You must first write 0x5555 to the write protection setting register before writing to other registers.
static const uint8_t BL0906_USR_WRPROT = 0x9E;

// Reset Register
static const uint8_t BL0906_SOFT_RESET = 0x9F;

const uint8_t BL0906_INIT[2][6] = {
    // Reset to default
    {BL0906_WRITE_COMMAND, BL0906_SOFT_RESET, 0x5A, 0x5A, 0x5A, 0x52},
    // Enable User Operation Write
    {BL0906_WRITE_COMMAND, BL0906_USR_WRPROT, 0x55, 0x55, 0x00, 0xB7}};

void BL0906::loop() {
  if (this->current_channel_ == UINT8_MAX) {
    return;
  }

  while (this->available())
    this->flush();

  if (this->current_channel_ == 0) {
    // Temperature
    read_data(BL0906_TEMPERATURE, temperature_reference_, temperature_sensor_);
  } else if (this->current_channel_ == 1) {
    read_data(BL0906_I_1_RMS, current_reference_, current_sensor_1_);
    read_data(BL0906_WATT_1, power_reference_, power_sensor_1_);
    read_data(BL0906_CF_1_CNT, energy_reference_, energy_sensor_1_);
  } else if (this->current_channel_ == 2) {
    read_data(BL0906_I_2_RMS, current_reference_, current_sensor_2_);
    read_data(BL0906_WATT_2, power_reference_, power_sensor_2_);
    read_data(BL0906_CF_2_CNT, energy_reference_, energy_sensor_2_);
  } else if (this->current_channel_ == 3) {
    read_data(BL0906_I_3_RMS, current_reference_, current_sensor_3_);
    read_data(BL0906_WATT_3, power_reference_, power_sensor_3_);
    read_data(BL0906_CF_3_CNT, energy_reference_, energy_sensor_3_);
  } else if (this->current_channel_ == 4) {
    read_data(BL0906_I_4_RMS, current_reference_, current_sensor_4_);
    read_data(BL0906_WATT_4, power_reference_, power_sensor_4_);
    read_data(BL0906_CF_4_CNT, energy_reference_, energy_sensor_4_);
  } else if (this->current_channel_ == 5) {
    read_data(BL0906_I_5_RMS, current_reference_, current_sensor_5_);
    read_data(BL0906_WATT_5, power_reference_, power_sensor_5_);
    read_data(BL0906_CF_5_CNT, energy_reference_, energy_sensor_5_);
  } else if (this->current_channel_ == 6) {
    read_data(BL0906_I_6_RMS, current_reference_, current_sensor_6_);
    read_data(BL0906_WATT_6, power_reference_, power_sensor_6_);
    read_data(BL0906_CF_6_CNT, energy_reference_, energy_sensor_6_);
  } else if (this->current_channel_ == UINT8_MAX - 2) {
    // Frequency
    read_data(BL0906_FREQUENCY, frequency_reference_, frequency_sensor_);
    // Voltage
    read_data(BL0906_V_RMS, voltage_reference_, voltage_sensor_);
  } else if (this->current_channel_ == UINT8_MAX - 1) {
    // Total power
    read_data(BL0906_WATT_SUM, sum_power_reference_, power_sensor_sum_);
    // Total Energy
    read_data(BL0906_CF_SUM_CNT, sum_energy_reference_, energy_sensor_sum_);
  } else {
    this->current_channel_ = UINT8_MAX - 2;  // Go to frequency and voltage
    return;
  }
  this->current_channel_++;
  handleActionCallback();
}

void BL0906::setup() {
  while (this->available())
    this->flush();
  this->write_array(USR_WRPROT_Witable, sizeof(USR_WRPROT_Witable));
  // Calibration (1: register address; 2: value before calibration; 3: value after calibration)
  Bias_correction(BL0906_RMSOS_1, 0.01600, 0);  // Calibration current_1
  Bias_correction(BL0906_RMSOS_2, 0.01500, 0);
  Bias_correction(BL0906_RMSOS_3, 0.01400, 0);
  Bias_correction(BL0906_RMSOS_4, 0.01300, 0);
  Bias_correction(BL0906_RMSOS_5, 0.01200, 0);
  Bias_correction(BL0906_RMSOS_6, 0.01200, 0);  // Calibration current_6

  // gain_correction(BL0906_RMSGN_1, 2.15000, 2.148, BL0906_ki);   //RMS gain adjustment current_1
  // gain_correction(BL0906_RMSGN_2, 2.15100, 2.148, BL0906_ki);
  // gain_correction(BL0906_RMSGN_3, 2.15200, 2.148, BL0906_ki);
  // gain_correction(BL0906_RMSGN_4, 2.14500, 2.148, BL0906_ki);
  // gain_correction(BL0906_RMSGN_5, 2.14600, 2.148, BL0906_ki);
  // gain_correction(BL0906_RMSGN_6, 2.14600, 2.148, BL0906_ki);   //RMS gain adjustment current_6

  // gain_correction(BL0906_WATTGN_1, 15.13427, 14.5, BL0906_Kp);  //Active power gain adjustment power_1
  // gain_correction(BL0906_WATTGN_2, 15.23937, 14.5, BL0906_Kp);
  // gain_correction(BL0906_WATTGN_3, 15.44956, 14.5, BL0906_Kp);
  // gain_correction(BL0906_WATTGN_4, 16.57646, 14.5, BL0906_Kp);
  // gain_correction(BL0906_WATTGN_5, 15.27440, 14.5, BL0906_Kp);
  // gain_correction(BL0906_WATTGN_6, 31.75744, 14.5, BL0906_Kp);  //Active power gain adjustment power_6
  this->write_array(USR_WRPROT_Onlyread, sizeof(USR_WRPROT_Onlyread));
}

void BL0906::update() { this->current_channel_ = 0; }

// The SUM byte is (Addr+Data_L+Data_M+Data_H)&0xFF negated;
uint8_t bl0906_checksum(const uint8_t address, const DataPacket *data) {
  return (address + data->l + data->m + data->h) ^ 0xFF;
}

int BL0906::addActionCallBack(ActionCallbackFuncPtr ptrFunc) {
  m_vecActionCallback.push_back(ptrFunc);
  return m_vecActionCallback.size();
}

void BL0906::handleActionCallback() {
  if (m_vecActionCallback.size() == 0) {
    return;
  }
  ActionCallbackFuncPtr ptrFunc = nullptr;
  for (int i = 0; i < m_vecActionCallback.size(); i++) {
    ptrFunc = m_vecActionCallback[i];
    if (ptrFunc) {
      ESP_LOGI(TAG, "HandleActionCallback[%d]...", i);
      (this->*ptrFunc)();
    }
  }

  while (this->available()) {
    this->read();
  }

  m_vecActionCallback.clear();
  if (m_process_state != PROCESS_DONE) {
    m_process_state = PROCESS_DONE;
  }
}

// Reset energy
void BL0906::reset_energy() {
  this->write_array(BL0906_INIT[0], 6);
  delay(1);
  this->flush();

  ESP_LOGW(TAG, "RMSOS:%02X%02X%02X%02X%02X%02X", BL0906_INIT[0][0], BL0906_INIT[0][1], BL0906_INIT[0][2],
           BL0906_INIT[0][3], BL0906_INIT[0][4], BL0906_INIT[0][5]);
}

// Read data
void BL0906::read_data(const uint8_t address, const float reference, sensor::Sensor *sensor) {
  if (sensor == nullptr) {
    return;
  }
  DataPacket buffer;
  ube24_t data_u24;
  sbe24_t data_s24;
  float value = 0;

  bool signed_result =
      reference == temperature_reference_ || reference == sum_power_reference_ || reference == power_reference_;

  this->write_byte(BL0906_READ_COMMAND);
  this->write_byte(address);
  if (this->read_array((uint8_t *) &buffer, sizeof(buffer) - 1)) {
    if (bl0906_checksum(address, &buffer) == buffer.checksum) {
      if (signed_result) {
        data_s24.l = buffer.l;
        data_s24.m = buffer.m;
        data_s24.h = buffer.h;
      } else {
        data_u24.l = buffer.l;
        data_u24.m = buffer.m;
        data_u24.h = buffer.h;
      }
    } else {
      ESP_LOGW(TAG, "Junk on wire. Throwing away partial message");
      while (read() >= 0)
        ;
      return;
    }
  }
  // Power
  if (reference == power_reference_) {
    value = (float) to_int32_t(data_s24) * reference;
  }

  // Total power
  if (reference == sum_power_reference_) {
    value = (float) to_int32_t(data_s24) * reference;
  }

  // Voltage, current, power, total power
  if (reference == voltage_reference_ || reference == current_reference_ || reference == energy_reference_ ||
      reference == sum_energy_reference_) {
    value = (float) to_uint32_t(data_u24) * reference;
  }

  // Frequency
  if (reference == frequency_reference_) {
    value = reference / (float) to_uint32_t(data_u24);
  }
  // Chip temperature
  if (reference == temperature_reference_) {
    value = (float) to_int32_t(data_s24);
    value = (value - 64) * 12.5 / 59 - 40;
  }
  sensor->publish_state(value);
}

// RMS offset correction
void BL0906::Bias_correction(const uint8_t address, const float measurements, const float Correction) {
  DataPacket data;
  float ki = 12875 * 1 * (5.1 + 5.1) * 1000 / 2000 / 1.097;  // Current coefficient
  float I_RMS0 = measurements * ki;
  float I_RMS = Correction * ki;
  int32_t value = (I_RMS * I_RMS - I_RMS0 * I_RMS0) / 256;
  data.l = value << 24 >> 24;
  data.m = value << 16 >> 24;
  if (value < 0) {
    data.h = (value << 8 >> 24) | 0b10000000;
  }
  data.address = bl0906_checksum(address, &data);
  ESP_LOGW(TAG, "RMSOS:%02X%02X%02X%02X%02X%02X", BL0906_WRITE_COMMAND, address, data.l, data.m, data.h, data.address);
  this->write_byte(BL0906_WRITE_COMMAND);
  this->write_byte(address);
  this->write_byte(data.l);
  this->write_byte(data.m);
  this->write_byte(data.h);
  this->write_byte(data.address);
}
// Gain adjustment
void BL0906::gain_correction(const uint8_t address, const float measurements, const float Correction,
                             const float coefficient) {
  DataPacket data;
  float I_RMS0 = measurements * coefficient;
  float I_RMS = Correction * coefficient;
  float rms_gn = int((I_RMS / I_RMS0 - 1) * 65536);
  int16_t value;
  if (rms_gn <= -32767) {
    value = -32767;
  } else {
    value = int(rms_gn);
  }
  data.h = 0xFF;
  data.m = value >> 8;
  data.l = value << 8 >> 8;
  data.address = bl0906_checksum(address, &data);
  // ESP_LOGW(TAG, "RMSOS:%02X%02X%02X%02X%02X%02X",BL0906_WRITE_COMMAND,address,data.l ,data.m, data.h,data.address
  // );
  this->write_byte(BL0906_WRITE_COMMAND);
  this->write_byte(address);
  this->write_byte(data.l);
  this->write_byte(data.m);
  this->write_byte(data.h);
  this->write_byte(data.address);
}

void BL0906::dump_config() {
  ESP_LOGCONFIG(TAG, "BL0906:");
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current1", this->current_sensor_1_);
  LOG_SENSOR("  ", "Current2", this->current_sensor_2_);
  LOG_SENSOR("  ", "Current3", this->current_sensor_3_);
  LOG_SENSOR("  ", "Current4", this->current_sensor_4_);
  LOG_SENSOR("  ", "Current5", this->current_sensor_5_);
  LOG_SENSOR("  ", "Current6", this->current_sensor_6_);

  LOG_SENSOR("  ", "Power1", this->power_sensor_1_);
  LOG_SENSOR("  ", "Power2", this->power_sensor_2_);
  LOG_SENSOR("  ", "Power3", this->power_sensor_3_);
  LOG_SENSOR("  ", "Power4", this->power_sensor_4_);
  LOG_SENSOR("  ", "Power5", this->power_sensor_5_);
  LOG_SENSOR("  ", "Power6", this->power_sensor_6_);

  LOG_SENSOR("  ", "Energy1", this->energy_sensor_1_);
  LOG_SENSOR("  ", "Energy2", this->energy_sensor_2_);
  LOG_SENSOR("  ", "Energy3", this->energy_sensor_3_);
  LOG_SENSOR("  ", "Energy4", this->energy_sensor_4_);
  LOG_SENSOR("  ", "Energy5", this->energy_sensor_5_);
  LOG_SENSOR("  ", "Energy6", this->energy_sensor_6_);
  LOG_SENSOR("  ", "Energy sum", this->energy_sensor_sum_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

uint32_t BL0906::to_uint32_t(ube24_t input) { return input.h << 16 | input.m << 8 | input.l; }

int32_t BL0906::to_int32_t(sbe24_t input) { return input.h << 16 | input.m << 8 | input.l; }

}  // namespace bl0906
}  // namespace esphome
