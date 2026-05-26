#pragma once

#include "esphome/components/button/button.h"
#include "comp_climate.h"

namespace esphome
{
  namespace sharp_ac
  {
    class ReconnectButton : public button::Button, public Parented<SharpAc>
    {
    protected:
      void press_action() override;
    };

  } 
}
