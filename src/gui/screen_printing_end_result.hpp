#pragma once

#include <window.hpp>
#include <window_text.hpp>
#include <window_icon.hpp>
#include <window_print_progress.hpp>
#include <gcode_info.hpp>
#include <marlin_vars.hpp>
#include <guiconfig.h>
#include <fonts.hpp>

/**
 * @brief Just the body, not the actual screen. However, this is the file where the screen should eventually be.
 *
 */
class EndResultBody : public AddSuperWindow<window_frame_t> {
public:
    using DateBufferT = std::array<char, sizeof("17/10/2023 18:00 AM") + 4>;

    EndResultBody(window_t *parent, Rect16 rect);

    // 'overloaded' window Hide
    void Hide();
    // 'overloaded' window Show
    void Show();

    // in header because it's used by screen_printing as well
    static constexpr const char *txt_printing_time { N_("Printing time") };

    static constexpr auto progress_font {
#if defined(USE_ILI9488)
        IDR_FNT_LARGE
#elif defined(USE_ST7789)
        IDR_FNT_NORMAL
#endif
    };
    static constexpr auto progress_alignment { Align_t::RightTop() };
    static constexpr int16_t extra_top_space { 6 }; // required extra top space to properly offset progress_txt, otherwise if just setting progress txt rect top to be 'above' what this frame holds, it will just not draw at all

    static Rect16 get_progress_txt_rect(int16_t row_0);

protected:
    void windowEvent(EventLock /*has private ctor*/, window_t *sender, GUI_event_t event, void *param) override;

private:
    /**
     * @brief row 0 is what is considered to be the top line. Extra top space is required to properly draw progress text which is offset 'above' row_0 to appear as if it were on row_0
     *
     */
    int16_t get_row_0() const;

    window_text_t printing_time_label;
    window_text_t printing_time_value;
    std::array<char, sizeof("100d 20h 30m") + 4> printing_time_value_buffer;

    window_text_t print_started_label;
    window_text_t print_started_value;
    DateBufferT print_started_value_buffer;

    window_text_t print_ended_label;
    window_text_t print_ended_value;
    DateBufferT print_ended_value_buffer;

    window_text_t consumed_material_label;
    std::array<window_text_t, EXTRUDERS> consumed_material_values;
    std::array<std::array<char, sizeof("T1 HIFIPETG 10.000g") + 4>, EXTRUDERS> consumed_material_values_buffers;

    window_text_t consumed_wipe_tower_value;
    std::array<char, sizeof("Wipe Tower 10.000g") + 4> consumed_wipe_tower_value_buffer;

    window_icon_t arrow_right;
    WindowNumbPrintProgress progress_txt;
};
