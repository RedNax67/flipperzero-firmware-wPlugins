#include "transmitter.h"
#include "../subghz_i.h"

#include <input/input.h>
#include <gui/elements.h>

#include <lib/subghz/protocols/keeloq.h>
#include <lib/subghz/protocols/alutech_at_4n.h>

struct SubGhzViewTransmitter {
    View* view;
    SubGhzViewTransmitterCallback callback;
    void* context;
};

typedef struct {
    FuriString* frequency_str;
    FuriString* preset_str;
    FuriString* key_str;
    uint8_t show_button;
    FuriString* temp_button_id;
    bool draw_temp_button;
} SubGhzViewTransmitterModel;

void subghz_view_transmitter_set_callback(
    SubGhzViewTransmitter* subghz_transmitter,
    SubGhzViewTransmitterCallback callback,
    void* context) {
    furi_assert(subghz_transmitter);

    subghz_transmitter->callback = callback;
    subghz_transmitter->context = context;
}

void subghz_view_transmitter_add_data_to_show(
    SubGhzViewTransmitter* subghz_transmitter,
    const char* key_str,
    const char* frequency_str,
    const char* preset_str,
    uint8_t show_button) {
    furi_assert(subghz_transmitter);
    with_view_model(
        subghz_transmitter->view,
        SubGhzViewTransmitterModel * model,
        {
            furi_string_set(model->key_str, key_str);
            furi_string_set(model->frequency_str, frequency_str);
            furi_string_set(model->preset_str, preset_str);
            model->show_button = show_button;
        },
        true);
}

static void subghz_view_transmitter_button_right(Canvas* canvas, const char* str) {
    const uint8_t button_height = 12;
    const uint8_t vertical_offset = 3;
    const uint8_t horizontal_offset = 1;
    const uint8_t string_width = canvas_string_width(canvas, str);
    const Icon* icon = &I_ButtonCenter_7x7;
    const uint8_t icon_offset = 3;
    const uint8_t icon_width_with_offset = icon_get_width(icon) + icon_offset;
    const uint8_t button_width = string_width + horizontal_offset * 2 + icon_width_with_offset;

    const uint8_t x = (canvas_width(canvas) - button_width) / 2 + 40;
    const uint8_t y = canvas_height(canvas);

    canvas_draw_box(canvas, x, y - button_height, button_width, button_height);

    canvas_draw_line(canvas, x - 1, y, x - 1, y - button_height + 0);
    canvas_draw_line(canvas, x - 2, y, x - 2, y - button_height + 1);
    canvas_draw_line(canvas, x - 3, y, x - 3, y - button_height + 2);

    canvas_draw_line(canvas, x + button_width + 0, y, x + button_width + 0, y - button_height + 0);
    canvas_draw_line(canvas, x + button_width + 1, y, x + button_width + 1, y - button_height + 1);
    canvas_draw_line(canvas, x + button_width + 2, y, x + button_width + 2, y - button_height + 2);

    canvas_invert_color(canvas);
    canvas_draw_icon(
        canvas,
        x + horizontal_offset,
        y - button_height + vertical_offset - 1,
        &I_ButtonCenter_7x7);
    canvas_draw_str(
        canvas, x + horizontal_offset + icon_width_with_offset, y - vertical_offset, str);
    canvas_invert_color(canvas);
}

void subghz_view_transmitter_draw(Canvas* canvas, SubGhzViewTransmitterModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(
        canvas, 0, 0, AlignLeft, AlignTop, furi_string_get_cstr(model->key_str));
    canvas_draw_str(canvas, 78, 7, furi_string_get_cstr(model->frequency_str));
    canvas_draw_str(canvas, 113, 7, furi_string_get_cstr(model->preset_str));

    if(model->draw_temp_button) {
        canvas_set_font(canvas, FontBatteryPercent);
        canvas_draw_str(canvas, 117, 40, furi_string_get_cstr(model->temp_button_id));
        canvas_set_font(canvas, FontSecondary);
    }

    if(model->show_button) {
        canvas_draw_str(canvas, 58, 62, furi_hal_subghz_get_radio_type() ? "R: Ext" : "R: Int");
        subghz_view_transmitter_button_right(canvas, "Send");
    }
}

