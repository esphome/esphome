import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import (
    CONF_ID,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_PROTOCOL,
    CONF_VISUAL,
)

CODEOWNERS = ["@rob-deutsch"]

AUTO_LOAD = ["climate_ir"]

heatpumpir_ns = cg.esphome_ns.namespace("heatpumpir")
HeatpumpIRClimate = heatpumpir_ns.class_("HeatpumpIRClimate", climate_ir.ClimateIR)

Protocol = heatpumpir_ns.enum("Protocol")
PROTOCOLS = {
    "aux": Protocol.PROTOCOL_AUX,
    "ballu": Protocol.PROTOCOL_BALLU,
    "carrier_mca": Protocol.PROTOCOL_CARRIER_MCA,
    "carrier_nqv": Protocol.PROTOCOL_CARRIER_NQV,
    "daikin_arc417": Protocol.PROTOCOL_DAIKIN_ARC417,
    "daikin_arc480": Protocol.PROTOCOL_DAIKIN_ARC480,
    "daikin": Protocol.PROTOCOL_DAIKIN,
    "electroluxyal": Protocol.PROTOCOL_ELECTROLUXYAL,
    "fuego": Protocol.PROTOCOL_FUEGO,
    "fujitsu_awyz": Protocol.PROTOCOL_FUJITSU_AWYZ,
    "gree": Protocol.PROTOCOL_GREE,
    "greeya": Protocol.PROTOCOL_GREEYAA,
    "greeyan": Protocol.PROTOCOL_GREEYAN,
    "greeyac": Protocol.PROTOCOL_GREEYAC,
    "greeyt": Protocol.PROTOCOL_GREEYT,
    "greeyap": Protocol.PROTOCOL_GREEYAP,
    "hisense_aud": Protocol.PROTOCOL_HISENSE_AUD,
    "hitachi": Protocol.PROTOCOL_HITACHI,
    "hyundai": Protocol.PROTOCOL_HYUNDAI,
    "ivt": Protocol.PROTOCOL_IVT,
    "midea": Protocol.PROTOCOL_MIDEA,
    "mitsubishi_fa": Protocol.PROTOCOL_MITSUBISHI_FA,
    "mitsubishi_fd": Protocol.PROTOCOL_MITSUBISHI_FD,
    "mitsubishi_fe": Protocol.PROTOCOL_MITSUBISHI_FE,
    "mitsubishi_heavy_fdtc": Protocol.PROTOCOL_MITSUBISHI_HEAVY_FDTC,
    "mitsubishi_heavy_zj": Protocol.PROTOCOL_MITSUBISHI_HEAVY_ZJ,
    "mitsubishi_heavy_zm": Protocol.PROTOCOL_MITSUBISHI_HEAVY_ZM,
    "mitsubishi_heavy_zmp": Protocol.PROTOCOL_MITSUBISHI_HEAVY_ZMP,
    "mitsubishi_heavy_kj": Protocol.PROTOCOL_MITSUBISHI_KJ,
    "mitsubishi_msc": Protocol.PROTOCOL_MITSUBISHI_MSC,
    "mitsubishi_msy": Protocol.PROTOCOL_MITSUBISHI_MSY,
    "mitsubishi_sez": Protocol.PROTOCOL_MITSUBISHI_SEZ,
    "panasonic_ckp": Protocol.PROTOCOL_PANASONIC_CKP,
    "panasonic_dke": Protocol.PROTOCOL_PANASONIC_DKE,
    "panasonic_jke": Protocol.PROTOCOL_PANASONIC_JKE,
    "panasonic_lke": Protocol.PROTOCOL_PANASONIC_LKE,
    "panasonic_nke": Protocol.PROTOCOL_PANASONIC_NKE,
    "samsung_aqv": Protocol.PROTOCOL_SAMSUNG_AQV,
    "samsung_fjm": Protocol.PROTOCOL_SAMSUNG_FJM,
    "sharp": Protocol.PROTOCOL_SHARP,
    "toshiba_daiseikai": Protocol.PROTOCOL_TOSHIBA_DAISEIKAI,
    "toshiba": Protocol.PROTOCOL_TOSHIBA,
    "zhlt01": Protocol.PROTOCOL_ZHLT01,
    "nibe": Protocol.PROTOCOL_NIBE,
    "carrier_qlima_1": Protocol.PROTOCOL_QLIMA_1,
    "carrier_qlima_2": Protocol.PROTOCOL_QLIMA_2,
    "samsung_aqv12msan": Protocol.PROTOCOL_SAMSUNG_AQV12MSAN,
    "zhjg01": Protocol.PROTOCOL_ZHJG01,
    "airway": Protocol.PROTOCOL_AIRWAY,
    "bgh_aud": Protocol.PROTOCOL_BGH_AUD,
    "panasonic_altdke": Protocol.PROTOCOL_PANASONIC_ALTDKE,
    "vaillantvai8": Protocol.PROTOCOL_VAILLANTVAI8,
    "r51m": Protocol.PROTOCOL_R51M,
}

