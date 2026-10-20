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

void set_left_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_right_rgb(uint8_t r, uint8_t g, uint8_t b);
extern bool f_bat_hold;

extern DEV_INFO_STRUCT dev_info;

enum user_keycodes {
    TG_GAME = QK_USER_0, // Fn+G: off -> Factorio -> Valheim -> off
    TG_NGHT,             // Fn+N: night base
    TG_WPM,              // Fn+W: WPM heat
};

enum { GAME_OFF, GAME_FACTORIO, GAME_VALHEIM };

static uint8_t  game;
static bool     night, wpm_heat, wispr_sweep;
static uint16_t mod_time, wispr_time;

// typing: mint ripples from each keypress on a dark board
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
    } else if (layer == 5) {
        r = 0x90; g = 0x00; b = 0xFF;
    } else if (layer == 1 || layer == 3) {
        r = 0xFF; g = 0x00; b = 0x00;
    } else if (wpm_heat && !game) {
        uint8_t heat = wpm_heat_level();
        RGB     c    = hsv_to_rgb((HSV){wpm_hue(heat), 255, 64 + scale8(191, heat)});
        r = c.r; g = c.g; b = c.b;
    } else if (game == GAME_FACTORIO) {
        r = 0x80; g = 0x3D; b = 0x00;
    } else if (game == GAME_VALHEIM) {
        r = 0x73; g = 0x09; b = 0x42;
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

    switch (keycode) {
        case TG_GAME:
            game = (game + 1) % 3;
            return false;
        case TG_NGHT:
            night = !night;
            return false;
        case TG_WPM:
            wpm_heat = !wpm_heat;
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
    static uint8_t       hue     = 120;
    uint8_t              mods    = get_mods();
    uint8_t              layer   = get_highest_layer(layer_state);

    // WPM heat retints the ripples
    uint8_t want = (wpm_heat && !game) ? wpm_hue(wpm_heat_level()) : 120;
    if (want != hue) {
        hue = want;
        rgb_matrix_sethsv_noeeprom(hue, 220, rgb_matrix_get_val());
    }

    // base: games and night replace the ripples
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
    } else if (night) {
        static const uint16_t homing[] = {KC_F, KC_J};
        rgb_matrix_set_color_all(0x0D, 0x06, 0x02);
        PAINT(homing, 0x33, 0x18, 0x08);
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

    // layer legend: only the keys that do something on the held layer
    if (layer == 5) legend(5, 0x48, 0x00, 0x80);
    if (layer == 1 || layer == 3) legend(layer, 0x80, 0x00, 0x00);

    // splashes, amber and a third as strong at night
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
            HSV     hsv = night && !game ? (HSV){20, 230, v / 3} : (HSV){hue, 220, v};
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
	KC_LCTL,	KC_LALT,	KC_LGUI,										KC_SPC, 							KC_RGUI,	MO(1),   	KC_RCTL,				KC_LEFT,	KC_DOWN,    KC_RGHT),

// layer Mac Fn
[1] = LAYOUT_ansi_84(
	_______, 	KC_F1,  	KC_F2,  	KC_F3, 		KC_F4,  	KC_F5,  	KC_F6,  	KC_F7,  	KC_F8,  	KC_F9, 		KC_F10, 	KC_F11, 	KC_F12, 	MAC_PRT,	_______,	_______,
	_______, 	LNK_BLE1,  	LNK_BLE2,  	LNK_BLE3,  	LNK_RF,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 				_______,	_______,
	_______, 	_______,   	TG_WPM ,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	DEV_RESET,	SLEEP_MODE, 			BAT_SHOW,	_______,
	_______,	_______,   	_______,   	_______,  	_______,   	TG_GAME,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	 						_______,	_______,
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
