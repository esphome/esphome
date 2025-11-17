import esphome.codegen as cg
from esphome.components import spi

ns = cg.esphome_ns.namespace("cc1101")
CC1101Component = ns.class_("CC1101Component", cg.Component, spi.SPIDevice)

CODEOWNERS = ["@lygris @gabest11"]
DEPENDENCIES = ["spi", "button", "remote_receiver"]
AUTO_LOAD = ["remote_base", "remote_transmitter", "binary_sensor"]
MULTI_CONF = True

CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO0_ADC_ID = "gdo0_adc_id"
CONF_OUTPUT_POWER = "output_power"
CONF_RX_ATTENUATION = "rx_attenuation"
CONF_DC_BLOCKING_FILTER = "dc_blocking_filter"

# tuner keys
CONF_IF_FREQUENCY = "if_frequency"
CONF_FILTER_BANDWIDTH = "filter_bandwidth"
CONF_CHANNEL_SPACING = "channel_spacing"
CONF_FSK_DEVIATION = "fsk_deviation"
CONF_MSK_DEVIATION = "msk_deviation"
CONF_SYMBOL_RATE = "symbol_rate"
CONF_SYNC_MODE = "sync_mode"
CONF_CARRIER_SENSE_ABOVE_THRESHOLD = "carrier_sense_above_threshold"
CONF_MODULATION_TYPE = "modulation_type"
CONF_MANCHESTER = "manchester"
CONF_NUM_PREAMBLE = "num_preamble"
CONF_SYNC1 = "sync1"
CONF_SYNC0 = "sync0"
CONF_PKTLEN = "pktlen"

# agc keys
CONF_MAGN_TARGET = "magn_target"
CONF_MAX_LNA_GAIN = "max_lna_gain"
CONF_MAX_DVGA_GAIN = "max_dvga_gain"
CONF_CARRIER_SENSE_ABS_THR = "carrier_sense_abs_thr"
CONF_CARRIER_SENSE_REL_THR = "carrier_sense_rel_thr"
CONF_LNA_PRIORITY = "lna_priority"
CONF_FILTER_LENGTH_FSK_MSK = "filter_length_fsk_msk"
CONF_FILTER_LENGTH_ASK_OOK = "filter_length_ask_ook"
CONF_FREEZE = "freeze"
CONF_HYST_LEVEL = "hyst_level"

# Enums
SyncMode = ns.enum("SyncMode", True)
SYNC_MODE = {
    "None": SyncMode.SYNC_MODE_NONE,
    "15/16": SyncMode.SYNC_MODE_15_16,
    "16/16": SyncMode.SYNC_MODE_16_16,
    "30/32": SyncMode.SYNC_MODE_30_32,
}

Modulation = ns.enum("Modulation", True)
MODULATION = {
    "2-FSK": Modulation.MODULATION_2_FSK,
    "GFSK": Modulation.MODULATION_GFSK,
    "UNUSED 2": Modulation.MODULATION_UNUSED_2,
    "ASK/OOK": Modulation.MODULATION_ASK_OOK,
    "4-FSK": Modulation.MODULATION_4_FSK,
    "UNUSED 5": Modulation.MODULATION_UNUSED_5,
    "UNUSED 6": Modulation.MODULATION_UNUSED_6,
    "MSK": Modulation.MODULATION_MSK,
}

RxAttenuation = ns.enum("RxAttenuation", True)
RX_ATTENUATION = {
    "0dB": RxAttenuation.RX_ATTENUATION_0DB,
    "6dB": RxAttenuation.RX_ATTENUATION_6DB,
    "12dB": RxAttenuation.RX_ATTENUATION_12DB,
    "18dB": RxAttenuation.RX_ATTENUATION_18DB,
}

