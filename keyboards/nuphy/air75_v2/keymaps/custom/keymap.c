/*
Copyright 2023 @ Nuphy <https://nuphy.com/>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H
#include "lib/lib8tion/lib8tion.h"
#include "leader_codes.h"

void set_left_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_right_rgb(uint8_t r, uint8_t g, uint8_t b);
extern bool f_bat_hold;

extern DEV_INFO_STRUCT dev_info;

enum user_keycodes {
    TG_GAME = QK_USER_0, // Fn+G: off -> Factorio -> Valheim -> off
    TG_NGHT,             // Fn+N: night base
    TG_WPM,              // Fn+W: WPM heat
    TG_STDY,             // Fn+S: study base
    TG_TMR,              // Fn+P: study timer start / cancel
};

enum { GAME_OFF, GAME_FACTORIO, GAME_VALHEIM };

static uint8_t  game;
static bool     night, wpm_heat, wispr_sweep;
static uint16_t mod_time, wispr_time;

// Neovim over Raw HID: [0x4E, mode, dirty, focused, instance], resent every second
enum { NV_NONE, NV_NORMAL, NV_INSERT, NV_VISUAL, NV_COMMAND };

static uint8_t  nv_mode, nv_prev, nv_owner;
static bool     nv_dirty, nv_focused;
static uint16_t nv_change;
static uint32_t nv_seen, nv_seen_focus;

// study: Night + Anki keys; Fn+P runs a 25 min timer on F1-F12 on any base
#define FOCUS_LEN 1500000

static bool     study, focus_timer;
static uint32_t focus_start;

void raw_hid_receive(uint8_t *data, uint8_t length) {
    if (data[0] != 0x4E) return;
    // a background Neovim can't override the focused one
    if (!data[3] && data[4] != nv_owner && timer_elapsed32(nv_seen_focus) < 1500) return;
    if (data[3]) {
        nv_owner      = data[4];
        nv_seen_focus = timer_read32();
    }
    if (data[1] != nv_mode) {
        nv_prev   = nv_mode;
        nv_mode   = data[1];
        nv_change = timer_read();
    }
    nv_dirty   = data[2];
    nv_focused = data[3];
    nv_seen    = timer_read32();
}

// heard from Neovim in the last 3 s, and no game or study base on
static bool nv_alive(void) {
    return nv_mode && timer_elapsed32(nv_seen) < 3000 && !game && !study;
}

// 150 ms fade between modes
static uint8_t nv_fade(void) {
    uint16_t el = timer_elapsed(nv_change);
    return el >= 150 ? 255 : el * 255 / 150;
}

static const uint8_t nv_rgb[][3] = {
    {0x00, 0x00, 0x00}, {0x24, 0xFF, 0xD4}, {0xFF, 0xF2, 0xD6}, {0xFF, 0x14, 0x93}, {0xFF, 0x7A, 0x00},
};

// key, the mode whose color it shows (0 = this mode), brightness %
typedef struct {
    uint16_t kc;
    uint8_t  to, pct;
} nv_key_t;

#define NV_EXITS {LT(1, KC_ESC), 0, 80}, {LT(5, KC_ESC), 0, 80}

static const nv_key_t nv_normal[] = {
    {KC_H, 0, 100}, {KC_J, 0, 100}, {KC_K, 0, 100}, {KC_L, 0, 100}, {KC_W, 0, 100}, {KC_B, 0, 100}, {KC_E, 0, 100}, {KC_G, 0, 100},
    {KC_D, 0, 60}, {KC_C, 0, 60}, {KC_Y, 0, 60}, {KC_P, 0, 60}, {KC_X, 0, 60}, {KC_U, 0, 60},
    {KC_I, NV_INSERT, 100}, {KC_A, NV_INSERT, 100}, {KC_O, NV_INSERT, 100}, {KC_V, NV_VISUAL, 100}, {KC_SCLN, NV_COMMAND, 100},
    {KC_SLSH, 0, 60}, {KC_N, 0, 60}, {KC_SPC, 0, 45},
};
static const nv_key_t nv_insert[] = {NV_EXITS};
static const nv_key_t nv_visual[] = {
    {KC_H, 0, 100}, {KC_J, 0, 100}, {KC_K, 0, 100}, {KC_L, 0, 100}, {KC_W, 0, 100}, {KC_B, 0, 100}, {KC_E, 0, 100},
    {KC_D, 0, 60}, {KC_C, 0, 60}, {KC_Y, 0, 60}, {KC_COMM, 0, 60}, {KC_DOT, 0, 60}, {KC_GRV, 0, 60},
    {KC_O, 0, 80}, NV_EXITS,
};
static const nv_key_t nv_command[] = {
    {KC_ENT, 0, 100}, {KC_UP, 0, 60}, {KC_DOWN, 0, 60}, {KC_TAB, 0, 60}, {KC_W, 0, 60}, {KC_Q, 0, 60}, NV_EXITS,
};

static const struct {
    const nv_key_t *keys;
    uint8_t         n;
} nv_sets[] = {
    {NULL, 0}, {nv_normal, ARRAY_SIZE(nv_normal)}, {nv_insert, ARRAY_SIZE(nv_insert)}, {nv_visual, ARRAY_SIZE(nv_visual)}, {nv_command, ARRAY_SIZE(nv_command)},
};

// what a key shows in a Neovim mode, at 50% (the board's max)
static RGB nv_key_rgb(uint8_t mode, uint16_t kc) {
    RGB c = {0};
    for (uint8_t i = 0; i < nv_sets[mode].n; i++) {
        const nv_key_t *k = &nv_sets[mode].keys[i];
        if (k->kc != kc) continue;
        const uint8_t *src = nv_rgb[k->to ? k->to : mode];
        c.r = src[0] * k->pct / 200;
        c.g = src[1] * k->pct / 200;
        c.b = src[2] * k->pct / 200;
        break;
    }
    return c;
}

// insert mode ink: typed keys stay lit and fade over 2 s
#define INK_MAX 24

static struct {
    uint8_t  led1; // led + 1, 0 = empty
    uint16_t time;
} ink[INK_MAX];
static uint8_t ink_next;

// leader: tap Right Cmd sends Hyper+Space (Better Aliases' Expand Alias), then the
// board lights only the keys that continue a code until one is left
#define LEADER_KEY RGUI_T(KC_F13)

static bool     leader_on;
static char     leader_buf[8];
static uint8_t  leader_len;
static uint32_t leader_time;

static char leader_char(uint16_t kc, bool shift) {
    if (kc >= KC_A && kc <= KC_Z) return (shift ? 'A' : 'a') + kc - KC_A;
    if (kc >= KC_1 && kc <= KC_9) return '1' + kc - KC_1;
    if (kc == KC_0) return '0';
    if (kc == KC_COMM) return ',';
    if (kc == KC_DOT) return '.';
    return 0;
}

// codes starting with what's typed (plus next, if given); a leading , means paste
static uint8_t leader_matches(char next) {
    const char *typed = leader_buf;
    uint8_t     len   = leader_len;
    if (len && typed[0] == ',') {
        typed++;
        len--;
    }
    uint8_t n = 0;
    for (uint8_t i = 0; i < ARRAY_SIZE(leader_codes); i++) {
        if (strncmp(leader_codes[i], typed, len) != 0) continue;
        if (next && leader_codes[i][len] != next) continue;
        n++;
    }
    return n;
}

// base: each pressed key flashes dark orange and fades over 500 ms, no background
#define RIPPLE_MAX 16

static struct {
    uint8_t  led1; // led + 1, 0 = empty
    uint16_t time;
} ripples[RIPPLE_MAX];
static uint8_t  ripple_next;
static uint32_t last_press;

static void base_flash(uint8_t hue) {
    for (uint8_t r = 0; r < RIPPLE_MAX; r++) {
        if (!ripples[r].led1) continue;
        uint16_t age = timer_elapsed(ripples[r].time);
        if (age >= 500) {
            ripples[r].led1 = 0;
            continue;
        }
        RGB c = hsv_to_rgb((HSV){hue, 255, 0xBF * (500 - age) / 500});
        rgb_matrix_set_color(ripples[r].led1 - 1, c.r, c.g, c.b);
    }
}

// the reactive effect only runs underneath; the keymap draws every look over it
void keyboard_post_init_user(void) {
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE);
    rgb_matrix_sethsv_noeeprom(120, 220, rgb_matrix_get_val());
}

// games turn the Caps nav layer off
layer_state_t layer_state_set_user(layer_state_t state) {
    if (game) state &= ~((layer_state_t)1 << 5);
    return state;
}

// mint -> orange -> hot pink as typing speeds up
static uint8_t wpm_hue(uint8_t heat) {
    if (heat < 140) return 120 - 100 * heat / 140;
    return 20 - 42 * (heat - 140) / 115;
}

static uint8_t wpm_heat_level(void) {
    uint8_t wpm = get_current_wpm();
    if (wpm <= 30) return 0;
    if (wpm >= 120) return 255;
    return (wpm - 30) * 255 / 90;
}

// rails: Wispr pink > layer (violet nav, red Fn) > WPM heat > game > off
void side_led_show_user(void) {
    uint8_t r = 0x00, g = 0x00, b = 0x00;
    uint8_t layer = get_highest_layer(layer_state);

    if (get_mods() & MOD_BIT(KC_RALT)) {
        uint8_t br = 90 + scale8(165, sin8(timer_read() / 6));
        r = br; g = scale8(20, br); b = scale8(147, br);
    } else if (leader_on) {
        r = 0xBF; g = 0x8A; b = 0x00;
    } else if (layer == 5) {
        r = 0x90; g = 0x00; b = 0xFF;
    } else if (layer == 1 || layer == 3) {
        r = 0xFF; g = 0x00; b = 0x00;
    } else if (nv_alive()) {
        uint8_t k   = nv_fade();
        uint8_t pct = nv_focused ? 75 : 25;
        r = lerp8by8(nv_rgb[nv_prev][0], nv_rgb[nv_mode][0], k) * pct / 100;
        g = lerp8by8(nv_rgb[nv_prev][1], nv_rgb[nv_mode][1], k) * pct / 100;
        b = lerp8by8(nv_rgb[nv_prev][2], nv_rgb[nv_mode][2], k) * pct / 100;
    } else if (wpm_heat && !game) {
        uint8_t heat = wpm_heat_level();
        RGB     c    = hsv_to_rgb((HSV){wpm_hue(heat), 255, 64 + scale8(191, heat)});
        r = c.r; g = c.g; b = c.b;
    } else if (game == GAME_FACTORIO) {
        r = 0x80; g = 0x3D; b = 0x00;
    } else if (game == GAME_VALHEIM) {
        r = 0x73; g = 0x09; b = 0x42;
    } else if (!night && !study) {
        // dark orange, 50% at rest easing up to 75% while you type
        static uint8_t  pct = 50;
        static uint16_t step;
        if (timer_elapsed(step) > 20) {
            step         = timer_read();
            uint8_t want = timer_elapsed32(last_press) < 800 ? 75 : 50;
            if (pct < want) pct++;
            if (pct > want) pct--;
        }
        r = 0xFF * pct / 100; g = 0x50 * pct / 100; b = 0x00;
    }

    set_left_rgb(r, g, b);
    set_right_rgb(r, g, b);
}

// right side splash: ring grows with how much the key does
#define SPLASH_MAX 4

typedef struct {
    uint8_t  x, y, size;
    uint16_t time;
} splash_t;

static splash_t splashes[SPLASH_MAX];
static uint8_t  splash_next;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        if (keycode == KC_RALT) {
            wispr_sweep = true;
            wispr_time  = timer_read();
        }
        return true;
    }

    // leader: follow the code as it's typed; modifiers pass, anything else ends it
    if (leader_on && keycode != LEADER_KEY && !(keycode >= KC_LCTL && keycode <= KC_RGUI)) {
        char c = leader_char(keycode, get_mods() & MOD_MASK_SHIFT);
        if (keycode == KC_BSPC && leader_len) {
            leader_len--;
        } else if (c && leader_len < sizeof(leader_buf)) {
            leader_buf[leader_len++] = c;
            if (!(leader_len == 1 && c == ',') && leader_matches(0) <= 1) leader_on = false;
        } else {
            leader_on = false;
        }
        leader_time = timer_read32();
    }

    switch (keycode) {
        case LEADER_KEY:
            if (record->tap.count) {
                tap_code16(HYPR(KC_SPC));
                leader_on   = true;
                leader_len  = 0;
                leader_time = timer_read32();
                return false;
            }
            mod_time = timer_read();
            break;
        case TG_GAME:
            game = (game + 1) % 3;
            return false;
        case TG_NGHT:
            night = !night;
            return false;
        case TG_WPM:
            wpm_heat = !wpm_heat;
            return false;
        case TG_STDY:
            study = !study;
            return false;
        case TG_TMR:
            focus_timer = !focus_timer;
            focus_start = timer_read32();
            return false;
        case KC_LGUI:
        case KC_RGUI:
        case KC_LALT:
            mod_time = timer_read();
            break;
    }

    uint8_t size = 0;
    switch (keycode) {
        case KC_PGUP:
        case KC_PGDN:
            size = 1;
            break;
        case KC_HOME:
        case KC_END:
            size = 2;
            break;
        case KC_DEL:
            size = 3;
            break;
    }

    uint8_t led = g_led_config.matrix_co[record->event.key.row][record->event.key.col];
    if (nv_alive() && nv_focused && nv_mode == NV_INSERT && led != NO_LED) {
        ink[ink_next].led1 = led + 1;
        ink[ink_next].time = timer_read();
        ink_next           = (ink_next + 1) % INK_MAX;
    }
    last_press = timer_read32();
    if (led != NO_LED) {
        ripples[ripple_next].led1 = led + 1;
        ripples[ripple_next].time = timer_read();
        ripple_next               = (ripple_next + 1) % RIPPLE_MAX;
    }
    if (size && led != NO_LED) {
        splashes[splash_next] = (splash_t){g_led_config.point[led].x, g_led_config.point[led].y, size, timer_read()};
        splash_next = (splash_next + 1) % SPLASH_MAX;
    }
    return true;
}

// light every key whose Mac base keycode is in the list
static void paint(const uint16_t *kcs, uint8_t n, uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint8_t led = g_led_config.matrix_co[row][col];
            if (led == NO_LED) continue;
            uint16_t kc = keymap_key_to_keycode(0, (keypos_t){.row = row, .col = col});
            for (uint8_t i = 0; i < n; i++) {
                if (kc == kcs[i]) rgb_matrix_set_color(led, r, g, b);
            }
        }
    }
}
#define PAINT(kcs, r, g, b) paint(kcs, ARRAY_SIZE(kcs), r, g, b)

// light every key that does something on a layer
static void legend(uint8_t layer, uint8_t r, uint8_t g, uint8_t b) {
    rgb_matrix_set_color_all(0, 0, 0);
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint8_t led = g_led_config.matrix_co[row][col];
            if (led == NO_LED) continue;
            if (keymap_key_to_keycode(layer, (keypos_t){.row = row, .col = col}) != KC_TRNS) rgb_matrix_set_color(led, r, g, b);
        }
    }
}

static const uint16_t wasd[]     = {KC_W, KC_A, KC_S, KC_D};
static const uint16_t numrow[]   = {KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0};
static const uint16_t cmd_keys[] = {KC_Q, KC_W, KC_E, KC_R, KC_T, KC_A, KC_S, KC_F, KC_Z, KC_X, KC_C, KC_V, KC_N};
static const uint16_t opt_keys[] = {KC_LEFT, KC_RGHT, KC_UP, KC_DOWN, KC_BSPC, KC_DEL};

// Factorio defaults plus your config.ini: ` rotate quickbars, L lane (mod)
static const uint16_t fac_build[] = {KC_E, KC_Q, KC_R, KC_T, KC_M, KC_Z, KC_F, KC_GRV, KC_L};
static const uint16_t fac_mods[]  = {KC_LSFT, KC_LCTL, KC_SPC};

// Valheim from its plist
static const uint16_t val_move[] = {KC_SPC, KC_LSFT, KC_LCTL, KC_Q, KC_C};
static const uint16_t val_act[]  = {KC_E, KC_TAB, KC_R, KC_F, KC_G, KC_V, KC_X, KC_M, KC_LBRC, KC_LGUI};
static const uint16_t val_bar[]  = {KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8};

bool rgb_matrix_indicators_user(void) {
    static const uint8_t reach[] = {0, 25, 60, 160};
    uint8_t              mods    = get_mods();
    uint8_t              layer   = get_highest_layer(layer_state);

    // key flashes and splashes are dark orange, WPM heat retints them
    uint8_t hue = (wpm_heat && !game) ? wpm_hue(wpm_heat_level()) : 14;

    // base: games, study and night replace the dark orange key flashes
    if (game == GAME_FACTORIO) {
        rgb_matrix_set_color_all(0, 0, 0);
        PAINT(numrow, 0x59, 0x2B, 0x00);
        PAINT(fac_build, 0x66, 0x08, 0x3B);
        PAINT(fac_mods, 0x1F, 0x0F, 0x00);
        PAINT(wasd, 0x30, 0x00, 0x00);
    } else if (game == GAME_VALHEIM) {
        rgb_matrix_set_color_all(0, 0, 0);
        PAINT(val_bar, 0x26, 0x12, 0x00);
        PAINT(val_move, 0x59, 0x2B, 0x00);
        PAINT(val_act, 0x66, 0x08, 0x3B);
        PAINT(wasd, 0x30, 0x00, 0x00);
    } else if (study) {
        static const uint16_t homing[] = {KC_F, KC_J};
        static const uint16_t again[]  = {KC_1, KC_J};
        static const uint16_t hard[]   = {KC_2, KC_K};
        static const uint16_t good[]   = {KC_3, KC_L};
        static const uint16_t easy[]   = {KC_4, KC_SCLN};
        static const uint16_t reveal[] = {KC_SPC};
        rgb_matrix_set_color_all(0x0D, 0x06, 0x02);
        PAINT(homing, 0x33, 0x18, 0x08);
        PAINT(again, 0x4D, 0x0C, 0x09);
        PAINT(hard, 0x4D, 0x2A, 0x00);
        PAINT(good, 0x18, 0x42, 0x1B);
        PAINT(easy, 0x18, 0x33, 0x4D);
        PAINT(reveal, 0x4D, 0x24, 0x0C);

    } else if (night) {
        static const uint16_t homing[] = {KC_F, KC_J};
        rgb_matrix_set_color_all(0x0D, 0x06, 0x02);
        PAINT(homing, 0x33, 0x18, 0x08);
    } else {
        rgb_matrix_set_color_all(0, 0, 0);
        base_flash(hue);
    }

    // Neovim (focused): mode keys fade in, W orange while unsaved, ink in insert
    if (nv_alive() && nv_focused) {
        uint8_t k = nv_fade();
        rgb_matrix_set_color_all(0, 0, 0);
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                uint8_t led = g_led_config.matrix_co[row][col];
                if (led == NO_LED) continue;
                uint16_t kc = keymap_key_to_keycode(0, (keypos_t){.row = row, .col = col});
                RGB      a  = nv_key_rgb(nv_prev, kc);
                RGB      c  = nv_key_rgb(nv_mode, kc);
                if (nv_dirty && kc == KC_W) c = (RGB){.r = 0x80, .g = 0x3D, .b = 0x00};
                rgb_matrix_set_color(led, lerp8by8(a.r, c.r, k), lerp8by8(a.g, c.g, k), lerp8by8(a.b, c.b, k));
            }
        }
        for (uint8_t i = 0; nv_mode == NV_INSERT && i < INK_MAX; i++) {
            if (!ink[i].led1) continue;
            uint16_t age = timer_elapsed(ink[i].time);
            if (age >= 2000) {
                ink[i].led1 = 0;
                continue;
            }
            uint8_t v = 0x52 * (2000 - age) / 2000;
            rgb_matrix_set_color(ink[i].led1 - 1, v, scale8(v, 0xF2), scale8(v, 0xD6));
        }
    }

    // Fn+P timer, any base: F1-F12 in teal drain over 25 min (one about every 2 min), teal ring from G when done
    uint32_t el = timer_elapsed32(focus_start);
    if (focus_timer && el < FOCUS_LEN) {
        uint8_t left = (12 * (FOCUS_LEN - el) + FOCUS_LEN - 1) / FOCUS_LEN;
        for (uint8_t col = 1; col <= left; col++) {
            uint8_t led = g_led_config.matrix_co[0][col];
            if (led != NO_LED) rgb_matrix_set_color(led, 0x12, 0x80, 0x6A);
        }
    } else if (focus_timer && el < FOCUS_LEN + 1200) {
        uint8_t g      = g_led_config.matrix_co[3][5];
        uint8_t radius = (el - FOCUS_LEN) * 170 / 1200;
        uint8_t fade   = 255 - (el - FOCUS_LEN) * 255 / 1200;
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            int16_t dx   = g_led_config.point[i].x - g_led_config.point[g].x;
            int16_t dy   = g_led_config.point[i].y - g_led_config.point[g].y;
            int16_t ring = abs(sqrt16(dx * dx + dy * dy) - radius);
            if (ring < 16) rgb_matrix_set_color(i, scale8(0x12, fade) * (16 - ring) / 16, scale8(0x80, fade) * (16 - ring) / 16, scale8(0x6A, fade) * (16 - ring) / 16);
        }
    } else {
        focus_timer = false;
    }

    // Fn+\ battery: number row gauge red -> green, WASD low red
    if (f_bat_hold) {
        uint8_t lit = (dev_info.rf_baterry + 5) / 10;
        for (uint8_t i = 0; i < 10 && i < lit; i++) {
            paint(&numrow[i], 1, 0x80 - i * 0x6C / 9, 0x0F + i * 0x64 / 9, 0x00 + i * 0x1E / 9);
        }
        PAINT(wasd, 0x30, 0x00, 0x00);
    }

    // Wispr: hold R Opt to dictate, mint sweep on release
    if (mods & MOD_BIT(KC_RALT)) {
        static const uint16_t ropt[] = {KC_RALT};
        rgb_matrix_set_color_all(0x01, 0x05, 0x04);
        PAINT(ropt, 0x99, 0x0C, 0x58);
    } else if (wispr_sweep) {
        uint16_t elapsed = timer_elapsed(wispr_time);
        if (elapsed >= 600) {
            wispr_sweep = false;
        } else {
            int16_t sx = (int32_t)elapsed * 170 / 600 - 10;
            for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
                int16_t d = abs(g_led_config.point[i].x - sx);
                if (d < 14) rgb_matrix_set_color(i, 0x12 * (14 - d) / 14, 0x80 * (14 - d) / 14, 0x6A * (14 - d) / 14);
            }
        }
    }

    // mod legend: Cmd or left Opt held past 0.3 s (off in games)
    if (!game && timer_elapsed(mod_time) > 300) {
        if (mods & MOD_MASK_GUI) PAINT(cmd_keys, 0x48, 0x4E, 0x59);
        else if (mods & MOD_BIT(KC_LALT)) PAINT(opt_keys, 0x10, 0x70, 0x5D);
    }

    // leader legend: gold on keys that continue a code, comma dimmer as "paste"
    if (leader_on && timer_elapsed32(leader_time) > 3000) leader_on = false;
    if (leader_on) {
        rgb_matrix_set_color_all(0, 0, 0);
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                uint8_t led = g_led_config.matrix_co[row][col];
                if (led == NO_LED) continue;
                uint16_t kc    = keymap_key_to_keycode(0, (keypos_t){.row = row, .col = col});
                char     lower = leader_char(kc, false), upper = leader_char(kc, true);
                if (lower == ',' && leader_len == 0) {
                    rgb_matrix_set_color(led, 0x60, 0x45, 0x00);
                } else if ((lower && leader_matches(lower)) || (upper != lower && leader_matches(upper))) {
                    rgb_matrix_set_color(led, 0xBF, 0x8A, 0x00);
                }
            }
        }
    }

    // layer legend: only the keys that do something on the held layer
    if (layer == 5) legend(5, 0x48, 0x00, 0x80);
    if (layer == 1 || layer == 3) legend(layer, 0x80, 0x00, 0x00);

    // splashes in the base color, amber and a third as strong in night or study
    for (uint8_t s = 0; s < SPLASH_MAX; s++) {
        splash_t *sp = &splashes[s];
        if (!sp->size) continue;

        uint16_t life    = 250 * sp->size;
        uint16_t elapsed = timer_elapsed(sp->time);
        if (elapsed >= life) {
            sp->size = 0;
            continue;
        }

        uint8_t radius = (uint32_t)reach[sp->size] * elapsed / life;
        uint8_t fade   = 255 - (uint32_t)255 * elapsed / life;

        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            int16_t dx   = g_led_config.point[i].x - sp->x;
            int16_t dy   = g_led_config.point[i].y - sp->y;
            int16_t ring = abs(sqrt16(dx * dx + dy * dy) - radius);
            if (ring >= 12) continue;

            uint8_t v   = (uint16_t)rgb_matrix_get_val() * fade / 255 * (12 - ring) / 12;
            HSV     hsv = (night || study) && !game ? (HSV){20, 230, v / 3} : (HSV){hue, 220, v};
            RGB     rgb = hsv_to_rgb(hsv);
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        }
    }
    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

// layer Mac
[0] = LAYOUT_ansi_84(
	LT(1, KC_ESC),	KC_BRID,  	KC_BRIU,  	MAC_TASK, 	MAC_SEARCH, MAC_VOICE,  MAC_DND,  	KC_MPRV,  	KC_MPLY,  	KC_MNXT, 	KC_MUTE, 	KC_VOLD, 	KC_VOLU, 	KC_PGUP,	KC_PGDN,	LT(1, KC_ESC),
	KC_GRV, 	KC_1,   	KC_2,   	KC_3,  		KC_4,   	KC_5,   	KC_6,   	KC_7,   	KC_8,   	KC_9,  		KC_0,   	KC_MINS,	KC_EQL, 				KC_BSPC,	KC_DEL,
	KC_TAB, 	KC_Q,   	KC_W,   	KC_E,  		KC_R,   	KC_T,   	KC_Y,   	KC_U,   	KC_I,   	KC_O,  		KC_P,   	KC_LBRC,	KC_RBRC, 				KC_BSLS,	KC_RALT,
	LT(5, KC_ESC),	KC_A,   	KC_S,   	KC_D,  		KC_F,   	KC_G,   	KC_H,   	KC_J,   	KC_K,   	KC_L,  		KC_SCLN,	KC_QUOT, 	 						KC_ENT,		KC_HOME,
	KC_LSFT,				KC_Z,   	KC_X,   	KC_C,  		KC_V,   	KC_B,   	KC_N,   	KC_M,   	KC_COMM,	KC_DOT,		KC_SLSH,				KC_RSFT,	KC_UP,		KC_END,
	KC_LCTL,	KC_LALT,	KC_LGUI,										KC_SPC, 							LEADER_KEY,	MO(1),   	KC_RCTL,				KC_LEFT,	KC_DOWN,    KC_RGHT),

// layer Mac Fn
[1] = LAYOUT_ansi_84(
	_______, 	KC_F1,  	KC_F2,  	KC_F3, 		KC_F4,  	KC_F5,  	KC_F6,  	KC_F7,  	KC_F8,  	KC_F9, 		KC_F10, 	KC_F11, 	KC_F12, 	MAC_PRT,	_______,	_______,
	_______, 	LNK_BLE1,  	LNK_BLE2,  	LNK_BLE3,  	LNK_RF,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 				_______,	_______,
	_______, 	_______,   	TG_WPM ,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	TG_TMR ,   	DEV_RESET,	SLEEP_MODE, 			BAT_SHOW,	_______,
	_______,	_______,   	TG_STDY,   	_______,  	_______,   	TG_GAME,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	 						_______,	_______,
	_______,				_______,   	_______,   	RGB_TEST,  	_______,   	BAT_NUM,   	TG_NGHT,	MO(4), 		RGB_SPD,	RGB_SPI,	_______,				_______,	RGB_VAI,	_______,
	_______,	_______,	_______,										_______, 							_______,	MO(1),   	_______,				RGB_MOD,	RGB_VAD,    RGB_HUI),

// layer win
[2] = LAYOUT_ansi_84(
	KC_ESC, 	KC_F1,  	KC_F2,  	KC_F3, 		KC_F4,  	KC_F5,  	KC_F6,  	KC_F7,  	KC_F8,  	KC_F9, 		KC_F10, 	KC_F11, 	KC_F12, 	KC_PSCR,	KC_INS,		KC_DEL,
	KC_GRV, 	KC_1,   	KC_2,   	KC_3,  		KC_4,   	KC_5,   	KC_6,   	KC_7,   	KC_8,   	KC_9,  		KC_0,   	KC_MINS,	KC_EQL, 				KC_BSPC,	KC_PGUP,
	KC_TAB, 	KC_Q,   	KC_W,   	KC_E,  		KC_R,   	KC_T,   	KC_Y,   	KC_U,   	KC_I,   	KC_O,  		KC_P,   	KC_LBRC,	KC_RBRC, 				KC_BSLS,	KC_PGDN,
	KC_CAPS,	KC_A,   	KC_S,   	KC_D,  		KC_F,   	KC_G,   	KC_H,   	KC_J,   	KC_K,   	KC_L,  		KC_SCLN,	KC_QUOT, 	 						KC_ENT,		KC_HOME,
	KC_LSFT,				KC_Z,   	KC_X,   	KC_C,  		KC_V,   	KC_B,   	KC_N,   	KC_M,   	KC_COMM,	KC_DOT,		KC_SLSH,				KC_RSFT,	KC_UP,		KC_END,
	KC_LCTL,	KC_LGUI,	KC_LALT,										KC_SPC, 							KC_RALT,	MO(3),   	KC_RCTL,				KC_LEFT,	KC_DOWN,    KC_RGHT),

// layer win Fn
[3] = LAYOUT_ansi_84(
	_______, 	KC_BRID,   	KC_BRIU,    _______,  	_______,   	_______,   	_______,   	KC_MPRV,   	KC_MPLY,   	KC_MNXT,  	KC_MUTE, 	KC_VOLD, 	KC_VOLU,	_______,	_______,	_______,
	_______, 	LNK_BLE1,  	LNK_BLE2,  	LNK_BLE3,  	LNK_RF,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 				_______,	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	DEV_RESET,	SLEEP_MODE, 			BAT_SHOW,	_______,
	_______,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	 						_______,	_______,
	_______,				_______,   	_______,   	RGB_TEST,  	_______,   	BAT_NUM,   	_______,	MO(4), 		RGB_SPD,	RGB_SPI,	_______,				_______,	RGB_VAI,	_______,
	_______,	_______,	_______,										_______, 							_______,	MO(3),   	_______,				RGB_MOD,	RGB_VAD,    RGB_HUI),

// layer 4
[4] = LAYOUT_ansi_84(
	_______, 	_______,  	_______,  	_______, 	_______,  	_______,  	_______,  	_______,  	_______,  	_______, 	_______, 	_______, 	_______, 	_______,	_______,	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 				_______,	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 				_______,	_______,
	_______,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,	_______, 	 						_______,	_______,
	_______,				_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	SIDE_SPD,	SIDE_SPI,	_______,				_______,	SIDE_VAI,	_______,
	_______,	_______,	_______,										_______, 							_______,	MO(4),   	_______,				SIDE_MOD,	SIDE_VAD,   SIDE_HUI),

// layer nav (hold Caps)
[5] = LAYOUT_ansi_84(
	_______, 	_______,  	_______,  	_______, 	_______,  	_______,  	_______,  	_______,  	_______,  	_______, 	_______, 	_______, 	_______, 	_______,	_______,	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 				_______,	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	KC_PGUP,   	KC_HOME,   	KC_END,  	_______,   	_______,	_______, 				_______,	_______,
	_______,	_______,   	_______,   	KC_PGDN,  	_______,   	_______,   	KC_LEFT,   	KC_DOWN,   	KC_UP,   	KC_RGHT,  	_______,	_______, 	 						_______,	_______,
	_______,				_______,   	_______,   	_______,  	_______,   	_______,   	KC_BSPC,   	KC_DEL,   	_______,	_______,	_______,				_______,	_______,	_______,
	_______,	_______,	_______,										_______, 							_______,	_______,   	_______,				_______,	_______,    _______)
};
