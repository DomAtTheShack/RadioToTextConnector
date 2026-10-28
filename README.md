## Radio to Text Connector

Send and receive text over ham radio from a terminal.

- **minimodem**: RTTY (Baudot) or Bell202, a continuous character stream.
- **direwolf**: AX.25 packets at 300, 1200 or 9600 baud. One packet per line, with your
  callsign and error checking.

PTT is CAT (Hamlib) or VOX. Common Icom, Yaesu, Kenwood, Elecraft and Xiegu radios are
listed; any other Hamlib model works by number, or through `rigctld`.

### Using it

Run `RadioToTextConnector`:

- **Start** runs with the current settings.
- **Settings** changes them. They're saved to:
  - Windows: `%APPDATA%\RTTY-Connector\settings.ini`
  - macOS: `~/Library/Application Support/RTTY-Connector/settings.ini`
  - Linux: `~/.config/rtty-connector/settings.ini`
- **Test tools**: loopback self-test, test files, tuning tone, test message, CAT check.

Menu keys: Up/Down (j/k), PgUp/PgDn, Home/End, Enter to choose, Esc/q to go back, or type
an item's number.

`RadioToTextConnector [-q] [file.wav]`: `file.wav` is decoded (Receive) or written
(Transmit); `-q` hides carrier lines.

#### minimodem

- **Receive**: prints text until Esc.
- **Transmit**: Enter sends a line, Tab finishes, Esc stops.
- **Duplex**: transmit until Tab, then receive until Tab. It never switches by itself, since
  signals fade and a lost tone doesn't mean the other station has finished.

#### direwolf

Set **Modem** to direwolf, then **Packet speed** and **Callsign**. Packets go to `CQ`.

- **Receive**: shows packets as they arrive.
- **Transmit**: Enter sends a packet, Tab finishes.
- **Duplex**: send and receive at once, chat style.

Esc waits for queued packets, then stops; Esc again stops now. direwolf always needs the
Rx sound card. With an audio file, `atest` decodes and `gen_packets` writes.

### Installing the modems

- **Windows**: the build compiles direwolf with Hamlib (the official build has no CAT
  PTT). Put the source in `direwolf/`
  (`git clone --branch 1.8.1 https://github.com/wb2osz/direwolf.git`), or build with
  `-DHAMPROOF_DIREWOLF=OFF`. minimodem is the Cygwin build; install Cygwin's
  `pulseaudio` package too, which the program runs on the sound cards you pick. Allow direwolf through Windows Firewall if asked.
- **Linux**: `sudo apt install minimodem direwolf` (see `APT-PACKAGES.txt`).
- **macOS**: `brew install hamlib minimodem direwolf && brew services start pulseaudio`.
  direwolf uses the default sound devices.

The program looks on the PATH, in `/usr/local/bin`, `/opt/homebrew/bin` and next to
itself.

### Tests

`tests/audio` holds RTTY, Bell202 and packet recordings plus calibration tones (see its
README). `ctest` runs:

- `unit`: settings, command lines, Baudot, AX.25/KISS, scoring
- `audio`: decodes each test file and checks the text. Skipped if the decoder isn't
  installed

To regenerate the files: `hamproof_tests generate ../tests/audio` from the build folder.
