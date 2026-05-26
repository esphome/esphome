#pragma once

#include "esphome/components/select/select.h"
#include "comp_climate.h"

namespace esphome
{
    namespace sharp_ac
    {

        class VaneSelectHorizontal : public select::Select, public Parented<SharpAc>
        {
        public:
            VaneSelectHorizontal() = default;

            void setVal(SwingHorizontal val)
            {
                switch (val)
                {

                case SwingHorizontal::swing:
                    this->publish_state("swing");
                    break;
                case SwingHorizontal::left:
                    this->publish_state("left");
                    break;
                case SwingHorizontal::middle:
                    this->publish_state("center");
                    break;
                case SwingHorizontal::right:
                    this->publish_state("right");
                    break;
                }
            }
            void control(const std::string &value) override
            {
                SwingHorizontal pos = SwingHorizontal::middle; 
                if (value == "swing")
                {
                    pos = SwingHorizontal::swing;
                }
                else if (value == "left")
                {
                    pos = SwingHorizontal::left;
                }
                else if (value == "center")
                {
                    pos = SwingHorizontal::middle;
                }
                else if (value == "right")
                {
                    pos = SwingHorizontal::right;
                }
                this->parent_->setVaneHorizontal(pos);
            };

        protected:
        private:
        };

    }
}