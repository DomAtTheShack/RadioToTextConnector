#include "connector_config.h"

#include <cctype>

#include "packet.h"

namespace connector {

namespace {

constexpr int rigctldModel = 2;   // Hamlib's NET rigctl: port is host:port

Json fx25ToJson(int fx25) {
    if (fx25 <= 0) return "off";
    if (fx25 == 1) return "auto";
    return fx25;
}

Json pttToJson(const Settings& s) {
    if (s.catPort.empty()) return {{"type", "vox"}};
    if (s.rigModel == rigctldModel) return {{"type", "rigctld"}, {"port", s.catPort}};
    return {{"type", "cat"}, {"port", s.catPort}, {"baud", s.catBaud}, {"rigModel", s.rigModel}};
}

bool validIpv4(const std::string& host) {
    int parts = 0;
    size_t i = 0;
    while (i <= host.size()) {
        size_t dot = host.find('.', i);
        std::string part = host.substr(i, dot == std::string::npos ? std::string::npos : dot - i);
        if (part.empty() || part.size() > 3) return false;
        for (char c : part)
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        if (std::stoi(part) > 255) return false;
        parts++;
        if (dot == std::string::npos) break;
        i = dot + 1;
    }
    return parts == 4;
}

// An integer in [low, high], or nullopt
std::optional<int> intIn(const Json& value, int low, int high) {
    if (!value.is_number_integer()) return std::nullopt;
    long long n = value.get<long long>();
    if (n < low || n > high) return std::nullopt;
    return static_cast<int>(n);
}

} // namespace

Json configToJson(const Settings& s) {
    return {
        {"callsign", s.callsign},
        {"packetBaud", s.packetBaud},
        {"fx25", fx25ToJson(s.fx25)},
        {"path", s.path},
        {"rxAudio", s.rxAudio},
        {"txAudio", s.txAudio},
        {"ptt", pttToJson(s)},
        {"retry", {{"tries", s.retryTries}, {"extraWaitSeconds", s.retryExtraWait}}},
        {"holdForMainSeconds", s.holdForMain},
        {"main", s.mainAddress},
    };
}

ConfigChange applyConfig(Settings& settings, const Json& patch) {
    ConfigChange change;
    auto fail = [&](const std::string& field, const std::string& message) {
        change.ok = false;
        change.field = field;
        change.message = message;
        return change;
    };
    if (!patch.is_object()) return fail("config", "must be an object");

    Settings s = settings;
    for (const auto& [key, value] : patch.items()) {
        if (key == "callsign") {
            if (!value.is_string()) return fail(key, "must be a callsign, e.g. N0CALL-7");
            std::string call = packet::normalizeCallsign(value.get<std::string>());
            if (call.empty()) return fail(key, "must be a callsign, e.g. N0CALL-7");
            s.callsign = call;
        } else if (key == "packetBaud") {
            auto baud = intIn(value, 300, 9600);
            if (!baud || (*baud != 300 && *baud != 1200 && *baud != 9600))
                return fail(key, "must be 300, 1200 or 9600");
            s.packetBaud = *baud;
        } else if (key == "fx25") {
            if (value == "off") s.fx25 = 0;
            else if (value == "auto") s.fx25 = 1;
            else if (value == 16 || value == 32 || value == 64 || value == "16" || value == "32" || value == "64")
                s.fx25 = value.is_string() ? std::stoi(value.get<std::string>()) : value.get<int>();
            else return fail(key, "must be off, auto, 16, 32 or 64");
        } else if (key == "path") {
            if (!value.is_array() || value.size() > 8) return fail(key, "must be a list of up to 8 digipeaters");
            std::vector<std::string> path;
            for (const Json& hop : value) {
                std::string call = hop.is_string() ? packet::normalizeCallsign(hop.get<std::string>()) : "";
                if (call.empty()) return fail(key, "must be a list of callsigns, e.g. [\"WIDE1-1\"]");
                path.push_back(call);
            }
            s.path = path;
        } else if (key == "rxAudio" || key == "txAudio") {
            if (!value.is_string()) return fail(key, "must be a sound card name");
            std::string& name = key == "rxAudio" ? s.rxAudio : s.txAudio;
            std::string& desc = key == "rxAudio" ? s.rxAudioDesc : s.txAudioDesc;
            if (name != value.get<std::string>()) desc.clear();   // The old description no longer fits
            name = value.get<std::string>();
        } else if (key == "ptt") {
            if (!value.is_object()) return fail(key, "must be an object");
            std::string type = s.catPort.empty() ? "vox" : s.rigModel == rigctldModel ? "rigctld" : "cat";
            if (value.contains("type")) {
                if (!value["type"].is_string()) return fail("ptt.type", "must be vox, cat or rigctld");
                type = value["type"].get<std::string>();
            }
            for (const auto& [field, fieldValue] : value.items())
                if (field != "type" && field != "port" && field != "baud" && field != "rigModel")
                    return fail("ptt." + field, "unknown setting");
            if (type == "vox") {
                s.catPort.clear();
            } else if (type == "cat" || type == "rigctld") {
                bool wasType = (type == "rigctld") == (s.rigModel == rigctldModel) && !s.catPort.empty();
                if (value.contains("port")) {
                    if (!value["port"].is_string() || value["port"].get<std::string>().empty())
                        return fail("ptt.port", type == "cat" ? "must be a serial port, e.g. COM5"
                                                              : "must be host:port, e.g. 127.0.0.1:4532");
                    s.catPort = value["port"].get<std::string>();
                } else if (!wasType) {
                    return fail("ptt.port", "is needed for " + type);
                }
                if (type == "rigctld") {
                    std::string host;
                    int port = 0;
                    if (!parseHostPort(s.catPort, host, port))
                        return fail("ptt.port", "must be host:port, e.g. 127.0.0.1:4532");
                    if (value.contains("baud") || value.contains("rigModel"))
                        return fail(value.contains("baud") ? "ptt.baud" : "ptt.rigModel", "isn't used with rigctld");
                    s.rigModel = rigctldModel;
                } else {
                    if (value.contains("baud")) {
                        auto baud = intIn(value["baud"], 300, 1000000);
                        if (!baud) return fail("ptt.baud", "must be a baud rate, e.g. 19200");
                        s.catBaud = *baud;
                    }
                    if (value.contains("rigModel")) {
                        auto model = intIn(value["rigModel"], 1, 99999);
                        if (!model || *model == rigctldModel)
                            return fail("ptt.rigModel", "must be a Hamlib model number (use type rigctld for 2)");
                        s.rigModel = *model;
                    } else if (s.rigModel == rigctldModel) {
                        s.rigModel = defaultRigModel;
                    }
                }
            } else {
                return fail("ptt.type", "must be vox, cat or rigctld");
            }
        } else if (key == "retry") {
            if (!value.is_object()) return fail(key, "must be an object");
            for (const auto& [field, fieldValue] : value.items()) {
                if (field == "tries") {
                    auto tries = intIn(fieldValue, 1, 20);
                    if (!tries) return fail("retry.tries", "must be 1 to 20");
                    s.retryTries = *tries;
                } else if (field == "extraWaitSeconds") {
                    auto wait = intIn(fieldValue, 0, 600);
                    if (!wait) return fail("retry.extraWaitSeconds", "must be 0 to 600");
                    s.retryExtraWait = *wait;
                } else {
                    return fail("retry." + field, "unknown setting");
                }
            }
        } else if (key == "holdForMainSeconds") {
            auto hold = intIn(value, 0, 86400);
            if (!hold) return fail(key, "must be 0 to 86400");
            s.holdForMain = *hold;
        } else if (key == "main") {
            std::string host;
            int port = 0;
            if (!value.is_string() || !parseHostPort(value.get<std::string>(), host, port))
                return fail(key, "must be host:port, e.g. 127.0.0.1:9000");
            s.mainAddress = value.get<std::string>();
        } else {
            return fail(key, "unknown setting");
        }
    }

    change.restartRequired = s.callsign != settings.callsign || s.packetBaud != settings.packetBaud ||
                             s.fx25 != settings.fx25 || s.rxAudio != settings.rxAudio ||
                             s.txAudio != settings.txAudio || s.catPort != settings.catPort ||
                             s.catBaud != settings.catBaud || s.rigModel != settings.rigModel;
    change.mainChanged = s.mainAddress != settings.mainAddress;
    settings = s;
    return change;
}

std::string missingConfig(const Settings& s) {
    if (packet::normalizeCallsign(s.callsign).empty()) return "callsign";
    if (s.rxAudio.empty()) return "rxAudio";
    if (s.txAudio.empty()) return "txAudio";
    return "";
}

bool parseHostPort(const std::string& text, std::string& host, int& port) {
    size_t colon = text.rfind(':');
    if (colon == std::string::npos || colon == 0) return false;
    host = text.substr(0, colon);
    std::string portText = text.substr(colon + 1);
    if (portText.empty() || portText.size() > 5) return false;
    for (char c : portText)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    port = std::stoi(portText);
    if (port < 1 || port > 65535) return false;
    if (host == "localhost") host = "127.0.0.1";
    return validIpv4(host);   // TcpConnection takes IP addresses only
}

Options engineOptions(const Settings& s) {
    Options o;
    o.callsign = packet::normalizeCallsign(s.callsign);
    o.path = s.path;
    o.fx25 = s.fx25;
    o.baud = s.packetBaud;
    o.tries = s.retryTries;
    o.extraWaitSeconds = s.retryExtraWait;
    o.holdForMainSeconds = s.holdForMain;
    return o;
}

} // namespace connector
