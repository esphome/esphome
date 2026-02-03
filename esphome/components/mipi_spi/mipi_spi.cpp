#include "mipi_spi.h"
#include "esphome/core/log.h"

namespace esphome::mipi_spi {

void internal_dump_config(const char *model, int width, int height, int offset_width, int offset_height, uint8_t madctl,
                          bool invert_colors, int display_bits, bool is_big_endian, const optional<uint8_t> &brightness,
                          GPIOPin *cs, GPIOPin *reset, GPIOPin *dc, int spi_mode, uint32_t data_rate, int bus_width) {
  ESP_LOGCONFIG(TAG,
                "MIPI_SPI Display\n"
                "  Model: %s\n"
                "  Width: %d\n"
                "  Height: %d\n"
                "  Swap X/Y: %s\n"
                "  Mirror X: %s\n"
                "  Mirror Y: %s\n"
                "  Invert colors: %s\n"
                "  Color order: %s\n"
                "  Display pixels: %d bits\n"
                "  Endianness: %s\n"
                "  SPI Mode: %d\n"
                "  SPI Data rate: %uMHz\n"
                "  SPI Bus width: %d",
                model, width, height, YESNO(madctl & MADCTL_MV), YESNO(madctl & (MADCTL_MX | MADCTL_XFLIP)),
                YESNO(madctl & (MADCTL_MY | MADCTL_YFLIP)), YESNO(invert_colors), (madctl & MADCTL_BGR) ? "BGR" : "RGB",
                display_bits, is_big_endian ? "Big" : "Little", spi_mode, static_cast<unsigned>(data_rate / 1000000),
                bus_width);
  LOG_PIN("  CS Pin: ", cs);
  LOG_PIN("  Reset Pin: ", reset);
  LOG_PIN("  DC Pin: ", dc);
  if (offset_width != 0)
    ESP_LOGCONFIG(TAG, "  Offset width: %d", offset_width);
  if (offset_height != 0)
    ESP_LOGCONFIG(TAG, "  Offset height: %d", offset_height);
  if (brightness.has_value())
    ESP_LOGCONFIG(TAG, "  Brightness: %u", brightness.value());
}

}  // namespace esphome::mipi_spi
