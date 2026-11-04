"""GoFree: Grundfos SCALA1 over Bluetooth for ESPHome.

This component only ships the C++ helpers (gofree.h) used by the GoFree package.
Add `gofree:` to your configuration (the package does it for you).
"""
import esphome.config_validation as cv

CODEOWNERS = []
DEPENDENCIES = ["esp32"]
AUTO_LOAD = []

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass
