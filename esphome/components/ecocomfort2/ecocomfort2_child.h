#pragma once

#include "esphome/core/helpers.h"

namespace esphome::ecocomfort2 {

class Ecocomfort2Hub;

class Ecocomfort2Client : public Parented<Ecocomfort2Hub> {
 public:
  /// Called after each successful C_OPER readback.
  virtual void on_status() = 0;
};

}  // namespace esphome::ecocomfort2
