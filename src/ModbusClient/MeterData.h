#pragma once

#include <stdint.h>

namespace Gateway
{

struct MeterData
{
    uint32_t ua = 0;
    uint32_t ub = 0;
    uint32_t uc = 0;

    uint32_t ia = 0;
    uint32_t ib = 0;
    uint32_t ic = 0;

    int32_t pa = 0;
    int32_t pb = 0;
    int32_t pc = 0;

    bool valid = false;
    uint32_t timestamp = 0;
};

} // namespace Gateway
