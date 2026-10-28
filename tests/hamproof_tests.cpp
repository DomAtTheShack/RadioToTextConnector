// hamproof_tests.cpp - Run by ctest:
//   hamproof_tests unit              No minimodem or direwolf needed
//   hamproof_tests audio <dir>       Decodes the test files. Exits 77 (skipped) if no decoder
//   hamproof_tests generate <dir>    Rewrites the test files
// On GitHub Actions, results also go to annotations and the job summary.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "fsk.h"
#include "modem.h"
#include "packet.h"
#include "settings.h"
#include "testcases.h"

using std::string;

namespace {

// ---- GitHub Actions reporting ------------------------------------------------

bool onGitHub() {
    const char* value = std::getenv("GITHUB_ACTIONS");
    return value && std::strcmp(value, "true") == 0;
}

// Annotation messages are one line: GitHub wants %, CR and LF escaped
std::string escapeAnnotation(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '%') out += "%25";
        else if (c == '\r') out += "%0D";
        else if (c == '\n') out += "%0A";
        else out += c;
    }
    return out;
}

// level is "error", "warning" or "notice"; file is relative to the repository
void annotate(const char* level, const std::string& file, int line, const std::string& title,
              const std::string& message) {
    if (!onGitHub()) return;
    std::printf("::%s file=%s", level, file.c_str());
    if (line > 0) std::printf(",line=%d", line);
    std::printf(",title=%s::%s\n", title.c_str(), escapeAnnotation(message).c_str());
}

void appendSummary(const std::string& markdown) {
    const char* path = std::getenv("GITHUB_STEP_SUMMARY");
    if (!path) return;
    if (std::FILE* f = std::fopen(path, "a")) {
        std::fputs(markdown.c_str(), f);
        std::fclose(f);
    }
}

std::string runnerName() {
    const char* os = std::getenv("RUNNER_OS");
    return os ? os : "local";
}

// Keeps a table cell on one line and stops '|' from starting a new column
std::string cell(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '|') out += "\\|";
        else if (c == '\n' || c == '\r') out += ' ';
        else out += c;
    }
    return out;
}

std::string percent(float value) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.0f%%", value * 100);
    return buf;
}

// ---- Unit test checks --------------------------------------------------------

int failures = 0;
int checks = 0;
std::vector<std::string> failureLines;   // For the summary

#define CHECK(condition) check((condition), #condition, __LINE__)

void check(bool ok, const char* what, int line) {
    checks++;
    if (ok) return;
    failures++;
    std::printf("FAIL tests/hamproof_tests.cpp:%d: %s\n", line, what);
    annotate("error", "tests/hamproof_tests.cpp", line, "Unit test failed", what);
    failureLines.push_back("line " + std::to_string(line) + ": `" + cell(what) + "`");
}

bool contains(const std::vector<std::string>& args, const std::vector<std::string>& run) {
    for (size_t i = 0; i + run.size() <= args.size(); i++) {
        bool match = true;
        for (size_t j = 0; j < run.size() && match; j++) match = args[i + j] == run[j];
        if (match) return true;
    }
    return false;
}

void testShiftFit() {
    CHECK(shiftFit(170, 45.45f) == ShiftFit::Comfortable);
    CHECK(shiftFit(85, 45.45f) == ShiftFit::Marginal);
    CHECK(shiftFit(170, 300) == ShiftFit::TooNarrow);
    CHECK(shiftFit(850, 300) == ShiftFit::Comfortable);
}

void testSettingsRoundTrip() {
    Settings s;
    s.operation = Operation::Duplex;
    s.protocol = Protocol::Bell202;
    s.baud = 75;
    s.centre = 1360;
    s.shift = 425;
    s.squelch = 2.5f;
    s.rxAudio = "plughw:CARD=PCH,DEV=0";
    s.rxAudioDesc = "HDA Intel PCH - Front mic";
    s.txAudio = "default";
    s.catPort = "/dev/ttyACM0";
    s.catBaud = 115200;
    s.audioFile = "C:\\Users\\me\\My Recordings\\rtty test.wav";
    s.quiet = true;

    Settings back = parseSettings(formatSettings(s));
    CHECK(back.operation == Operation::Duplex);
    CHECK(back.protocol == Protocol::Bell202);
    CHECK(back.baud == 75);
    CHECK(back.centre == 1360);
    CHECK(back.shift == 425);
    CHECK(back.squelch == 2.5f);
    CHECK(back.rxAudio == s.rxAudio);
    CHECK(back.rxAudioDesc == s.rxAudioDesc);
    CHECK(back.txAudio == "default");
    CHECK(back.catPort == "/dev/ttyACM0");
    CHECK(back.catBaud == 115200);
    CHECK(back.audioFile == s.audioFile);
    CHECK(back.quiet);

    // 45.45 must survive the trip exactly enough to show as 45.45 again
    Settings defaults;
    CHECK(formatNumber(parseSettings(formatSettings(defaults)).baud) == "45.45");
}

