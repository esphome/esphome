#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "constants.h"
#include "q3da_parser.h"

namespace esphome {
namespace q3da {

static const char *const TAG = "q3da";

ObisInfo::ObisInfo(bytes server_id, char* str) { //: server_id(std::move(server_id)) {
  //char* str = (char*)data.c_str();        // convert to c_str
  
  this->server_id = server_id;

  char* first  = strtok(str, "(");        // split into first half (before '(')
  char* second = strtok(NULL, "(");       // and second half

  char *token     = strtok(first, "-");
  this->medium    = (uint8_t)atoi(token); // Value Group A: Electricity = 1, Gas = 7, ...
  token     = strtok(NULL, "-");

  token     = strtok(token, ":");
  this->channels  = (uint8_t)atoi(token); // Value Group B: internal or external
  token     = strtok(NULL, ":");

  token     = strtok(token, ".");
  this->unit      = (uint8_t)atoi(token); // Value Group C: Active, Reactive, ...
  token     = strtok(NULL, ".");
  this->type      = (uint8_t)atoi(token); // Value Group D: Kind of measurement: Max, current, ...
  token     = strtok(NULL, ".");

  token     = strtok(token, "*");
  this->tariff    = (uint8_t)atoi(token); // Value Group E: Kind of tariff: Total, Tarif1, Tarif2, ...
  token     = strtok(NULL, "*");
  this->scaler    = (uint8_t)atoi(token); // Value Group F: 00..99, 255 = ignored?

  int star = strchr(second, '*')-second+1;
  //ESP_LOGV(TAG, "%s", second);

  if (star > 0) {         // we have found a '*' = 42, so a float is in the rest
    token  = strtok(second, "*");
    this->value = (float)atof(token);     // Actual Value for example 1234.5
    second = strtok(NULL, "*");
  } else {
    this->value = 0.0/0.0;
  }

/*  ESP_LOGV(TAG, "%s", second);
  token     = strtok(second, ")");
  strcpy(this->symbol, token); // Copy the rest of the data and without the last ')'*/
}

Q3DATelegram::Q3DATelegram(const bytes buffer) {//: buffer_(std::move(buffer)) {
  std::string data;
  data.assign(buffer.begin(), buffer.end());
  char* str = (char*)data.c_str();

  this->messages.clear();

  std::vector<char*> tokens;
  tokens.clear();

  // First line is the type / name of the smartmeter
  char* token = strtok(str, "\r\n");
  token++;  

  bytes server_id;
  for (int i = 0; i < strlen(token); i++) {
    server_id.push_back(token[i]);
  }

  while (token != NULL) {
    token = strtok(NULL, "\r\n");
    
    if (token != NULL && token[0] != END_MASK) {
      //this->messages.emplace_back(obis);
      tokens.push_back(token);
    }
  }

  for (auto & token: tokens) {
      this->messages.emplace_back(ObisInfo(server_id, token));
  }
  //ESP_LOGV(TAG, "Found %d msg", this->messages.size());

}

std::string bytes_repr(const bytes &buffer) {
  std::string repr;
  for (auto const value : buffer) {
    repr += str_sprintf("%02x", value & 0xff);
  }
  return repr;
}

std::vector<ObisInfo> Q3DATelegram::get_obis_info() {
  return this->messages;
}

std::string ObisInfo::code_repr() const {
  return str_sprintf("%d-%d:%d.%d.%d", this->medium, this->channels, this->unit, this->type, this->tariff);
}

}  // namespace q3da
}  // namespace esphome
