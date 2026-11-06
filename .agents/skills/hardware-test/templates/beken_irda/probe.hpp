#pragma once
namespace probe {
inline uint32_t rd(uint32_t a) { return *reinterpret_cast<volatile uint32_t *>(a); }
inline void wr(uint32_t a, uint32_t v) { *reinterpret_cast<volatile uint32_t *>(a) = v; }
inline void dump(const char *tag) {
  ESP_LOGI("probe",
           "%s IRDA ctrl=%08X mask=%08X int=%08X fifo=%08X | ICU mux=%08X pwmmux=%08X pwd=%08X gate=%08X | "
           "G26=%08X func=%08X func2=%08X func3=%08X",
           tag, rd(0x802400), rd(0x802404), rd(0x802408), rd(0x80240C), rd(0x802000), rd(0x802004), rd(0x802008),
           rd(0x80200C), rd(0x802800 + 26 * 4), rd(0x802800 + 32 * 4), rd(0x802800 + 46 * 4), rd(0x802800 + 47 * 4));
}
inline void tryit() {
  dump("after-setup");
  wr(0x802400, 0x39210001);
  dump("ctrl-write");
  wr(0x802008, rd(0x802008) & ~(1u << 3));
  dump("pwd-bit3-clear");
  wr(0x80200C, rd(0x80200C) | (1u << 4));
  dump("gate-bit4-set");
  wr(0x802400, 0x39210001);
  dump("ctrl-write2");
}
}  // namespace probe