void testSettingsGarbage() {
    Settings s = parseSettings("baud=abc\nshift=-5\noperation=sideways\nnonsense\n=\ncentre=1500\n"
                               "# comment=1\r\nsquelch = 5 \r\n");
    Settings defaults;
    CHECK(s.baud == defaults.baud);
    CHECK(s.shift == defaults.shift);
    CHECK(s.operation == defaults.operation);
    CHECK(s.centre == 1500);
    CHECK(s.squelch == 5);   // Spaces and Windows line endings are fine
}

void testTones() {
    Settings s;   // 2210 centre, 170 shift
    CHECK(s.markHz() == 2125);
    CHECK(s.spaceHz() == 2295);
}

void testMinimodemArgs() {
    Settings s;
    s.squelch = 3.5f;
    auto rx = minimodemArgs(s, false, {"-f", "in.wav"});
    CHECK(contains(rx, {"-5", "--stopbits", "1.5"}));
    CHECK(contains(rx, {"-M", "2125", "-S", "2295"}));
    CHECK(contains(rx, {"-f", "in.wav"}));
    CHECK(contains(rx, {"-c", "3.5"}));
    CHECK(contains(rx, {"--rx"}));
    CHECK(!contains(rx, {"--rx-one"}));                  // Duplex switches only on Tab
    CHECK(!contains(rx, {"-q"}));
    CHECK(rx.back() == "45.45");                         // minimodem wants the baud rate last

    s.protocol = Protocol::Bell202;
    s.quiet = true;
    auto tx = minimodemArgs(s, true, {"--alsa=plug:default"});
    CHECK(contains(tx, {"--tx-carrier"}));
    CHECK(contains(tx, {"--tx"}));
    CHECK(contains(tx, {"-q"}));
    CHECK(!contains(tx, {"-5"}));
    CHECK(!contains(tx, {"-c"}));                        // Squelch only matters receiving
    CHECK(tx.back() == "1200");
}

void testBaudot() {
    // LTRS LTRS R Y SPACE FIGS 1 2: a space unshifts at the receiver, so FIGS is sent again
    std::vector<uint8_t> expected = {0x1F, 0x1F, 0x0A, 0x15, 0x04, 0x1B, 0x17, 0x13};
    CHECK(fsk::baudotEncode("ry 12") == expected);
    // Figures then letters without a space needs an explicit LTRS
    std::vector<uint8_t> mixed = {0x1F, 0x1F, 0x1B, 0x17, 0x1F, 0x03};
    CHECK(fsk::baudotEncode("1A") == mixed);
    // Newline is CR LF; characters Baudot can't send are dropped
    std::vector<uint8_t> newline = {0x1F, 0x1F, 0x03, 0x08, 0x02, 0x03};
    CHECK(fsk::baudotEncode("a\n{a") == newline);
}

void testSignalLength() {
    // 1 s leader + 0.5 s trailer + 0.4 s silence, plus 7.5 bits per code at 50 baud
    fsk::Audio audio = fsk::rtty("RY", 50, 1275, 1725, 8000);
    size_t codes = fsk::baudotEncode("RY").size();
    double seconds = 1.9 + codes * 7.5 / 50;
    CHECK(audio.rate == 8000);
    CHECK(std::abs(static_cast<double>(audio.samples.size()) - seconds * 8000) < 2);
}

void testPacketSettings() {
    Settings s;
    s.engine = Engine::Direwolf;
    s.packetBaud = 9600;
    s.callsign = "N0CALL-7";
    s.rigModel = 3073;
    Settings back = parseSettings(formatSettings(s));
    CHECK(back.engine == Engine::Direwolf);
    CHECK(back.packet());
    CHECK(!back.rtty());                                  // RTTY settings don't apply to packets
    CHECK(back.packetBaud == 9600);
    CHECK(back.callsign == "N0CALL-7");
    CHECK(back.rigModel == 3073);
    // Old settings files have no modem line: still minimodem, and the X6100
    Settings old = parseSettings("protocol=rtty\npacket_baud=4800\n");
    CHECK(old.engine == Engine::Minimodem);
    CHECK(old.rigModel == defaultRigModel);
    CHECK(old.packetBaud == 1200);                        // 4800 isn't a direwolf speed we offer
}

