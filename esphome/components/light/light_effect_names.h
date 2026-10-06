#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include "light_effect.h"

namespace esphome::light {

/// Effect names as the native API lists them: "None" first, then each effect; empty without effects.
/// Reads the light's effect table directly, so listing entities allocates nothing.
class LightEffectNames {
 public:
  explicit LightEffectNames(const ConstVector<LightEffect *> &effects) : effects_(effects) {}

  class Iterator {
   public:
    Iterator(const ConstVector<LightEffect *> &effects, size_t index) : effects_(effects), index_(index) {}
    StringRef operator*() const {
      return this->index_ == 0 ? StringRef::from_lit("None") : this->effects_[this->index_ - 1]->get_name();
    }
    Iterator &operator++() {
      ++this->index_;
      return *this;
    }
    bool operator!=(const Iterator &other) const { return this->index_ != other.index_; }

   protected:
    const ConstVector<LightEffect *> &effects_;
    size_t index_;
  };

  Iterator begin() const { return {this->effects_, 0}; }
  Iterator end() const { return {this->effects_, this->effects_.empty() ? 0 : this->effects_.size() + 1}; }
  bool empty() const { return this->effects_.empty(); }

 protected:
  const ConstVector<LightEffect *> &effects_;
};

}  // namespace esphome::light
