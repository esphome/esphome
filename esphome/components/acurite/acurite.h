#pragma once

#include "esphome/components/remote_receiver/remote_receiver.h"

namespace esphome {
namespace acurite {

class AcuRiteDevice {
 public:
  virtual void update_battery(uint8_t value) {}
  virtual void update_speed(float value) {}
  virtual void update_direction(float value) {}
  virtual void update_temperature(float value) {}
  virtual void update_humidity(float value) {}
  virtual void update_distance(float value) {}
  virtual void update_rainfall(uint32_t count) {}
  virtual void update_lightning(uint32_t count) {}
  virtual void update_uv(float value) {}
  virtual void update_lux(float value) {}
  void set_id(uint16_t id) { this->id_ = id; }
  uint16_t get_id() { return this->id_; }

 protected:
  uint16_t id_{0};
};

class AcuRiteComponent : public Component, public remote_base::RemoteReceiverListener {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void add_device(AcuRiteDevice *dev, uint16_t id) { this->devices_.push_back(dev); }
  void set_srcrecv(remote_receiver::RemoteReceiverComponent *recv) { this->remote_receiver_ = recv; }
  bool on_receive(remote_base::RemoteReceiveData data) override;

 protected:
  void decode_temperature_(uint8_t *data, uint8_t len);
  void decode_rainfall_(uint8_t *data, uint8_t len);
  void decode_lightning_(uint8_t *data, uint8_t len);
  void decode_atlas_(uint8_t *data, uint8_t len);
  void decode_notos_(uint8_t *data, uint8_t len);
  void decode_iris_(uint8_t *data, uint8_t len);
  bool validate_(uint8_t *data, uint8_t len, int8_t except);
  remote_receiver::RemoteReceiverComponent *remote_receiver_{nullptr};
  std::vector<AcuRiteDevice *> devices_;
};

}  // namespace acurite
}  // namespace esphome
