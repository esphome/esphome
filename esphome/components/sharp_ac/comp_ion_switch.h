#pragma once

#include "esphome/components/switch/switch.h"
#include "comp_climate.h"

namespace esphome
{
   namespace sharp_ac
   {

      class IonSwitch : public switch_::Switch, public Parented<SharpAc>
      {

      protected:
         void write_state(bool state) override
         {
            parent_->setIon(state);
         };
      };

   }
}