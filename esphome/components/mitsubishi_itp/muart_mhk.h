namespace esphome {
namespace mitsubishi_itp {

/// A struct that represents the connected MHK's state for management and synchronization purposes.
struct MHKState {
  float cool_setpoint_ = NAN;
  float heat_setpoint_ = NAN;
};

}  // namespace mitsubishi_itp
}  // namespace esphome
