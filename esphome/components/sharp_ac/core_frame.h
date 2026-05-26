#pragma once

#include <cstdint>
#include <cstddef>
#include "core_types.h"

class SharpState;

class SharpFrame
{
protected:
    uint8_t *data;
    size_t size;

public:
    SharpFrame();
    SharpFrame(char c);
    SharpFrame(const uint8_t *arr, size_t sz);
    ~SharpFrame();
    SharpFrame(const SharpFrame &other);
    uint8_t *getData() const;
    size_t getSize() const;
    int setSize(size_t sz);
    void print();
    virtual void setChecksum();
    bool validateChecksum();
    uint8_t calcChecksum();
};

class SharpStatusFrame : public SharpFrame
{
public:
    SharpStatusFrame(const uint8_t *arr);
    int getTemperature();
};

class SharpModeFrame : public SharpFrame
{
public:
    SharpModeFrame(const uint8_t *arr);
    int getTemperature();
    bool getState();
    FanMode getFanMode();
    PowerMode getPowerMode();
    SwingVertical getSwingVertical();
    SwingHorizontal getSwingHorizontal();
    Preset getPreset();
    bool getIon();
};

class SharpCommandFrame : public SharpFrame
{
public:
    SharpCommandFrame();
    void setData(SharpState *state);
    void setChecksum() override;

private:
    void commandChecksum();
};

class SharpACKFrame : public SharpFrame
{
public:
    SharpACKFrame();
    void setChecksum() override;
};