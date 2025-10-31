import esphome.codegen as cg

CODEOWNERS = ["@abel-msk"]

storage_ns = cg.esphome_ns.namespace("storage")
RawStorage = storage_ns.class_("RawStorage")
FileProvider = storage_ns.class_("FileProvider")