void testCallsigns() {
    CHECK(packet::normalizeCallsign("n0call") == "N0CALL");
    CHECK(packet::normalizeCallsign(" w1aw-7 ") == "W1AW-7");
    CHECK(packet::normalizeCallsign("W1AW-0") == "W1AW");
    CHECK(packet::normalizeCallsign("W1AW-15") == "W1AW-15");
    CHECK(packet::normalizeCallsign("W1AW-16").empty());
    CHECK(packet::normalizeCallsign("TOOLONG1").empty());
    CHECK(packet::normalizeCallsign("W1/AW").empty());
    CHECK(packet::normalizeCallsign("W1AW-").empty());
    CHECK(packet::normalizeCallsign("").empty());
}

void testAx25Frames() {
    // Same bytes direwolf's gen_packets makes for N0CALL>CQ, apart from the C bits
    packet::Bytes frame = packet::encodeUi("N0CALL", "CQ", "Hi");
    packet::Bytes expected = {0x86, 0xa2, 0x40, 0x40, 0x40, 0x40, 0xe0,
                              0x9c, 0x60, 0x86, 0x82, 0x98, 0x98, 0x61, 0x03, 0xf0, 'H', 'i'};
    CHECK(frame == expected);

    auto back = packet::decode(packet::encodeUi("W1AW-12", "CQ", "{\"msg\":\"73\"}"));
    CHECK(back.has_value());
    CHECK(back && back->source == "W1AW-12");
    CHECK(back && back->destination == "CQ");
    CHECK(back && back->path.empty());
    CHECK(back && back->info == "{\"msg\":\"73\"}");

    // Non-UI frames without information, and truncated ones, give nothing
    packet::Bytes sabm = frame;
    sabm.resize(15);
    sabm[14] = 0x2F;
    CHECK(!packet::decode(sabm));
    CHECK(!packet::decode(packet::Bytes(frame.begin(), frame.begin() + 10)));
    CHECK(!packet::decode({}));

    CHECK(packet::printable("hello\r\n") == "hello");
    CHECK(packet::printable("a\tb") == "a<0x09>b");
    CHECK(packet::printable("caf\xc3\xa9") == "caf\xc3\xa9");   // UTF-8 left alone
}

void testSplitInfo() {
    std::string longLine(600, 'x');
    auto pieces = packet::splitInfo(longLine);
    CHECK(pieces.size() == 3);
    CHECK(pieces.size() == 3 && pieces[0].size() == 256 && pieces[2].size() == 88);
    // Never inside a UTF-8 character: 255 x's then a 2-byte character
    std::string utf8 = std::string(255, 'x') + "\xc3\xa9";
    auto split = packet::splitInfo(utf8);
    CHECK(split.size() == 2 && split[0].size() == 255 && split[1] == "\xc3\xa9");
    CHECK(packet::splitInfo("").empty());
}

void testKiss() {
    packet::Bytes frame = {0x01, 0xC0, 0x02, 0xDB, 0x03};
    std::string wire = packet::kissEncode(frame);
    std::string expected = {'\xC0', '\x00', '\x01', '\xDB', '\xDC', '\x02', '\xDB', '\xDD', '\x03', '\xC0'};
    CHECK(wire == expected);

    // Split across reads, with a non-data command frame and empty frames around it
    packet::KissDecoder decoder;
    std::string stream = std::string("\xC0\xC0\x06\x10\xC0", 5) + wire;
    auto first = decoder.feed(stream.substr(0, 9));
    auto second = decoder.feed(stream.substr(9));
    CHECK(first.empty());
    CHECK(second.size() == 1 && second[0] == frame);
}

void testAtestDump() {
    // Shortened from real `atest -h` output, colour codes and all
    std::string output =
        "\x1b[38;2;0;192;0m\nDECODED[1] 0:00.486 N0CALL audio level = 50(14/14)\n"
        "\x1b[38;2;0;192;0m[0] N0CALL>CQ:Hi <0x0a>\n------\n"
        "U frame UI: p/f=0, No layer 3 protocol implemented., length = 18\n"
        " dest    CQ      0 c/r=1 res=3 last=0\n"
        " source  N0CALL  0 c/r=1 res=3 last=1\n"
        "  000:  86 a2 40 40 40 40 e0 9c 60 86 82 98 98 e1 03 f0  ..@@@@..`.......\n"
        "  010:  48 69                                            Hi\n"
        "------\n"
        "1 packets decoded in 0.016 seconds.\n";
    auto frames = packet::parseAtestDump(output);
    CHECK(frames.size() == 1);
    auto frame = frames.empty() ? std::nullopt : packet::decode(frames[0]);
    CHECK(frame && frame->source == "N0CALL" && frame->info == "Hi");
}

