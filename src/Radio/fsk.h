// fsk.h - Test audio (RTTY, Bell202, AX.25, tones, noise), made without minimodem or
// direwolf so decoding it checks we match them.
#ifndef RTC_FSK_H
#define RTC_FSK_H

#include <cstdint>
#include <string>
#include <vector>

namespace fsk {

// Mono audio, samples between -1 and 1
struct Audio {
    int rate = 11025;
    std::vector<float> samples;
};

// ITA2 with U.S. figures, unshift on space (like minimodem). '\n' becomes CR LF
std::vector<uint8_t> baudotEncode(const std::string& text);

Audio rtty(const std::string& text, float baud, float markHz, float spaceHz, int rate = 11025);
Audio bell202(const std::string& text, int rate = 11025);

// Frames without FCS. baud: 1200 (AFSK VHF), 300 (AFSK HF) or 9600 (G3RUH)
Audio ax25(const std::vector<std::vector<uint8_t>>& frames, int baud, int rate = 48000);
uint16_t ax25Fcs(const std::vector<uint8_t>& frame);

Audio tone(float hz, float seconds, int rate = 11025);
Audio noise(float seconds, int rate = 11025, uint32_t seed = 1);
void addNoise(Audio& audio, float snrDb, uint32_t seed = 1);

// 16-bit PCM
bool writeWav(const std::string& path, const Audio& audio);

} // namespace fsk

#endif //RTC_FSK_H
