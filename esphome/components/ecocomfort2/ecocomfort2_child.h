#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace ecocomfort2 {

// Forward declare hub
class Ecocomfort2Hub;

class Ecocomfort2Client : public Parented<Ecocomfort2Hub> {
 public:
  virtual void on_status() = 0;
  virtual void on_config() = 0;
  virtual void on_connect(bool connected) = 0;

 protected:
  friend Ecocomfort2Hub;
  virtual const char *describe() const = 0;
};

}  // namespace ecocomfort2
}  // namespace esphome
