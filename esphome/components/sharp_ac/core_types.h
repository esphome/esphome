#pragma once

const int IonMode = 0x80;

enum class PowerMode
{
    heat = 0x1,
    cool = 0x2,
    dry = 0x3,
    fan = 0x4
};

enum class Preset
{
    NONE = 0x0,
    ECO = 0x1,
    FULLPOWER = 0x2
};

enum class FanMode
{
    low = 0x4,
    mid = 0x3,
    high = 0x5,
    highest = 0x7,
    auto_fan = 0x2
};

enum class SwingVertical
{
    swing = 0xF,
    auto_position = 0x8,
    highest = 0x9,
    high = 0xA,
    mid = 0xB,
    low = 0xC,
    lowest = 0xD,
};

enum class SwingHorizontal
{
    swing = 0xF,
    middle = 0x1,
    right = 0x2,
    left = 0x3,
};