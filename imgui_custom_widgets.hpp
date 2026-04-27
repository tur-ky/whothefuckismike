#pragma once
#include <imgui.h>
extern ImFont* g_BoldFont;

extern ImFont* g_HeaderFont;

namespace UI {
    void ApplyBrutalistTheme();

    bool CircularKnob(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f");

    bool Intellipan(const char* label, float* x, float* y, int* current_mode);

    bool DrawPanBox(const char* label, float* x, float* y);

    bool CustomGainSlider(const char* label, float* v, float v_min, float v_max, const char* strip_name = nullptr, const char* format = "%.1f");

    bool ToggleButton(const char* label, bool* v, bool is_routing_button = false);

    bool KButton(const char* label, float* v);

    void PeakMeter(const char* label, float peak_val, float height = 80.0f);

}
