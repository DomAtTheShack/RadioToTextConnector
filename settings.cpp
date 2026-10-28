#include "settings.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool toFloat(const std::string& s, float& out) {
    char* end = nullptr;
    float value = std::strtof(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !(value > 0)) return false;
    out = value;
    return true;
}

std::string number(float value) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", value);
    return buf;
}

fs::path configDir() {
#ifdef _WIN32
    if (const wchar_t* appData = _wgetenv(L"APPDATA")) return fs::path(appData) / "RTTY-Connector";
    return fs::path("RTTY-Connector");
#else
    const char* home = std::getenv("HOME");
    fs::path base = home ? fs::path(home) : fs::path(".");
  #ifdef __APPLE__
    return base / "Library" / "Application Support" / "RTTY-Connector";
  #else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        return fs::path(xdg) / "rtty-connector";
    return base / ".config" / "rtty-connector";
  #endif
#endif
}

} // namespace

const char* operationName(Operation operation) {
    switch (operation) {
        case Operation::Receive:  return "Receive";
        case Operation::Transmit: return "Transmit";
        case Operation::Duplex:   return "Duplex";
    }
    return "?";
}

const char* engineName(Engine engine) {
    return engine == Engine::Minimodem ? "minimodem" : "direwolf";
}

const char* protocolName(Protocol protocol) {
    return protocol == Protocol::Rtty ? "RTTY" : "Bell202 1200 bps";
}

std::string formatSettings(const Settings& s) {
    std::ostringstream out;
    out << "# Radio to Text Connector settings\n";
    out << "operation=" << (s.operation == Operation::Receive  ? "receive"
                          : s.operation == Operation::Transmit ? "transmit" : "duplex") << "\n";
    out << "modem=" << engineName(s.engine) << "\n";
    out << "protocol=" << (s.protocol == Protocol::Rtty ? "rtty" : "bell202") << "\n";
    out << "baud=" << number(s.baud) << "\n";
    out << "centre=" << number(s.centre) << "\n";
    out << "shift=" << number(s.shift) << "\n";
    out << "squelch=" << number(s.squelch) << "\n";
    out << "packet_baud=" << s.packetBaud << "\n";
    out << "callsign=" << s.callsign << "\n";
    out << "rx_audio=" << s.rxAudio << "\n";
    out << "tx_audio=" << s.txAudio << "\n";
    out << "rx_audio_desc=" << s.rxAudioDesc << "\n";
    out << "tx_audio_desc=" << s.txAudioDesc << "\n";
    out << "cat_port=" << s.catPort << "\n";
    out << "cat_baud=" << s.catBaud << "\n";
    out << "rig_model=" << s.rigModel << "\n";
    out << "audio_file=" << s.audioFile << "\n";
    out << "quiet=" << (s.quiet ? "1" : "0") << "\n";
    return out.str();
}

Settings parseSettings(const std::string& text) {
    Settings s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (key == "operation") {
            if (value == "receive")  s.operation = Operation::Receive;
            if (value == "transmit") s.operation = Operation::Transmit;
            if (value == "duplex")   s.operation = Operation::Duplex;
        } else if (key == "modem") {
            if (value == "minimodem") s.engine = Engine::Minimodem;
            if (value == "direwolf")  s.engine = Engine::Direwolf;
        } else if (key == "protocol") {
            if (value == "rtty")    s.protocol = Protocol::Rtty;
            if (value == "bell202") s.protocol = Protocol::Bell202;
        } else if (key == "baud")       toFloat(value, s.baud);
        else if (key == "centre")       toFloat(value, s.centre);
        else if (key == "shift")        toFloat(value, s.shift);
        else if (key == "squelch")      toFloat(value, s.squelch);
        else if (key == "packet_baud") {
            float baud;
            if (toFloat(value, baud) && (baud == 300 || baud == 1200 || baud == 9600))
                s.packetBaud = static_cast<int>(baud);
        }
        else if (key == "callsign")     s.callsign = value;
        else if (key == "rig_model") {
            float model;
            if (toFloat(value, model) && model >= 1 && model < 100000) s.rigModel = static_cast<int>(model);
        }
        else if (key == "rx_audio")     s.rxAudio = value;
        else if (key == "tx_audio")     s.txAudio = value;
        else if (key == "rx_audio_desc") s.rxAudioDesc = value;
        else if (key == "tx_audio_desc") s.txAudioDesc = value;
        else if (key == "cat_port")     s.catPort = value;
        else if (key == "cat_baud") {
            float baud;
            if (toFloat(value, baud)) s.catBaud = static_cast<int>(baud);
        }
        else if (key == "audio_file")   s.audioFile = value;
        else if (key == "quiet")        s.quiet = value == "1" || value == "true";
    }
    return s;
}

std::string settingsPath() {
    return (configDir() / "settings.ini").string();
}

Settings loadSettings() {
    std::ifstream in(configDir() / "settings.ini");
    if (!in) return Settings{};
    std::stringstream text;
    text << in.rdbuf();
    return parseSettings(text.str());
}

bool saveSettings(const Settings& settings) {
    std::error_code ec;
    fs::create_directories(configDir(), ec);
    // Write to a temp file and rename, so a crash can't leave a half-written file
    fs::path path = configDir() / "settings.ini";
    fs::path temp = configDir() / "settings.ini.tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) return false;
        out << formatSettings(settings);
        if (!out) return false;
    }
    fs::rename(temp, path, ec);
    if (ec) {
        // Some Windows runtimes won't rename over an existing file
        fs::remove(path, ec);
        fs::rename(temp, path, ec);
    }
    return !ec;
}
