#ifndef MITSUBISHIITP_MHK
#define MITSUBISHIITP_MHK

#include <cmath>

namespace esphome {
namespace mitsubishi_itp {

/// A struct that represents the connected MHK's state for management and synchronization purposes.
struct MHKState {
  float cool_setpoint_ = NAN;
  float heat_setpoint_ = NAN;
};

}  // namespace mitsubishi_itp
}  // namespace esphome

#endif  // MITSUBISHIITP_MHK