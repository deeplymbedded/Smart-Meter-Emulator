#pragma once

#include <Arduino.h>

namespace Gateway
{

#if __has_include(<BuildInfoGenerated.h>)
#include <BuildInfoGenerated.h>
#else
#define GATEWAY_BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

constexpr char SOFTWARE_BUILD_TIMESTAMP[] PROGMEM = GATEWAY_BUILD_TIMESTAMP;

} // namespace Gateway