CONF_HORIZONTAL_DEFAULT = "horizontal_default"
HorizontalDirections = heatpumpir_ns.enum("HorizontalDirections")
HORIZONTAL_DIRECTIONS = {
    "auto": HorizontalDirections.HORIZONTAL_DIRECTION_AUTO,
    "middle": HorizontalDirections.HORIZONTAL_DIRECTION_MIDDLE,
    "left": HorizontalDirections.HORIZONTAL_DIRECTION_LEFT,
    "mleft": HorizontalDirections.HORIZONTAL_DIRECTION_MLEFT,
    "mright": HorizontalDirections.HORIZONTAL_DIRECTION_MRIGHT,
    "right": HorizontalDirections.HORIZONTAL_DIRECTION_RIGHT,
}

CONF_VERTICAL_DEFAULT = "vertical_default"
VerticalDirections = heatpumpir_ns.enum("VerticalDirections")
VERTICAL_DIRECTIONS = {
    "auto": VerticalDirections.VERTICAL_DIRECTION_AUTO,
    "up": VerticalDirections.VERTICAL_DIRECTION_UP,
    "mup": VerticalDirections.VERTICAL_DIRECTION_MUP,
    "middle": VerticalDirections.VERTICAL_DIRECTION_MIDDLE,
    "mdown": VerticalDirections.VERTICAL_DIRECTION_MDOWN,
    "down": VerticalDirections.VERTICAL_DIRECTION_DOWN,
}

CONFIG_SCHEMA = cv.All(
    climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HeatpumpIRClimate),
            cv.Required(CONF_PROTOCOL): cv.enum(PROTOCOLS),
            cv.Required(CONF_HORIZONTAL_DEFAULT): cv.enum(HORIZONTAL_DIRECTIONS),
            cv.Required(CONF_VERTICAL_DEFAULT): cv.enum(VERTICAL_DIRECTIONS),
            cv.Required(CONF_MIN_TEMPERATURE): cv.temperature,
            cv.Required(CONF_MAX_TEMPERATURE): cv.temperature,
        }
    ),
    cv.only_with_arduino,
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_VISUAL not in config:
        config[CONF_VISUAL] = {}
    visual = config[CONF_VISUAL]
    if CONF_MAX_TEMPERATURE not in visual:
        visual[CONF_MAX_TEMPERATURE] = config[CONF_MAX_TEMPERATURE]
    if CONF_MIN_TEMPERATURE not in visual:
        visual[CONF_MIN_TEMPERATURE] = config[CONF_MIN_TEMPERATURE]
    yield climate_ir.register_climate_ir(var, config)
    cg.add(var.set_protocol(config[CONF_PROTOCOL]))
    cg.add(var.set_horizontal_default(config[CONF_HORIZONTAL_DEFAULT]))
    cg.add(var.set_vertical_default(config[CONF_VERTICAL_DEFAULT]))
    cg.add(var.set_max_temperature(config[CONF_MAX_TEMPERATURE]))
    cg.add(var.set_min_temperature(config[CONF_MIN_TEMPERATURE]))

    cg.add_library("tonia/HeatpumpIR", "1.0.27")
