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

// typing: mint ripples from each keypress on a dark board
void keyboard_post_init_user(void) {
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE);
    rgb_matrix_sethsv_noeeprom(120, 220, rgb_matrix_get_val());
}

// side lights: dark while typing, violet for nav, red for Fn
void side_led_show_user(void) {
    uint8_t r = 0x00, g = 0x00, b = 0x00;

    switch (get_highest_layer(layer_state)) {
        case 5:
            r = 0x90; g = 0x00; b = 0xFF;
            break;
        case 1:
        case 3:
            r = 0xFF; g = 0x00; b = 0x00;
            break;
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
    if (!record->event.pressed) return true;

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

bool rgb_matrix_indicators_user(void) {
    static const uint8_t reach[] = {0, 25, 60, 160};
    static const uint8_t wasd[][2] = {{2, 2}, {3, 1}, {3, 2}, {3, 3}};

    // WASD: low red so they're findable in the dark, while Fn+\ battery display is on
    for (uint8_t k = 0; f_bat_hold && k < 4; k++) {
        uint8_t led = g_led_config.matrix_co[wasd[k][0]][wasd[k][1]];
        if (led != NO_LED) rgb_matrix_set_color(led, 0x30, 0x00, 0x00);
    }

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

            HSV hsv = {120, 220, (uint16_t)rgb_matrix_get_val() * fade / 255 * (12 - ring) / 12};
            RGB rgb = hsv_to_rgb(hsv);
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
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	DEV_RESET,	SLEEP_MODE, 			BAT_SHOW,	_______,
	_______,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	 						_______,	_______,
	_______,				_______,   	_______,   	RGB_TEST,  	_______,   	BAT_NUM,   	_______,	MO(4), 		RGB_SPD,	RGB_SPI,	_______,				_______,	RGB_VAI,	_______,
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
