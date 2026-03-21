#include "dbus_text_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "dbus-print-message.h"

namespace esphome {
namespace dbus {

static const char *const TAG = "dbus.text_sensor";

DBusTextSensor::~DBusTextSensor() {}

void DBusTextSensor::setup() {
  printf("[%s] DBusTextSensor::setup()\n", this->get_name().c_str());
  ESP_LOGV(TAG, "%s  DBusTextSensor::setup(): '%s'", prefix, (obj)->unique_id().c_str());
  this->publish_state("DBusTextSensor::setup()");

  this->DBusWrapper::setup();
  printf("[%s] DBusTextSensor::setup() conn_=%p\n", this->get_name().c_str(), this->conn_);
  this->update();
  this->DBusWrapper::register_for_signal_(this->dbus_interface_, this->dbus_path_);
}

void DBusTextSensor::update() {
  printf("[%s] DBusTextSensor::update() destination:%s path:%s interface.method:%s.%s properties:[%d]\n",
         this->get_name().c_str(), this->dbus_destination_.c_str(), this->dbus_path_.c_str(),
         this->dbus_interface_.c_str(), this->dbus_method_.c_str(), this->properties_.size());
  ESP_LOGV(TAG, "%s  DBusTextSensor::update(): '%s'", prefix, (obj)->unique_id().c_str());

  std::string ret =
      DBusWrapper::send(this->dbus_destination_, this->dbus_path_, this->dbus_interface_, this->dbus_method_,
                        this->dbus_args_, this->properties_, this->property_separator_);

  // this->publish_state("dbus sync data!");
  printf("[%s] DBusTextSensor::update()   ==================== sending reply:%s\n", this->get_name().c_str(),
         ret.c_str());
  this->publish_state(ret);
}

float DBusTextSensor::get_setup_priority() const { return setup_priority::DATA; }
// void DBusTextSensor::set_hide_timestamp(bool hide_timestamp) { this->hide_timestamp_ = hide_timestamp; }
void DBusTextSensor::dump_config() { LOG_TEXT_SENSOR("", "dbus Text Sensor", this); }

void DBusTextSensor::loop() {
  // spam
  // printf("[%s] DBusTextSensor::loop\n", this->get_name().c_str());
  dbus_connection_read_write(this->conn_, 0);
  DBusMessage *msg = dbus_connection_pop_message(this->conn_);
  char *sigvalue = NULL;

  // loop again if we haven't read a message
  if (NULL == msg) {
    // printf("no dbus signal\n");
    return;
  }
  printf("[%s] DBusTextSensor::loop Got Signal\n", this->get_name().c_str());
  // print_message(msg, true);
  dbus_message_unref(msg);
  printf("[%s] DBusTextSensor::loop update\n", this->get_name().c_str());
  this->update();
  printf("[%s] DBusTextSensor::loop done\n", this->get_name().c_str());
}

}  // namespace dbus
}  // namespace esphome