bool subghz_view_transmitter_input(InputEvent* event, void* context) {
    furi_assert(context);
    SubGhzViewTransmitter* subghz_transmitter = context;
    bool can_be_sent = false;

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        with_view_model(
            subghz_transmitter->view,
            SubGhzViewTransmitterModel * model,
            {
                furi_string_reset(model->frequency_str);
                furi_string_reset(model->preset_str);
                furi_string_reset(model->key_str);
                furi_string_reset(model->temp_button_id);
                model->show_button = 0;
                model->draw_temp_button = false;
            },
            false);
        return false;
    }

    with_view_model(
        subghz_transmitter->view,
        SubGhzViewTransmitterModel * model,
        {
            if(model->show_button) {
                can_be_sent = true;
            }
        },
        true);

    if(can_be_sent && event->key == InputKeyOk && event->type == InputTypePress) {
        with_view_model(
            subghz_transmitter->view,
            SubGhzViewTransmitterModel * model,
            {
                furi_string_reset(model->temp_button_id);
                model->draw_temp_button = false;
            },
            true);
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStart, subghz_transmitter->context);
        return true;
    } else if(can_be_sent && event->key == InputKeyOk && event->type == InputTypeRelease) {
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStop, subghz_transmitter->context);
        return true;
    }

    // Temp Buttons (UP)
    if(can_be_sent && event->key == InputKeyUp && event->type == InputTypePress) {
        keeloq_set_btn(1);
        alutech_set_btn(1);
        with_view_model(
            subghz_transmitter->view,
            SubGhzViewTransmitterModel * model,
            {
                furi_string_reset(model->temp_button_id);
                if(keeloq_get_original_btn() != 0) {
                    furi_string_printf(model->temp_button_id, "%01X", keeloq_get_original_btn());
                    model->draw_temp_button = true;
                } else if(alutech_get_original_btn() != 0) {
                    furi_string_printf(model->temp_button_id, "%01X", alutech_get_original_btn());
                    model->draw_temp_button = true;
                }
            },
            true);
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStart, subghz_transmitter->context);
        return true;
    } else if(can_be_sent && event->key == InputKeyUp && event->type == InputTypeRelease) {
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStop, subghz_transmitter->context);
        return true;
    }
    // Down
    if(can_be_sent && event->key == InputKeyDown && event->type == InputTypePress) {
        keeloq_set_btn(2);
        alutech_set_btn(2);
        with_view_model(
            subghz_transmitter->view,
            SubGhzViewTransmitterModel * model,
            {
                furi_string_reset(model->temp_button_id);
                if(keeloq_get_original_btn() != 0) {
                    furi_string_printf(model->temp_button_id, "%01X", keeloq_get_original_btn());
                    model->draw_temp_button = true;
                } else if(alutech_get_original_btn() != 0) {
                    furi_string_printf(model->temp_button_id, "%01X", alutech_get_original_btn());
                    model->draw_temp_button = true;
                }
            },
            true);
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStart, subghz_transmitter->context);
        return true;
    } else if(can_be_sent && event->key == InputKeyDown && event->type == InputTypeRelease) {
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStop, subghz_transmitter->context);
        return true;
    }
    // Left
    if(can_be_sent && event->key == InputKeyLeft && event->type == InputTypePress) {
        keeloq_set_btn(3);
        alutech_set_btn(3);
        with_view_model(
            subghz_transmitter->view,
            SubGhzViewTransmitterModel * model,
            {
                furi_string_reset(model->temp_button_id);
                if(keeloq_get_original_btn() != 0) {
                    furi_string_printf(model->temp_button_id, "%01X", keeloq_get_original_btn());
                    model->draw_temp_button = true;
                } else if(alutech_get_original_btn() != 0) {
                    furi_string_printf(model->temp_button_id, "%01X", alutech_get_original_btn());
                    model->draw_temp_button = true;
                }
            },
            true);
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStart, subghz_transmitter->context);
        return true;
    } else if(can_be_sent && event->key == InputKeyLeft && event->type == InputTypeRelease) {
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStop, subghz_transmitter->context);
        return true;
    }
    // Right
    if(can_be_sent && event->key == InputKeyRight && event->type == InputTypePress) {
        keeloq_set_btn(4);
        alutech_set_btn(4);
        with_view_model(
            subghz_transmitter->view,
            SubGhzViewTransmitterModel * model,
            {
                furi_string_reset(model->temp_button_id);
                if(keeloq_get_original_btn() != 0) {
                    furi_string_printf(model->temp_button_id, "%01X", keeloq_get_original_btn());
                    model->draw_temp_button = true;
                } else if(alutech_get_original_btn() != 0) {
                    furi_string_printf(model->temp_button_id, "%01X", alutech_get_original_btn());
                    model->draw_temp_button = true;
                }
            },
            true);
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStart, subghz_transmitter->context);
        return true;
    } else if(can_be_sent && event->key == InputKeyRight && event->type == InputTypeRelease) {
        subghz_transmitter->callback(
            SubGhzCustomEventViewTransmitterSendStop, subghz_transmitter->context);
        return true;
    }

    return true;
}

void subghz_view_transmitter_enter(void* context) {
    furi_assert(context);
}

void subghz_view_transmitter_exit(void* context) {
    furi_assert(context);
}

SubGhzViewTransmitter* subghz_view_transmitter_alloc() {
    SubGhzViewTransmitter* subghz_transmitter = malloc(sizeof(SubGhzViewTransmitter));

    // View allocation and configuration
    subghz_transmitter->view = view_alloc();
    view_allocate_model(
        subghz_transmitter->view, ViewModelTypeLocking, sizeof(SubGhzViewTransmitterModel));
    view_set_context(subghz_transmitter->view, subghz_transmitter);
    view_set_draw_callback(
        subghz_transmitter->view, (ViewDrawCallback)subghz_view_transmitter_draw);
    view_set_input_callback(subghz_transmitter->view, subghz_view_transmitter_input);
    view_set_enter_callback(subghz_transmitter->view, subghz_view_transmitter_enter);
    view_set_exit_callback(subghz_transmitter->view, subghz_view_transmitter_exit);

    with_view_model(
        subghz_transmitter->view,
        SubGhzViewTransmitterModel * model,
        {
            model->frequency_str = furi_string_alloc();
            model->preset_str = furi_string_alloc();
            model->key_str = furi_string_alloc();
            model->temp_button_id = furi_string_alloc();
        },
        true);
    return subghz_transmitter;
}

void subghz_view_transmitter_free(SubGhzViewTransmitter* subghz_transmitter) {
    furi_assert(subghz_transmitter);

    with_view_model(
        subghz_transmitter->view,
        SubGhzViewTransmitterModel * model,
        {
            furi_string_free(model->frequency_str);
            furi_string_free(model->preset_str);
            furi_string_free(model->key_str);
            furi_string_free(model->temp_button_id);
        },
        true);
    view_free(subghz_transmitter->view);
    free(subghz_transmitter);
}

View* subghz_view_transmitter_get_view(SubGhzViewTransmitter* subghz_transmitter) {
    furi_assert(subghz_transmitter);
    return subghz_transmitter->view;
}
