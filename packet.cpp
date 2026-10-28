#include "packet.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace packet {

namespace {

constexpr uint8_t FEND = 0xC0, FESC = 0xDB, TFEND = 0xDC, TFESC = 0xDD;
constexpr uint8_t UI = 0x03, NO_LAYER3 = 0xF0;
constexpr size_t addressBytes = 7;
constexpr size_t maxAddresses = 10;        // Destination, source and up to 8 digipeaters
constexpr size_t maxMonitorLine = 390;     // gen_packets reads lines into a 400-byte buffer

// 6 shifted, space-padded characters, then the SSID byte
void appendAddress(Bytes& out, const std::string& call, bool command, bool last) {
    size_t dash = call.find('-');
    std::string base = call.substr(0, dash);
    int ssid = dash == std::string::npos ? 0 : std::atoi(call.c_str() + dash + 1);
    for (size_t i = 0; i < 6; i++)
        out.push_back(static_cast<uint8_t>((i < base.size() ? base[i] : ' ') << 1));
    // Bits: C (command/response), two reserved bits set, SSID, last-address flag
    out.push_back(static_cast<uint8_t>((command ? 0x80 : 0) | 0x60 | (ssid << 1) | (last ? 1 : 0)));
}

std::string readAddress(const uint8_t* p) {
    std::string call;
    for (int i = 0; i < 6; i++) {
        char c = static_cast<char>(p[i] >> 1);
        if (c != ' ') call += c;
    }
    int ssid = (p[6] >> 1) & 0x0F;
    if (ssid) call += "-" + std::to_string(ssid);
    return call;
}

bool startsUtf8Char(uint8_t byte) { return (byte & 0xC0) != 0x80; }

std::string hexEscape(uint8_t byte) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "<0x%02x>", byte);
    return buf;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string stripAnsi(std::string_view text) {
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') {
            i += 2;
            while (i < text.size() && !(text[i] >= 0x40 && text[i] <= 0x7e)) i++;
            continue;
        }
        out += text[i];
    }
    return out;
}

} // namespace

std::string normalizeCallsign(const std::string& text) {
    std::string call;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        call += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    size_t dash = call.find('-');
    std::string base = call.substr(0, dash);
    if (base.empty() || base.size() > 6) return "";
    for (char c : base)
        if (!std::isalnum(static_cast<unsigned char>(c))) return "";
    if (dash == std::string::npos) return base;
    std::string ssid = call.substr(dash + 1);
    if (ssid.empty() || ssid.size() > 2) return "";
    for (char c : ssid)
        if (!std::isdigit(static_cast<unsigned char>(c))) return "";
    int n = std::atoi(ssid.c_str());
    if (n > 15) return "";
    return n == 0 ? base : base + "-" + std::to_string(n);
}

Bytes encodeUi(const std::string& source, const std::string& destination, std::string_view info) {
    Bytes frame;
    frame.reserve(2 * addressBytes + 2 + info.size());
    // AX.25 v2 command frame: C bit set on the destination, clear on the source
    appendAddress(frame, destination, true, false);
    appendAddress(frame, source, false, true);
    frame.push_back(UI);
    frame.push_back(NO_LAYER3);
    frame.insert(frame.end(), info.begin(), info.end());
    return frame;
}

std::optional<Frame> decode(const Bytes& frame) {
    // The last address has its lowest bit set
    size_t addresses = 0;
    while (true) {
        size_t at = addresses * addressBytes;
        if (at + addressBytes > frame.size() || addresses >= maxAddresses) return std::nullopt;
        addresses++;
        if (frame[at + addressBytes - 1] & 1) break;
    }
    if (addresses < 2) return std::nullopt;
    size_t control = addresses * addressBytes;
    if (control + 2 > frame.size()) return std::nullopt;   // Needs control and PID
    uint8_t c = frame[control];
    bool iFrame = (c & 0x01) == 0;
    bool uiFrame = (c & ~0x10) == UI;                       // Either poll/final bit
    if (!iFrame && !uiFrame) return std::nullopt;

    Frame out;
    out.destination = readAddress(&frame[0]);
    out.source = readAddress(&frame[addressBytes]);
    for (size_t i = 2; i < addresses; i++) out.path.push_back(readAddress(&frame[i * addressBytes]));
    out.info.assign(frame.begin() + static_cast<long>(control + 2), frame.end());
    return out;
}

std::vector<std::string> splitInfo(std::string_view text, size_t maxBytes) {
    std::vector<std::string> pieces;
    while (!text.empty()) {
        size_t take = text.size();
        if (take > maxBytes) {
            take = maxBytes;
            while (take > 0 && !startsUtf8Char(static_cast<uint8_t>(text[take]))) take--;
            if (take == 0) take = maxBytes;
        }
        pieces.emplace_back(text.substr(0, take));
        text.remove_prefix(take);
    }
    return pieces;
}

