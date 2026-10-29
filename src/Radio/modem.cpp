#include "modem.h"

#include <cstdio>

#include "../backend.h"

ShiftFit shiftFit(float shift, float baud) {
    float ratio = shift / baud;
    if (ratio < 1.0f) return ShiftFit::TooNarrow;
    if (ratio < 2.0f) return ShiftFit::Marginal;
    return ShiftFit::Comfortable;
}

std::string formatNumber(float value) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", value);
    return buf;
}

std::vector<std::string> minimodemArgs(const Settings& s, bool transmit,
                                       const std::vector<std::string>& audioArgs) {
    std::vector<std::string> args = {os::minimodemPath()};
    if (s.rtty()) {
        // Baudot, 1.5 stop bits
        args.insert(args.end(), {
            "-5", "--stopbits", "1.5",
            "-M", formatNumber(s.markHz()), "-S", formatNumber(s.spaceHz()),
            // Tone grid. minimodem's default of 50 Hz from 100 baud up moves 2125 to 2150
            "-b", "10",
        });
    }
    args.insert(args.end(), audioArgs.begin(), audioArgs.end());
    if (transmit) {
        // Idle on mark between lines so VOX stays keyed and the receiver stays locked
        args.emplace_back("--tx-carrier");
    } else {
        args.insert(args.end(), {"-c", formatNumber(s.squelch)});
    }
    args.emplace_back(transmit ? "--tx" : "--rx");
    if (s.quiet) args.emplace_back("-q");
    args.emplace_back(s.rtty() ? formatNumber(s.baud) : "1200");
    return args;
}

const char* packetModemName(int baud) {
    switch (baud) {
        case 300:  return "AFSK, HF";
        case 9600: return "G3RUH";
        default:   return "AFSK, VHF/FM";
    }
}

std::string direwolfDevice(const std::string& name) {
#ifdef __APPLE__
    (void)name;
    return "default";
#else
    // Windows: WinMM index. Linux: ALSA name
    return name.empty() ? "default" : name;
#endif
}

std::string direwolfConfig(const Settings& s, const std::string& rxDevice,
                           const std::string& txDevice, int kissPort) {
    std::string call = s.callsign.empty() ? "N0CALL" : s.callsign;
    std::string config =
        "# Temporary config written by Radio to Text Connector\n"
        "ADEVICE " + rxDevice + " " + txDevice + "\n"
        "ARATE 48000\n"
        "ACHANNELS 1\n"
        "CHANNEL 0\n"
        "MYCALL " + call + "\n"
        "MODEM " + std::to_string(s.packetBaud) + "\n";
    // Receiving decodes FX.25 either way
    if (s.fx25 > 0) config += "FX25TX " + std::to_string(s.fx25) + "\n";
    switch (s.ptt) {
        case PttType::Vox:
            break;
        case PttType::Cat:
            // Model 2 (rigctld) takes host:port and no baud rate
            config += "PTT RIG " + std::to_string(s.rigModel) + " " + s.catPort;
            if (s.rigModel != ownRigctldModel) config += " " + std::to_string(s.catBaud);
            config += "\n";
            break;
        case PttType::Rts:
        case PttType::Dtr:
            // The CAT port's own line, or a second port (e.g. the FTDX3000's Standard port)
            config += "PTT " + s.keyingPort() + (s.ptt == PttType::Rts ? " RTS\n" : " DTR\n");
            break;
    }
    config +=
        "AGWPORT 0\n"
        "KISSPORT 0\n"
        "KISSPORT " + std::to_string(kissPort) + "\n";
    return config;
}

std::vector<std::string> direwolfArgs(const std::string& configPath) {
    // -t 0: no colours. -q hd: quiet; packets are shown from the KISS port
    return {os::direwolfPath(), "-c", configPath, "-t", "0", "-q", "hd"};
}

std::vector<std::string> atestArgs(const Settings& s, const std::string& wavPath) {
    return {os::atestPath(), "-B", std::to_string(s.packetBaud), "-h", wavPath};
}

std::vector<std::string> genPacketsArgs(const Settings& s, const std::string& wavPath) {
    std::vector<std::string> args = {os::genPacketsPath(), "-B", std::to_string(s.packetBaud),
                                     "-r", "48000"};
    if (s.fx25 > 0) {
        args.push_back("-X");
        args.push_back(std::to_string(s.fx25));
    }
    args.insert(args.end(), {"-o", wavPath, "-"});
    return args;
}

std::string describe(const Settings& s) {
    std::string text = operationName(s.operation);
    if (s.packet()) {
        text += " | direwolf " + std::to_string(s.packetBaud) + " baud AX.25 (" +
                packetModemName(s.packetBaud) + ")" +
                (s.transmits() && s.fx25 > 0 ? " FX.25" : "") + " | " +
                (s.callsign.empty() ? std::string("no callsign") : s.callsign);
        if (!s.audioFile.empty()) text += " | file " + s.audioFile;
        return text;
    }
    if (s.rtty()) {
        text += " | RTTY " + formatNumber(s.baud) + " baud | mark " + formatNumber(s.markHz()) +
                " / space " + formatNumber(s.spaceHz()) + " Hz";
    } else {
        text += " | Bell202 1200 bps";
    }
    if (s.receives()) text += " | squelch " + formatNumber(s.squelch);
    if (!s.audioFile.empty()) text += " | file " + s.audioFile;
    return text;
}
