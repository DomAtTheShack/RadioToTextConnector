// session.h - Receive, transmit and duplex sessions. Each returns on Esc or when done.
#ifndef HAMPROOF_SESSION_H
#define HAMPROOF_SESSION_H

#include <functional>
#include <string>
#include <vector>

class Cat;

// false if minimodem failed
bool runReceive(const std::vector<std::string>& rxArgs);

// For feeding a program other than minimodem (gen_packets)
struct TransmitFormat {
    const char* name = "minimodem";
    std::function<std::string(const std::string&)> format;   // Typed text -> stdin. Empty = as-is
    bool hideOutput = false;   // Show output only on failure
};

// Enter sends a line, Tab finishes, Esc stops. text is sent first; with finishAfterText
// the session ends once it's sent. radio keys PTT over CAT; null = VOX
bool runTransmit(const std::vector<std::string>& txArgs, Cat* radio,
                 const std::string& text = "", bool finishAfterText = false,
                 const TransmitFormat& format = {});

// Alternates Tx and Rx, starting with Tx. Only Tab switches: a dropped carrier is too
// common on the air to mean the other station has finished
bool runDuplex(const std::vector<std::string>& txArgs, const std::vector<std::string>& rxArgs,
               Cat* radio);

// ---- Packets (direwolf) ----------------------------------------------------------

struct PacketSession {
    std::vector<std::string> direwolfArgs;
    std::string workingDir;                  // Where direwolf and its config are
    int kissPort = 0;
    std::string callsign;
    int baud = 1200;                         // For estimating airtime
    bool showReceived = true;                // Receive and Duplex
    bool canType = false;                    // Transmit and Duplex
    std::string text;                        // Sent first
    bool finishAfterText = false;
};

// One packet per line typed. Esc waits for sent packets to go out; Esc again stops now.
// transmitted is set if anything was sent. false if direwolf failed
bool runPacketSession(const PacketSession& session, bool& transmitted);

#endif //HAMPROOF_SESSION_H