std::string printable(std::string_view info) {
    if (!info.empty() && info.back() == '\n') info.remove_suffix(1);
    if (!info.empty() && info.back() == '\r') info.remove_suffix(1);
    std::string out;
    for (char ch : info) {
        auto byte = static_cast<uint8_t>(ch);
        if (byte < 0x20 || byte == 0x7f) out += hexEscape(byte);
        else out += ch;
    }
    return out;
}

std::string kissEncode(const Bytes& frame) {
    std::string out;
    out.reserve(frame.size() + 4);
    out += static_cast<char>(FEND);
    out += '\0';                                            // Port 0, data frame
    for (uint8_t byte : frame) {
        if (byte == FEND) {
            out += static_cast<char>(FESC);
            out += static_cast<char>(TFEND);
        } else if (byte == FESC) {
            out += static_cast<char>(FESC);
            out += static_cast<char>(TFESC);
        } else {
            out += static_cast<char>(byte);
        }
    }
    out += static_cast<char>(FEND);
    return out;
}

std::vector<Bytes> KissDecoder::feed(std::string_view bytes) {
    std::vector<Bytes> frames;
    for (char ch : bytes) {
        auto byte = static_cast<uint8_t>(ch);
        if (byte == FEND) {
            // Command byte low nibble 0 = data frame
            if (inFrame && current.size() > 1 && (current[0] & 0x0F) == 0)
                frames.emplace_back(current.begin() + 1, current.end());
            current.clear();
            escaped = false;
            inFrame = true;
            continue;
        }
        if (!inFrame) continue;
        if (escaped) {
            if (byte == TFEND) current.push_back(FEND);
            else if (byte == TFESC) current.push_back(FESC);
            escaped = false;
        } else if (byte == FESC) {
            escaped = true;
        } else {
            current.push_back(byte);
        }
    }
    return frames;
}

std::vector<Bytes> parseAtestDump(std::string_view output) {
    // Each frame:
    //   U frame UI: p/f=0, No layer 3 protocol implemented., length = 32
    //     000:  86 a2 40 40 40 40 e0 9c 60 86 82 98 98 e1 03 f0  ..@@@@..`.......
    // "length" says how many bytes to read
    std::string text = stripAnsi(output);
    std::vector<Bytes> frames;
    Bytes current;
    size_t wanted = 0;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string_view line(text.data() + start, end - start);
        start = end + 1;

        if (size_t at = line.find(", length = "); at != std::string_view::npos) {
            wanted = static_cast<size_t>(std::atoi(std::string(line.substr(at + 11)).c_str()));
            current.clear();
            continue;
        }
        if (wanted == 0) continue;
        size_t i = line.find_first_not_of(' ');
        if (i == std::string_view::npos || i + 4 > line.size() || line[i + 3] != ':' ||
            hexValue(line[i]) < 0 || hexValue(line[i + 1]) < 0 || hexValue(line[i + 2]) < 0) continue;
        i += 4;
        for (int n = 0; n < 16 && current.size() < wanted; n++) {
            while (i < line.size() && line[i] == ' ') i++;
            if (i + 2 > line.size()) break;
            int high = hexValue(line[i]), low = hexValue(line[i + 1]);
            if (high < 0 || low < 0) break;
            current.push_back(static_cast<uint8_t>(high * 16 + low));
            i += 2;
        }
        if (current.size() == wanted) {
            frames.push_back(current);
            current.clear();
            wanted = 0;
        }
    }
    return frames;
}

std::vector<std::string> monitorLines(const std::string& source, const std::string& destination,
                                      std::string_view text) {
    std::vector<std::string> lines;
    std::string header = source + ">" + destination + ":";
    for (const std::string& piece : splitInfo(text)) {
        std::string line = header;
        for (char ch : piece) {
            auto byte = static_cast<uint8_t>(ch);
            std::string part = (byte < 0x20 || byte >= 0x7f || ch == '<') ? hexEscape(byte)
                                                                         : std::string(1, ch);
            if (line.size() + part.size() > maxMonitorLine) {
                lines.push_back(line);
                line = header;
            }
            line += part;
        }
        lines.push_back(line);
    }
    return lines;
}

double airtime(size_t frameBytes, int baud) {
    // Flags, FCS and ~5% bit stuffing
    double bits = static_cast<double>(frameBytes + 4) * 8 * 1.05;
    return bits / baud;
}

} // namespace packet