void testMonitorLines() {
    auto lines = packet::monitorLines("N0CALL-1", "CQ", "a<b\tc");
    CHECK(lines.size() == 1 && lines[0] == "N0CALL-1>CQ:a<0x3c>b<0x09>c");
    // Escapes make this too long for one gen_packets line, so it's split
    auto escaped = packet::monitorLines("N0CALL", "CQ", std::string(200, '<'));
    CHECK(escaped.size() == 4);
    for (const auto& line : escaped) CHECK(line.size() <= 390);
}

void testDirewolfConfig() {
    Settings s;
    s.engine = Engine::Direwolf;
    s.packetBaud = 300;
    s.callsign = "N0CALL-7";
    std::string vox = direwolfConfig(s, "plughw:1,0", "default", 7001);
    CHECK(vox.find("ADEVICE plughw:1,0 default\n") != std::string::npos);
    CHECK(vox.find("MODEM 300\n") != std::string::npos);
    CHECK(vox.find("MYCALL N0CALL-7\n") != std::string::npos);
    CHECK(vox.find("KISSPORT 0\nKISSPORT 7001\n") != std::string::npos);
    CHECK(vox.find("AGWPORT 0\n") != std::string::npos);
    CHECK(vox.find("PTT") == std::string::npos);           // VOX: direwolf mustn't key anything

    s.catPort = "COM5";
    s.catBaud = 38400;
    s.rigModel = 1037;   // FTDX-3000
    CHECK(direwolfConfig(s, "1", "2", 7001).find("PTT RIG 1037 COM5 38400\n") != std::string::npos);
    s.rigModel = 2;      // rigctld: host:port, no serial speed
    s.catPort = "localhost:4532";
    CHECK(direwolfConfig(s, "1", "2", 7001).find("PTT RIG 2 localhost:4532\n") != std::string::npos);

    auto atest = atestArgs(s, "in.wav");
    CHECK(contains(atest, {"-B", "300", "-h", "in.wav"}));
    auto gen = genPacketsArgs(s, "out.wav");
    CHECK(contains(gen, {"-B", "300"}));
    CHECK(contains(gen, {"-o", "out.wav", "-"}));
}

void testAx25Audio() {
    // The FCS of "123456789" is CRC-16/X.25's check value
    std::vector<uint8_t> digits = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(fsk::ax25Fcs(digits) == 0x906E);
    fsk::Audio audio = fsk::ax25({packet::encodeUi("N0CALL", "CQ", "x")}, 1200, 48000);
    CHECK(audio.rate == 48000);
    CHECK(!audio.samples.empty());
}

void testAccuracy() {
    CHECK(textAccuracy("THE FOX", "THE FOX") == 1.0f);
    CHECK(textAccuracy("THE FOX", "\r\nTHE   FOX\n") == 1.0f);   // Spacing doesn't count
    CHECK(textAccuracy("ABCDEFGHIJ", "ABCDEFGHI") == 0.9f);       // One character lost
    CHECK(textAccuracy("ABC", "") == 0.0f);
    CHECK(textAccuracy("", "") == 1.0f);
    CHECK(normalizeText("  A \r\n B  ") == "A B");
}

int runUnit() {
    struct Test {
        const char* name;
        void (*run)();
    };
    const Test tests[] = {
        {"Shift vs baud rating", testShiftFit},
        {"Settings save and load", testSettingsRoundTrip},
        {"Broken settings file", testSettingsGarbage},
        {"Mark and space tones", testTones},
        {"minimodem command lines", testMinimodemArgs},
        {"Baudot encoding", testBaudot},
        {"Signal length", testSignalLength},
        {"Accuracy scoring", testAccuracy},
        {"Packet settings", testPacketSettings},
        {"Callsigns", testCallsigns},
        {"AX.25 frames", testAx25Frames},
        {"Splitting long text", testSplitInfo},
        {"KISS framing", testKiss},
        {"Reading atest's hex dump", testAtestDump},
        {"gen_packets lines", testMonitorLines},
        {"direwolf config and command lines", testDirewolfConfig},
        {"AX.25 test audio", testAx25Audio},
    };
    string summary = "### Unit tests (" + runnerName() + ")\n\n| Result | Test | Checks |\n|---|---|---|\n";
    for (const Test& test : tests) {
        int failuresBefore = failures, checksBefore = checks;
        test.run();
        int ran = checks - checksBefore, failed = failures - failuresBefore;
        std::printf("%s  %s (%d checks)\n", failed ? "FAIL" : "PASS", test.name, ran);
        summary += string("| ") + (failed ? "❌ FAIL" : "✅ PASS") + " | " + test.name + " | " +
                   std::to_string(ran - failed) + "/" + std::to_string(ran) + " |\n";
    }
    std::printf("%d of %d checks passed\n", checks - failures, checks);
    summary += "\n**" + std::to_string(checks - failures) + " of " + std::to_string(checks) +
               " checks passed**\n";
    if (!failureLines.empty()) {
        summary += "\nFailed checks in `tests/hamproof_tests.cpp`:\n";
        for (const string& line : failureLines) summary += "- " + line + "\n";
    }
    appendSummary(summary + "\n");
    return failures == 0 ? 0 : 1;
}

