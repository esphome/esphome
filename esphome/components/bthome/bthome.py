import esphome.codegen as cg

bthome_ns = cg.esphome_ns.namespace("bthome")
bthome_object_types = bthome_ns.enum("BTHomeObjectType", True)


class FunctionMap(dict):
    def __init__(self, keys, func):
        # We store the keys in a set for O(1) lookups
        self._keys = set(keys)
        self._func = func

    def __getitem__(self, key):
        if key not in self._keys:
            # Standard dictionary behavior for missing keys
            raise KeyError(key)
        return self._func(key)

    def __iter__(self):
        return iter(self._keys)

    def __len__(self):
        return len(self._keys)

    def __repr__(self):
        return f"{self.__class__.__name__}(keys={list(self._keys)})"


BTHOME_OBJECT_TYPE_MAPPING = FunctionMap(
    [
        "ACCELERATION_MSS_E3",
        "ACCELERATION_MSS_I32_E6",
        "BATTERY_PCT",
        "CHANNEL",
        "CO2_PPM",
        "CONDUCTIVITY_USCM",
        "COUNT_U8",
        "COUNT_U16",
        "COUNT_U32",
        "COUNT_I8",
        "COUNT_I16",
        "COUNT_I32",
        "CURRENT_A_E3",
        "CURRENT_A_I16_E3",
        "DEWPOINT_C_E2",
        "DIRECTION_DEG_E2",
        "DISTANCE_MM",
        "DISTANCE_M_E1",
        "DURATION_S_E3",
        "ENERGY_KWH_E3",
        "ENERGY_KWH_U32_E3",
        "GAS_M3_U24_E3",
        "GAS_M3_U32_E3",
        "GYROSCOPE_DEGS_E3",
        "HUMIDITY_PCT_E2",
        "HUMIDITY_PCT_U8",
        "ILLUMINANCE_LX_E2",
        "MASS_KG_E2",
        "MASS_LB_E2",
        "MOISTURE_PCT_E2",
        "MOISTURE_PCT_U8",
        "PACKET_ID",
        "PM10_UGM3",
        "PM25_UGM3",
        "POWER_W_E2",
        "POWER_W_I32_E2",
        "PRECIPITATION_MM_E1",
        "PRESSURE_HPA_E2",
        "RAW",
        "ROTATION_DEG_E1",
        "ROTATIONAL_SPEED_RPM",
        "SPEED_MS_E2",
        "SPEED_MS_I32_E6",
        "TEMPERATURE_C_E2",
        "TEMPERATURE_C_E1",
        "TEMPERATURE_C_I8",
        "TEMPERATURE_C_I8_0_35",
        "TEXT",
        "TIMESTAMP",
        "TVOC_UGM3",
        "UV_INDEX_E1",
        "VOLTAGE_V_E3",
        "VOLTAGE_V_E1",
        "VOLUME_FLOW_M3HR_E3",
        "VOLUME_L_E1",
        "VOLUME_ML",
        "VOLUME_L_U32_E3",
        "VOLUME_STORAGE_L_E3",
        "WATER_L_E3",
        "BATTERY_CHARGING",
        "BATTERY_LOW",
        "CO_DETECTED",
        "COLD_DETECTED",
        "CONNECTIVITY_CONNECTED",
        "DOOR_OPEN",
        "GARAGE_DOOR_OPEN",
        "GAS_DETECTED",
        "GENERIC_BOOLEAN",
        "HEAT_DETECTED",
        "LIGHT_DETECTED",
        "LOCK_UNLOCKED",
        "MOISTURE_WET",
        "MOTION_DETECTED",
        "MOVING_ACTIVE",
        "OCCUPANCY_DETECTED",
        "OPENING_OPEN",
        "PLUG_PLUGGED_IN",
        "POWER_ON",
        "PRESENCE_HOME",
        "PROBLEM_DETECTED",
        "RUNNING_ACTIVE",
        "SAFETY_SAFE",
        "SMOKE_DETECTED",
        "SOUND_DETECTED",
        "TAMPER_ACTIVE",
        "VIBRATION_DETECTED",
        "WINDOW_OPEN",
    ],
    bthome_object_types.__getattr__,
)
