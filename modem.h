// modem.h - minimodem and direwolf command lines and config.
#ifndef HAMPROOF_MODEM_H
#define HAMPROOF_MODEM_H

#include <string>
#include <vector>

#include "settings.h"

// Shift should be at least the baud rate; twice that is comfortable
enum class ShiftFit { TooNarrow, Marginal, Comfortable };
ShiftFit shiftFit(float shift, float baud);

std::string formatNumber(float value);   // %g

// audioArgs pick the sound card or file
std::vector<std::string> minimodemArgs(const Settings& settings, bool transmit,
                                       const std::vector<std::string>& audioArgs);

// ---- direwolf ------------------------------------------------------------------

const char* packetModemName(int baud);

// Sound card name for ADEVICE. Always "default" on macOS
std::string direwolfDevice(const std::string& name);

// AGW and the default KISS port are off so another direwolf doesn't clash
std::string direwolfConfig(const Settings& settings, const std::string& rxDevice,
                           const std::string& txDevice, int kissPort);
std::vector<std::string> direwolfArgs(const std::string& configPath);
std::vector<std::string> atestArgs(const Settings& settings, const std::string& wavPath);
std::vector<std::string> genPacketsArgs(const Settings& settings, const std::string& wavPath);

// One-line summary, e.g. "Receive | RTTY 45.45 baud | mark 2125 / space 2295 Hz | squelch 3.5"
std::string describe(const Settings& settings);

#endif //HAMPROOF_MODEM_H
