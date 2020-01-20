#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
//#include "light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome {
namespace gradient {
using esphome::light::ESPColor;
using esphome::light::ESPHSVColor;

/* The GradientPoint as used in the gimp gradient format. See esphome/ggr.py */

struct GradientPoint {
  public:
    float l, m, r, rl, gl, bl, rr, gr, br;
    // uint8_t causes crash on access, alignment did not work
    uint32_t fn, space;
};

class Gradient {

public: 
  void set_name(std::string name) { this->name_ = name;}

  void set_gradient(const GradientPoint* start, uint32_t length) {
    //   int i;
    //   // the last entry 
    //   for(i = 0, start[i].l != nullptr || start[i].r != nullptr, i++ ) {

    //   }
      ESP_LOGD("custom", "set gp: %p len:%d ", start, length);
      this->length_ = length;
      this->start_ = start;
      ESP_LOGD("custom", "set gp: %p len:%d ", this->start_, length);
  }
  const GradientPoint* get_gradient() const { return (const GradientPoint*)this->length_; }
  size_t get_length() const { return this->length_; }

  /* returns the color at point x of the gradient. 0 <= x <= 1.0 */
  ESPColor color(float x) const {
        /* for seg in self.segs:
            if seg.l <= x <= seg.r:
                break
        else:
            # No segment applies! Return black I guess.
            return (0,0,0)

        # Normalize the segment geometry.
        mid = (seg.m - seg.l)/(seg.r - seg.l)
        pos = (x - seg.l)/(seg.r - seg.l)
        
        # Assume linear (most common, and needed by most others).
        if pos <= mid:
            f = pos/mid/2
        else:
            f = (pos - mid)/(1 - mid)/2 + 0.5

        # Find the correct interpolation factor.
        if seg.fn == 1:   # Curved
            f = math.pow(pos, math.log(0.5) / math.log(mid));
        elif seg.fn == 2:   # Sinusoidal
            f = (math.sin((-math.pi/2) + math.pi*f) + 1)/2
        elif seg.fn == 3:   # Spherical increasing
            f -= 1
            f = math.sqrt(1 - f*f)
        elif seg.fn == 4:   # Spherical decreasing
            f = 1 - math.sqrt(1 - f*f);

        # Interpolate the colors
        if seg.space == 0:
            c = (
                seg.rl + (seg.rr-seg.rl) * f,
                seg.gl + (seg.gr-seg.gl) * f,
                seg.bl + (seg.br-seg.bl) * f
                )
        elif seg.space in (1,2):
            hl, sl, vl = colorsys.rgb_to_hsv(seg.rl, seg.gl, seg.bl)
            hr, sr, vr = colorsys.rgb_to_hsv(seg.rr, seg.gr, seg.br)

            if seg.space == 1 and hr < hl:
                hr += 1
            elif seg.space == 2 and hr > hl:
                hr -= 1

            c = colorsys.hsv_to_rgb(
                (hl + (hr-hl) * f) % 1.0,
                sl + (sr-sl) * f,
                vl + (vr-vl) * f
                )
        return c
         */
    //ESP_LOGF("x point:", x);
    // ESP_LOGD("custom", "Pos x:%f len:%d gp:%p", x, this->length_, this->start_);
    const GradientPoint* seg;
    ESPColor c = ESPColor(0,0,0);
    bool ok = false;
    int i=0;
    for (i=0; i < this->length_; i++) {
      //(*friends_ptrs)
      seg = &(this->start_)[i];
      //seg = this->start_ + (sizeof(GradientPoint) * i);
      //break;
      //ESP_LOGD("custom", "segpos %p", this->start_[i]);
      //ESP_LOGD("custom", "Test i: %d (%p) l: %f r: %f", i, seg, seg->l, seg->r);
      if(seg->l <= x && x <= seg->r) {
        ok = true;
        break;
      }
    }
    // ESP_LOGD("custom", "i:%d ",i);

    // for(auto seg: this->gradient_) {
    //   if(seg.l <= x && x <= seg.r) {
    //     ok = true;
    //     break;
    //   }
    // }
    if(!ok){
      ESP_LOGD("custom", "No color point found in gradient");
      return ESPColor::BLACK;
    }
    // ESP_LOGD("custom", "seg.rl: %f seg.gl: %f seg.bl: %f seg.rr: %f seg.gr: %f seg.br: %f",
    //         seg->rl, seg->gl, seg->bl, seg->rr, seg->gr, seg->br);
    // ESP_LOGD("custom", "seg.l: %f seg.m: %f seg.r: %f ",
    //         seg->l, seg->m, seg->r);
    // ESP_LOGD("custom", "seg.fn: %u",
    //         seg->space);
    // ESP_LOGD("custom", "x");
    float mid = (seg->m - seg->l)/(seg->r - seg->l);
    // ESP_LOGD("custom", "x");
    float pos = (x - seg->l)/(seg->r - seg->l);
    // ESP_LOGD("custom", "x");
    float f;

    // Assume linear (most common, and needed by most others).
    if (pos <= mid) {
      f = (pos/mid/2.0);
    } else {
      f = (pos - mid)/(1.0 - mid)/2.0 + 0.5;
    }
    // ESP_LOGD("custom", "x");

    if(seg->fn == 1) {   // Curved
      f = pow(pos,(log(0.5) / log(mid)));
    } else if (seg->fn == 2) {  // Sinusoidal
      f = (sin((-M_PI/2.0) + M_PI*f) + 1.0)/2.0;
    } else if (seg->fn == 3) {   // Spherical increasing
      f -= 1.0;
      f = sqrt(1.0 - f*f);
    } else if (seg->fn == 4) {   // Spherical decreasing
      f = 1.0 - sqrt(1.0 - f*f);
    }

    // ESP_LOGD("custom", "i:%d x: %f mid:%f pos:%f f:%f",i, x, mid, pos, f);
    // ESP_LOGD("custom", "seg.rl: %f seg.gl: %f seg.bl: %f seg.rr: %f seg.gr: %f seg.br: %f",
            // seg->rl, seg->gl, seg->bl, seg->rr, seg->gr, seg->br);

    
    // ESP_LOGD("custom", "finish r:%d g:%d b:%d",
    //     (uint8_t)((seg->rl + (seg->rr-seg->rl) * f)*255),
    //     (uint8_t)((seg->gl + (seg->gr-seg->gl) * f)*255),
    //     (uint8_t)((seg->bl + (seg->br-seg->bl) * f)*255));

    if (seg->space == 0) {
      c = ESPColor(
        (uint8_t)((seg->rl + (seg->rr-seg->rl) * f)*255),
        (uint8_t)((seg->gl + (seg->gr-seg->gl) * f)*255),
        (uint8_t)((seg->bl + (seg->br-seg->bl) * f)*255)
      );
      
    } else if (seg->space == 1 || seg->space == 2) {
      float hl, sl, vl, hr, sr, vr;
      ESPHSVColor::rgb2hsv(seg->rl, seg->gl, seg->bl, hl, sl, vl);
      ESPHSVColor::rgb2hsv(seg->rr, seg->gr, seg->br, hr, sr, vr);

      if (seg->space == 1 && hr < hl) {
        hr += 1;
      } else if ( seg->space == 2 and hr > hl) {
        hr -= 1;
      }
      //float h = l.h + (r.h-l.h) * f;
      // ESP_LOGD("custom", "xx %f %f %d", (hl + (hr-hl) * f), fmod(hl + (hr-hl) * f, 1.0), (uint8_t)(fmod(hl + (hr-hl) * f, 360)*(255.0/360.0)));
      c = ESPHSVColor(
            (uint8_t)(fmod(hl + (hr-hl) * f, 360.0)*(255.0/360.0)),
            (uint8_t)((sl + (sr-sl) * f)*255),
            (uint8_t)((vl + (vr-vl) * f)*255)
            ).to_rgb();
      
    }
    return c;
  }
 protected:
  std::string name_;
  // gets updated when GradienPoint is set
  size_t length_{0};
  // pointer to the start element of gradient
  //const struct GradientPoint* start_;
  const GradientPoint* start_;
};

} // namespace light
} // namespace esphome