MagnTarget = ns.enum("MagnTarget", True)
MAGN_TARGET = {
    "24dB": MagnTarget.MAGN_TARGET_24DB,
    "27dB": MagnTarget.MAGN_TARGET_27DB,
    "30dB": MagnTarget.MAGN_TARGET_30DB,
    "33dB": MagnTarget.MAGN_TARGET_33DB,
    "36dB": MagnTarget.MAGN_TARGET_36DB,
    "38dB": MagnTarget.MAGN_TARGET_38DB,
    "40dB": MagnTarget.MAGN_TARGET_40DB,
    "42dB": MagnTarget.MAGN_TARGET_42DB,
}

MaxLnaGain = ns.enum("MaxLnaGain", True)
MAX_LNA_GAIN = {
    "Default": MaxLnaGain.MAX_LNA_GAIN_DEFAULT,
    "2.6dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_2P6DB,
    "6.1dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_6P1DB,
    "7.4dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_7P4DB,
    "9.2dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_9P2DB,
    "11.5dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_11P5DB,
    "14.6dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_14P6DB,
    "17.1dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_17P1DB,
}

MaxDvgaGain = ns.enum("MaxDvgaGain", True)
MAX_DVGA_GAIN = {
    "Default": MaxDvgaGain.MAX_DVGA_GAIN_DEFAULT,
    "-1": MaxDvgaGain.MAX_DVGA_GAIN_MINUS_1,
    "-2": MaxDvgaGain.MAX_DVGA_GAIN_MINUS_2,
    "-3": MaxDvgaGain.MAX_DVGA_GAIN_MINUS_3,
}

CarrierSenseRelThr = ns.enum("CarrierSenseRelThr", True)
CARRIER_SENSE_REL_THR = {
    "Default": CarrierSenseRelThr.CARRIER_SENSE_REL_THR_DEFAULT,
    "+6dB": CarrierSenseRelThr.CARRIER_SENSE_REL_THR_PLUS_6DB,
    "+10dB": CarrierSenseRelThr.CARRIER_SENSE_REL_THR_PLUS_10DB,
    "+14dB": CarrierSenseRelThr.CARRIER_SENSE_REL_THR_PLUS_14DB,
}

FilterLengthFskMsk = ns.enum("FilterLengthFskMsk", True)
FILTER_LENGTH_FSK_MSK = {
    "8": FilterLengthFskMsk.FILTER_LENGTH_8DB,
    "16": FilterLengthFskMsk.FILTER_LENGTH_16DB,
    "32": FilterLengthFskMsk.FILTER_LENGTH_32DB,
    "64": FilterLengthFskMsk.FILTER_LENGTH_64DB,
}

FilterLengthAskOok = ns.enum("FilterLengthAskOok", True)
FILTER_LENGTH_ASK_OOK = {
    "4dB": FilterLengthAskOok.FILTER_LENGTH_4DB,
    "8dB": FilterLengthAskOok.FILTER_LENGTH_8DB,
    "12dB": FilterLengthAskOok.FILTER_LENGTH_12DB,
    "16dB": FilterLengthAskOok.FILTER_LENGTH_16DB,
}

Freeze = ns.enum("Freeze", True)
FREEZE = {
    "Default": Freeze.FREEZE_DEFAULT,
    "On Sync": Freeze.FREEZE_ON_SYNC,
    "Analog Only": Freeze.FREEZE_ANALOG_ONLY,
    "Analog And Digital": Freeze.FREEZE_ANALOG_AND_DIGITAL,
}

WaitTime = ns.enum("WaitTime", True)
WAIT_TIME = {
    "8": WaitTime.WAIT_TIME_8_SAMPLES,
    "16": WaitTime.WAIT_TIME_16_SAMPLES,
    "24": WaitTime.WAIT_TIME_24_SAMPLES,
    "32": WaitTime.WAIT_TIME_32_SAMPLES,
}

HystLevel = ns.enum("HystLevel", True)
HYST_LEVEL = {
    "None": HystLevel.HYST_LEVEL_NONE,
    "Low": HystLevel.HYST_LEVEL_LOW,
    "Medium": HystLevel.HYST_LEVEL_MEDIUM,
    "High": HystLevel.HYST_LEVEL_HIGH,
}
