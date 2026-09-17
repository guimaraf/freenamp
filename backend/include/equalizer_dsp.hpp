#pragma once

#include <array>
#include <string>
#include <sstream>
#include <iomanip>

namespace freenamp::backend {

class EqualizerDsp {
public:
    static constexpr size_t NUM_BANDS = 10;
    static constexpr std::array<int, NUM_BANDS> FREQUENCIES = {
        60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000
    };

    EqualizerDsp() {
        m_band_gains.fill(0.0f);
    }

    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

    void set_preamp(float gain_db) {
        if (gain_db < -12.0f) gain_db = -12.0f;
        if (gain_db > 12.0f) gain_db = 12.0f;
        m_preamp_db = gain_db;
    }
    float get_preamp() const { return m_preamp_db; }

    void set_band_gain(size_t band_idx, float gain_db) {
        if (band_idx >= NUM_BANDS) return;
        if (gain_db < -12.0f) gain_db = -12.0f;
        if (gain_db > 12.0f) gain_db = 12.0f;
        m_band_gains[band_idx] = gain_db;
    }

    float get_band_gain(size_t band_idx) const {
        if (band_idx >= NUM_BANDS) return 0.0f;
        return m_band_gains[band_idx];
    }

    const std::array<float, NUM_BANDS>& get_all_gains() const {
        return m_band_gains;
    }

    // Generates the FFmpeg lavfi / libmpv audio filter string
    // e.g. "lavfi=[firequalizer=gain_entry='entry(60,0);...']" or volume filter for preamp
    std::string build_mpv_filter_string() const {
        if (!m_enabled) {
            return "";
        }

        std::ostringstream oss;
        oss << "lavfi=[volume=" << std::fixed << std::setprecision(1) << m_preamp_db << "dB,firequalizer=gain_entry='";
        for (size_t i = 0; i < NUM_BANDS; ++i) {
            if (i > 0) oss << ";";
            oss << "entry(" << FREQUENCIES[i] << "," << std::fixed << std::setprecision(1) << m_band_gains[i] << ")";
        }
        oss << "']";
        return oss.str();
    }

private:
    bool m_enabled = false;
    float m_preamp_db = 0.0f;
    std::array<float, NUM_BANDS> m_band_gains;
};

} // namespace freenamp::backend