int runAudio(const std::string& dir) {
    bool haveMinimodem = minimodemAvailable(), haveAtest = atestAvailable();
    if (!haveMinimodem && !haveAtest) {
        std::printf("Neither minimodem nor atest found; skipping the audio tests\n");
        annotate("notice", "tests/audio/README.md", 0, "Audio tests skipped",
                 "minimodem and direwolf aren't installed on this runner, so tests/audio wasn't decoded here");
        appendSummary("### Audio tests (" + runnerName() + ")\n\n⏭️ Skipped: minimodem and direwolf "
                      "aren't installed on this runner.\n\n");
        return 77;
    }
    int failed = 0, skipped = 0;
    string summary = "### Audio tests (" + runnerName() + ")\n\nEach file in `tests/audio` decoded "
                     "by minimodem (or direwolf's atest, for packets) with the program's own settings.\n\n"
                     "| Result | Right | Needed | File | What it tests |\n|---|---|---|---|---|\n";
    string failureDetails;
    for (const AudioCase& c : standardCases()) {
        if (!(c.settings.packet() ? haveAtest : haveMinimodem)) {
            const char* tool = c.settings.packet() ? "atest" : "minimodem";
            std::printf("SKIP            %s (%s not installed)\n", c.file.c_str(), tool);
            summary += "| ⏭️ SKIP | | | `" + c.file + "` | " + cell(c.description) + " (" + tool +
                       " not installed) |\n";
            skipped++;
            continue;
        }
        CaseResult result = runCase(c, dir);
        std::printf("%s %3.0f%% (need %.0f%%)  %s\n", result.passed ? "PASS" : "FAIL",
                    result.accuracy * 100, c.minAccuracy * 100, c.file.c_str());
        summary += string("| ") + (result.passed ? "✅ PASS" : "❌ FAIL") + " | " +
                   percent(result.accuracy) + " | " + percent(c.minAccuracy) + " | `" + c.file +
                   "` | " + cell(c.description) + " |\n";
        if (result.passed) continue;

        failed++;
        string expected = c.text.empty() ? "(nothing)" : c.text;
        string detail = result.error.empty()
            ? "expected: " + expected + "\ndecoded:  " + result.decoded
            : result.error;
        string indented = detail;
        for (size_t at = indented.find('\n'); at != string::npos; at = indented.find('\n', at + 6))
            indented.replace(at, 1, "\n     ");
        std::printf("     %s\n", indented.c_str());
        annotate("error", "tests/audio/" + c.file, 0, "Audio test failed: " + c.file,
                 percent(result.accuracy) + " right, needs " + percent(c.minAccuracy) + "\n" + detail);
        failureDetails += "\n**`" + c.file + "`** (" + cell(describe(c.settings)) + ")\n```\n" +
                    detail + "\n```\n";
    }
    int total = static_cast<int>(standardCases().size()) - skipped;
    std::printf("%d of %d passed", total - failed, total);
    if (skipped) std::printf(" (%d skipped)", skipped);
    std::printf("\n");
    summary += "\n**" + std::to_string(total - failed) + " of " + std::to_string(total) +
               " passed**" + (skipped ? " (" + std::to_string(skipped) + " skipped)" : "") + "\n" +
               failureDetails;
    appendSummary(summary + "\n");
    return failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    std::string command = argc > 1 ? argv[1] : "";
    if (command == "unit") return runUnit();
    if (command == "audio" && argc > 2) return runAudio(argv[2]);
    if (command == "generate" && argc > 2) {
        std::string error;
        if (!writeTestAudio(argv[2], error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        std::printf("Wrote the test files to %s\n", argv[2]);
        return 0;
    }
    std::fprintf(stderr, "Usage: %s unit | audio <dir> | generate <dir>\n", argv[0]);
    return 2;
}
