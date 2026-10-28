// cat.h - Radio control through Hamlib.
#ifndef HAMPROOF_CAT_H
#define HAMPROOF_CAT_H

#include <string>
#include <vector>

#include <hamlib/rig.h>

struct RadioModel {
    rig_model_t model;
    const char* name;
};

const std::vector<RadioModel>& commonRadios();
// "" if Hamlib doesn't know the model
std::string radioName(rig_model_t model);

// Every call throws std::runtime_error on failure
class Cat {
public:
    // model 1 = dummy rig, 2 = rigctld (port is host:port)
    Cat(rig_model_t model, const std::string& port, int baud = 19200);
    ~Cat();

    Cat(const Cat&) = delete;
    Cat& operator=(const Cat&) = delete;

    double frequency();                 // Hz
    void setFrequency(double hz);

    rmode_t mode();
    void setMode(rmode_t mode, pbwidth_t width = RIG_PASSBAND_NORMAL);

    bool transmitting();
    void setPtt(bool on);

    int sMeter();                       // dB relative to S9 (S9 = 0, S0 = -54)

private:
    RIG* rig_;
};

// Keys PTT while it exists
class PttGuard {
public:
    explicit PttGuard(Cat& cat) : cat_(cat) { cat_.setPtt(true); }
    ~PttGuard() {
        try { cat_.setPtt(false); } catch (...) {}
    }

    PttGuard(const PttGuard&) = delete;
    PttGuard& operator=(const PttGuard&) = delete;

private:
    Cat& cat_;
};


#endif //HAMPROOF_CAT_H
