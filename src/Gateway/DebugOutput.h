#pragma once

#include <Arduino.h>

namespace GatewayDebug
{

void println(const char *message);
void printf(const char *format, ...);
bool enabled();
void setEnabled(bool enabled);
String logJson();

} // namespace GatewayDebug
