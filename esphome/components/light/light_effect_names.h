#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include "light_effect.h"

namespace esphome::light {

/// Effect names as the native API lists them: "None" first, then each effect; empty without effects.
/// Reads the light's effect table directly, so listing entities allocates nothing. Holds a reference:
/// it only lives for one synchronous encode, and by value costs flash on every copy.
class LightEffectNames {
 public:
  explicit LightEffectNames(const ConstVector<LightEffect *> &effects) : effects_(effects) {}

  class Iterator {
   public:
    ESPHOME_ALWAYS_INLINE Iterator(const ConstVector<LightEffect *> &effects, size_t index)
        : effects_(effects), index_(index) {}
    ESPHOME_ALWAYS_INLINE StringRef operator*() const {
      return this->index_ == 0 ? EFFECT_NONE_REF : this->effects_[this->index_ - 1]->get_name();
    }
    ESPHOME_ALWAYS_INLINE Iterator &operator++() {
      ++this->index_;
      return *this;
    }
    ESPHOME_ALWAYS_INLINE bool operator!=(const Iterator &other) const { return this->index_ != other.index_; }

   protected:
    const ConstVector<LightEffect *> &effects_;
    size_t index_;
  };

  ESPHOME_ALWAYS_INLINE Iterator begin() const { return {this->effects_, 0}; }
  ESPHOME_ALWAYS_INLINE Iterator end() const {
    return {this->effects_, this->effects_.empty() ? 0 : this->effects_.size() + 1};
  }
  ESPHOME_ALWAYS_INLINE bool empty() const { return this->effects_.empty(); }

 protected:
  const ConstVector<LightEffect *> &effects_;
};

}  // namespace esphome::light
