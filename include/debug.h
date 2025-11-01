#pragma once

#ifdef DEBUG
#define DebugPrint(x) Serial.print(x)
#define DebugPrintln(x) Serial.println(x)
#define DebugPrintf(...) Serial.printf(__VA_ARGS__)
#define DebugBegin(baud) Serial.begin(baud)

#else
#define DebugPrint(x)
#define DebugPrintln(x)
#define DebugPrintf(...)
#define DebugBegin(baux)
#endif