// options.h - Choices for the settings menus.
#ifndef HAMPROOF_OPTIONS_H
#define HAMPROOF_OPTIONS_H

struct Option {
    float value;
    const char* label;
};

const Option catBaudRates[] = {
    {4800.0f,   "4800"},
    {9600.0f,   "9600"},
    {19200.0f,  "19200"},
    {38400.0f,  "38400"},
    {115200.0f, "115200"},
};

const Option baudRates[] = {
    {45.45f, "45.45  (amateur)"},
    {50.0f,  "50     (weather)"},
    {75.0f,  "75"},
    {100.0f, "100"},
    {110.0f, "110"},
    {300.0f, "300"},
};
const Option centreFreqs[] = {
    {1000.0f, "1000 Hz"},
    {1360.0f, "1360 Hz  (1275/1445)"},
    {1500.0f, "1500 Hz"},
    {2210.0f, "2210 Hz  (2125/2295)"},
};
const Option shifts[] = {
    {170.0f, "170 Hz  (amateur)"},
    {85.0f,  "85 Hz"},
    {200.0f, "200 Hz"},
    {425.0f, "425 Hz  (maritime)"},
    {450.0f, "450 Hz  (weather)"},
    {850.0f, "850 Hz"},
};
// minimodem -c: how much stronger one tone must be than the other. Noise is about 1-2
const Option squelchLevels[] = {
    {1.5f, "Off     (1.5)"},
    {2.5f, "Low     (2.5)"},
    {3.5f, "Medium  (3.5)"},
    {5.0f, "High    (5.0)"},
    {8.0f, "Max     (8.0)"},
};

#endif //HAMPROOF_OPTIONS_H