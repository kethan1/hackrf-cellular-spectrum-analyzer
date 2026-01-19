#ifndef HACKRF_GAIN_STATE_HPP
#define HACKRF_GAIN_STATE_HPP

class HackRFGainState {
   public:
    HackRFGainState() = default;
    HackRFGainState(bool amp, int lna, int vga)
        : amp_enable(amp), lna_gain(lna), vga_gain(vga) {}

    [[nodiscard]] int total_gain() const;
    [[nodiscard]] bool is_valid() const;

    bool get_amp_enable() const noexcept {
        return amp_enable;
    }

    [[nodiscard]] int get_vga_gain() const noexcept {
        return vga_gain;
    }

    [[nodiscard]] int get_lna_gain() const noexcept {
        return lna_gain;
    }

    void set_amp_enable(bool enable) noexcept;
    void set_vga_gain(int gain);
    void set_lna_gain(int gain);

   private:
    bool amp_enable = false;
    int vga_gain = 0;
    int lna_gain = 0;
};

#endif  // HACKRF_GAIN_STATE_HPP