#pragma once

#include "esphome/components/select/select.h"
#include "comp_climate.h"

namespace esphome
{
    namespace sharp_ac
    {

        class VaneSelectVertical : public select::Select, public Parented<SharpAc>
        {
        public:
            VaneSelectVertical() = default;
            void control(const std::string &value) override
            {
                SwingVertical pos = SwingVertical::auto_position;
                if (value == "auto")
                {
                    pos = SwingVertical::auto_position;
                }
                else if (value == "swing")
                {
                    pos = SwingVertical::swing;
                }
                else if (value == "up")
                {
                    pos = SwingVertical::highest;
                }
                else if (value == "up_center")
                {
                    pos = SwingVertical::high;
                }
                else if (value == "center")
                {
                    pos = SwingVertical::mid;
                }
                else if (value == "down_center")
                {
                    pos = SwingVertical::low;
                }
                else if (value == "down")
                {
                    pos = SwingVertical::lowest;
                }
                this->parent_->setVaneVertical(pos);
            };
            void setVal(SwingVertical val)
            {
                switch (val)
                {
                case SwingVertical::auto_position:
                    this->publish_state("auto");
                    break;
                case SwingVertical::swing:
                    this->publish_state("swing");
                    break;
                case SwingVertical::highest:
                    this->publish_state("up");
                    break;
                case SwingVertical::high:
                    this->publish_state("up_center");
                    break;
                case SwingVertical::mid:
                    this->publish_state("center");
                    break;
                case SwingVertical::low:
                    this->publish_state("down_center");
                    break;
                case SwingVertical::lowest:
                    this->publish_state("down");
                    break;
                }
            }

        protected:
        private:
        };

    }
}