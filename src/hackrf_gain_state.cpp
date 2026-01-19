#include "hackrf_gain_state.hpp"

#include "hackrf_controller.hpp"

[[nodiscard]] int HackRFGainState::total_gain() const {
    return lna_gain + vga_gain + (amp_enable ? hackrf_hardware::AMP_GAIN_DB : 0);
}

[[nodiscard]] bool HackRFGainState::is_valid() const {
    const bool vga_valid = (vga_gain >= hackrf_hardware::VGA_MIN) &&
                           (vga_gain <= hackrf_hardware::VGA_MAX) &&
                           (vga_gain % hackrf_hardware::VGA_STEP == 0);
    const bool lna_valid = (lna_gain >= hackrf_hardware::LNA_MIN) &&
                           (lna_gain <= hackrf_hardware::LNA_MAX) &&
                           (lna_gain % hackrf_hardware::LNA_STEP == 0);
    return vga_valid && lna_valid;
}

void HackRFGainState::set_amp_enable(bool enable) noexcept {
    amp_enable = enable;
}

void HackRFGainState::set_vga_gain(int gain) {
    const bool vga_valid = (gain >= hackrf_hardware::VGA_MIN) &&
                           (gain <= hackrf_hardware::VGA_MAX) &&
                           (gain % hackrf_hardware::VGA_STEP == 0);

    if (!vga_valid) {
        return;
    }

    vga_gain = gain;
}

void HackRFGainState::set_lna_gain(int gain) {
    const bool lna_valid = (gain >= hackrf_hardware::LNA_MIN) &&
                           (gain <= hackrf_hardware::LNA_MAX) &&
                           (gain % hackrf_hardware::LNA_STEP == 0);

    if (!lna_valid) {
        return;
    }

    lna_gain = gain;
}
