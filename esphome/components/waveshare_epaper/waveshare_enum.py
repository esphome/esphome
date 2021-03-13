import esphome.codegen as cg

waveshare_epaper_ns = cg.esphome_ns.namespace("waveshare_epaper")

WAVESHARE_COLORS_ENUM = {
    "Waveshare_Black": waveshare_epaper_ns.Waveshare_Black,
    "Waveshare_White": waveshare_epaper_ns.Waveshare_White,
    "Waveshare_Green": waveshare_epaper_ns.Waveshare_Green,
    "Waveshare_Blue": waveshare_epaper_ns.Waveshare_Blue,
    "Waveshare_Red": waveshare_epaper_ns.Waveshare_Red,
    "Waveshare_Yellow": waveshare_epaper_ns.Waveshare_Yellow,
    "Waveshare_Orange": waveshare_epaper_ns.Waveshare_Orange,
    "Waveshare_Blank": waveshare_epaper_ns.Waveshare_Blank,
}
