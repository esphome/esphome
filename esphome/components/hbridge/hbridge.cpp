#include "hbridge.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

static const char *const TAG = "hbridge";

void HBridge::setup() {
    //Always start in off-state
    hbridge_set_state(HBRIDGE_MODE_OFF, 0);
}


void HBridge::loop() {

  //If we are ramping to a certain dutycycle
  if(this->ramp_to_mode != HBRIDGE_RAMP_IDLE){
      
    //Calculate the time between this iteration and the previous one
    uint32_t milliseconds_after_last_adjustment = millis() - ramp_to_last_step_time_;

    //If the step is more than 1 ms ago
    if(milliseconds_after_last_adjustment > 0){
      //Calculate new dutycycle for this 'time-point' in the slope.
      float new_dutycycle = this->current_dutycycle_ + (milliseconds_after_last_adjustment * this->ramp_to_dutycycle_step_per_ms_);

      //limit to target dutycycle (mind for down/up slope)
      if(this->ramp_to_mode == HBRIDGE_RAMP_UP){
        if(new_dutycycle >= this->ramp_to_target_dutycycle_){
          //We are done with this ramp-up
          new_dutycycle = this->ramp_to_target_dutycycle_;
          this->ramp_to_mode = HBRIDGE_RAMP_IDLE;
        }
      }
      else if(this->ramp_to_mode == HBRIDGE_RAMP_DOWN){
        if(new_dutycycle <= this->ramp_to_target_dutycycle_){
          //We are done with this ramp-down
          new_dutycycle = this->ramp_to_target_dutycycle_;
          this->ramp_to_mode = HBRIDGE_RAMP_IDLE;
        }
      }

      //Set new state to outputs
      hbridge_set_output_state(this->current_mode_, new_dutycycle);

      //Store last step time
      ramp_to_last_step_time_ = millis();
    }
  }
}


void HBridge::hbridge_set_state(HBridgeMode mode, float dutycycle){   
    //Print info on mode change
    if(this->current_mode_ != mode || this->current_dutycycle_ != dutycycle){
      ESP_LOGD(TAG, "Set mode %d - Dutycycle: %.2f", mode, dutycycle);
    }

    //Cancel possible ongoing ramp
    this->ramp_to_mode = HBRIDGE_RAMP_IDLE;
    
    //Set state to outputs
    hbridge_set_output_state(mode, dutycycle);
}

void HBridge::hbridge_ramp_to_dutycycle(float target_dutycycle, uint32_t ramp_duration_ms){
    //Store state (ramp-to-dutycycle mode)
    this->ramp_to_duration_ms_ = ramp_duration_ms;
    this->ramp_to_dutycycle_step_per_ms_ = this->current_dutycycle_;
    this->ramp_to_target_dutycycle_ = target_dutycycle;
    
    if(this->ramp_to_target_dutycycle_ > this->current_dutycycle_){
      //Ramp-up
      this->ramp_to_mode = HBRIDGE_RAMP_UP;
    }
    else if(this->ramp_to_target_dutycycle_ < this->current_dutycycle_){
      //Ramp-down
      this->ramp_to_mode = HBRIDGE_RAMP_DOWN;
    }
    else{
      //Target is equal, no ramp needed.
      this->ramp_to_mode = HBRIDGE_RAMP_IDLE;
    }
    

    ESP_LOGD(TAG, "Ramp from dutycycle: %.2f to dutycycle: %.2f in %d ms", this->current_dutycycle_, this->ramp_to_target_dutycycle_, this->ramp_to_duration_ms_);
}

void HBridge::hbridge_set_output_state(HBridgeMode mode, float dutycycle){   
    HBridgeMode new_mode = mode;
    float new_dutycycle = dutycycle;

    if(new_dutycycle <= 0){
        //If dutycycle is 0 or negative, force off
        new_mode = HBRIDGE_MODE_OFF;
        new_dutycycle = 0;
    }
    
    //Set pin states + duty cycle according to mode
    switch(new_mode){
          case HBRIDGE_MODE_OFF:
            if(this->enable_pin_ != nullptr){ this->enable_pin_->set_level(0); }
            this->pin_a_->set_level(0);
            this->pin_b_->set_level(0);
            break;

          case HBRIDGE_MODE_DIRECTION_A:
            this->pin_b_->set_level(0);
            this->pin_a_->set_level(new_dutycycle);
            if(this->enable_pin_ != nullptr){ this->enable_pin_->set_level(1); }            
            break;

          case HBRIDGE_MODE_DIRECTION_B:
            this->pin_a_->set_level(0);
            this->pin_b_->set_level(new_dutycycle);
            if(this->enable_pin_ != nullptr){ this->enable_pin_->set_level(1); }
            break;

          case HBRIDGE_MODE_SHORT:
            this->pin_a_->set_level(1);
            this->pin_b_->set_level(1);
            if(this->enable_pin_ != nullptr){ this->enable_pin_->set_level(1); }
            break;
    }


    //Store current values
    this->current_mode_ = new_mode;
    this->current_dutycycle_ = new_dutycycle;
}

}  // namespace hbridge
}  // namespace esphome
