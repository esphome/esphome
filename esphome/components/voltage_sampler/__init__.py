import esphome.codegen as cg

DOMAIN = "voltage_sampler"

voltage_sampler_ns = cg.esphome_ns.namespace("voltage_sampler")
VoltageSampler = voltage_sampler_ns.class_("VoltageSampler")
