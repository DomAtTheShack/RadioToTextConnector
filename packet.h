// packet.h - AX.25 UI frames, KISS framing, and atest/gen_packets formats.
#ifndef HAMPROOF_PACKET_H
#define HAMPROOF_PACKET_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace packet {

using Bytes = std::vector<uint8_t>;

inline const std::string groupAddress = "CQ";
constexpr size_t maxInfo = 256;   // AX.25 default; longer text is split

// "n0call-7" -> "N0CALL-7", "N0CALL-0" -> "N0CALL". Empty if invalid
std::string normalizeCallsign(const std::string& text);

struct Frame {
    std::string source;
    std::string destination;
    std::vector<std::string> path;   // Digipeaters
    std::string info;
};

// Without the FCS. Callsigns must be normalized
Bytes encodeUi(const std::string& source, const std::string& destination, std::string_view info);
// UI and I frames only
std::optional<Frame> decode(const Bytes& frame);

// Never splits a UTF-8 character
std::vector<std::string> splitInfo(std::string_view text, size_t maxBytes = maxInfo);

// One line; control bytes shown as <0x0d>
std::string printable(std::string_view info);

// ---- KISS --------------------------------------------------------------------

std::string kissEncode(const Bytes& frame);   // Port 0 data frame

class KissDecoder {
public:
    std::vector<Bytes> feed(std::string_view bytes);

private:
    Bytes current;
    bool escaped = false;
    bool inFrame = false;
};

// ---- direwolf's file tools ---------------------------------------------------

// Frames from "atest -h" hex dumps
std::vector<Bytes> parseAtestDump(std::string_view output);

// gen_packets input lines: "N0CALL>CQ:text"
std::vector<std::string> monitorLines(const std::string& source, const std::string& destination,
                                      std::string_view text);

// Seconds on air, not counting key-up
double airtime(size_t frameBytes, int baud);

} // namespace packet

#endif //HAMPROOF_PACKET_H
