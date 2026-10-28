// settings.h - Settings and their file.
#ifndef HAMPROOF_SETTINGS_H
#define HAMPROOF_SETTINGS_H

#include <string>

enum class Operation { Receive, Transmit, Duplex };
enum class Engine { Minimodem, Direwolf };
enum class Protocol { Rtty, Bell202 };

// RIG_MODEL_X6100, without needing Hamlib's headers here
constexpr int defaultRigModel = 3087;

struct Settings {
    Operation operation = Operation::Receive;
    Engine    engine    = Engine::Minimodem;
    Protocol  protocol  = Protocol::Rtty;      // minimodem only
    float baud    = 45.45f;    // RTTY only; Bell202 is always 1200
    float centre  = 2210.0f;   // Hz, RTTY only
    float shift   = 170.0f;    // Hz, RTTY only
    float squelch = 3.5f;      // minimodem -c, receiving only
    int packetBaud = 1200;     // direwolf: 300, 1200 or 9600
    std::string callsign;      // direwolf
    std::string rxAudio;       // AudioDevice::name; empty = ask
    std::string txAudio;
    std::string rxAudioDesc;
    std::string txAudioDesc;
    std::string catPort;       // Empty = VOX
    int catBaud = 19200;
    int rigModel = defaultRigModel;   // Hamlib model number
    std::string audioFile;     // Empty = sound card
    bool quiet = false;        // minimodem -q

    // Mark is the lower tone
    float markHz()  const { return centre - shift / 2.0f; }
    float spaceHz() const { return centre + shift / 2.0f; }
    bool rtty()      const { return engine == Engine::Minimodem && protocol == Protocol::Rtty; }
    bool packet()    const { return engine == Engine::Direwolf; }
    bool transmits() const { return operation != Operation::Receive; }
    bool receives()  const { return operation != Operation::Transmit; }
};

// key=value lines. Unknown keys and bad values keep the default
std::string formatSettings(const Settings& settings);
Settings parseSettings(const std::string& text);

//   Windows  %APPDATA%\RTTY-Connector\settings.ini
//   macOS    ~/Library/Application Support/RTTY-Connector/settings.ini
//   Linux    $XDG_CONFIG_HOME/rtty-connector/settings.ini (~/.config by default)
std::string settingsPath();
Settings loadSettings();
bool saveSettings(const Settings& settings);

const char* operationName(Operation operation);
const char* engineName(Engine engine);
const char* protocolName(Protocol protocol);

#endif //HAMPROOF_SETTINGS_H