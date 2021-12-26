

class TM1638Key : public binary_sensor::BinarySensor {
  friend class TM1638Component;

 public:
  void set_keycode(uint8_t key_code) { key_code_ = key_code; }  //needed for binary sensor
  void process(uint8_t data)

 protected:
  uint8_t key_code_{0};
};
