/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common.h"

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef void (*print_function_t)(uint8_t port, uint8_t socket, uint8_t line);

#define J15 (1 << 31)
#define J14 (1 << 30)
#define J13 (1 << 29)
#define J12 (1 << 28)
#define J11 (1 << 27)
#define J10 (1 << 26)
#define J9 (1 << 25)
#define J8 (1 << 24)
#define J7 (1 << 23)
#define J6 (1 << 22)
#define J5 (1 << 21)
#define J4 (1 << 20)
#define J3 (1 << 19)
#define J2 (1 << 18)
#define J1 (1 << 17)
#define J0 (1 << 16)
#define B3 (1 << 3)
#define B2 (1 << 2)
#define B1 (1 << 1)
#define B0 (1 << 0)

#define MAX_PORT 2
#define MAX_SOCKET 4
#define MAX_ROW 4
#define MAX_BANK 4
#define STDPAD_BTNS_CNT 24
#define SIXDPAD_BTNS_CNT 7
#define ANALOG_BTNS_CNT 8

//#define TEST_MODE

enum {
    NONE = 0,
    STDPAD,
    ROTARY,
    BANKED,
    HEAD,
    KBM,
    SIXDPAD,
    ANALOG,
    ERR_NO_BANK,
    ERR_BK_UKN,
    ERR_INVALID,
    CTRL_MAX,
};

static void print_none(uint8_t port, uint8_t socket, uint8_t line);
static void print_stdpad_btns(uint8_t port, uint8_t socket, uint8_t line);
static void print_6dpad_btns(uint8_t port, uint8_t socket, uint8_t line);

static const char *dev_name[CTRL_MAX] = {
    "NONE",
    "STDPAD",
    "ROTARY",
    "BANKED",
    "HEAD",
    "KBM",
    "SIXDPAD",
    "ANALOG",
    "ERR_NO_BANK",
    "ERR_BK_UKN",
    "ERR_INVALID",
};
static const print_function_t print_function[CTRL_MAX] = {
    print_none,
    print_stdpad_btns,
    print_none,
    print_none,
    print_none,
    print_none,
    print_6dpad_btns,
    print_none,
    print_none,
    print_none,
    print_none,
};
static const uint8_t axes_shift[MAX_PORT] = {24, 28};
static const uint32_t cbits_mask[MAX_PORT] = {B0, B2};
static const uint32_t bbits_mask[MAX_PORT] = {B1, B3};
static const char btns[STDPAD_BTNS_CNT] = {
    '^', 'v', '<', '>', 'P', 'O', 'C', 'B', 'A', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#'
};
static const uint8_t btns_row[STDPAD_BTNS_CNT] = {
    0, 0, 0, 0, 0, 3, 2, 1, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3
};
static const uint32_t btns_mask[MAX_PORT][STDPAD_BTNS_CNT] = {
    {
        J8, J9, J10, J11, B0, B1, B1, B1, B1, J11, J11, J11, J10, J10, J10, J9, J9, J9, J8, J8, J8
    },
    {
        J12, J13, J14, J15, B2, B3, B3, B3, B3, J15, J15, J15, J14, J14, J14, J13, J13, J13, J12, J12, J12
    },
};
static const char btns_6d[SIXDPAD_BTNS_CNT] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G'
};
static const uint8_t btns_row_6d[SIXDPAD_BTNS_CNT] = {
    0, 1, 2, 3, 3, 2, 1
};
static const uint8_t btns_bank_6d[SIXDPAD_BTNS_CNT] = {
    0, 0, 0, 0, 1, 1, 1
};
static const char btns_analog[ANALOG_BTNS_CNT] = {
    '^', 'v', '<', '>', 'A', 'B', 'C', 'D'
};
static const uint8_t btns_row_analog[ANALOG_BTNS_CNT] = {
    0, 0, 0, 0, 0, 1, 2, 3
};
static const uint8_t btns_bank_analog[ANALOG_BTNS_CNT] = {
    1, 1, 1, 1, 0, 0, 0, 0
};
static const uint32_t btns_mask_analog[MAX_PORT][ANALOG_BTNS_CNT] = {
    {
        J8, J9, J10, J11, B1, B1, B1, B1
    },
    {
        J12, J13, J14, J15, B3, B3, B3, B3
    },
};
static const uint8_t sockets_row_codes[MAX_SOCKET][MAX_ROW] = {
    {0x7E, 0xBD, 0xDB, 0xE7},
    {0x00, 0x81, 0x42, 0xC3},
    {0x24, 0xA5, 0x66, 0x18},
    {0x99, 0x5A, 0x3C, 0xFF},
};

static volatile uint32_t *get_joy = (uint32_t *)JOYSTICK;
static volatile uint16_t *set_joy = (uint16_t *)JOYSTICK;

static uint16_t cnt = 0;
static uint8_t line;

static uint8_t dev_type[MAX_PORT][MAX_SOCKET];
static uint32_t rows[MAX_BANK][MAX_SOCKET][MAX_ROW];

static void get_socket_rows(uint8_t socket, uint8_t dst_bank) {
    for (uint8_t i = 0; i < 4; i++) {
        *set_joy = 0x8000 | sockets_row_codes[socket][i];
        rows[dst_bank][socket][i] = *get_joy;
    }
}

static uint8_t get_banked_rows(uint8_t port, uint8_t socket) {
    uint8_t bank_cnt = 0;
    do {
        get_socket_rows(socket, 0);
        if (++bank_cnt > 10) {
            return 0;
        }
    } while (rows[0][socket][0] & cbits_mask[port]);

    for (bank_cnt = 1; bank_cnt < MAX_BANK; bank_cnt++) {
        get_socket_rows(socket, bank_cnt);
        if (!(rows[bank_cnt][socket][0] & cbits_mask[port])) {
            break;
        }
    }
    return bank_cnt;
}

static uint8_t get_banked_type(uint8_t port, uint8_t socket) {
    uint8_t type = 0;
    uint8_t bank_cnt = get_banked_rows(port, socket);

    if (bank_cnt == 0) {
        return ERR_NO_BANK;
    }

    for (uint8_t i = 0; i < MAX_ROW; i++, type <<= 1) {
        type |= (rows[bank_cnt - 1][socket][MAX_ROW - i] & bbits_mask[port]) ? 0x01 : 0x00;
    }

    switch (type & 0xF) {
        case 0x7:
            return HEAD;
        case 0xD:
            return KBM;
        case 0xE:
            return SIXDPAD;
        case 0xF:
            return ANALOG;
    }
    return ERR_BK_UKN;
}

static uint8_t get_basic_type(uint8_t port, uint8_t socket) {
    if ((rows[0][socket][2] & cbits_mask[port]) && (rows[0][socket][3] & cbits_mask[port])) {
        return STDPAD;
    }
    else if ((rows[0][socket][2] & cbits_mask[port]) && !(rows[0][socket][3] & cbits_mask[port])) {
        return ROTARY;
    }
    else if (!(rows[0][socket][2] & cbits_mask[port]) && (rows[0][socket][3] & cbits_mask[port])) {
        return BANKED;
    }
    return ERR_INVALID;
}

static void detect_ctrl(void) {
    for (uint8_t i = 0; i < MAX_PORT; i++) {
        if (!(rows[0][3][1] & cbits_mask[i])) {
            for (uint8_t j = 0; j < MAX_SOCKET; j++) {
                dev_type[i][j] = get_basic_type(i, j);
                if (dev_type[i][j] == BANKED) {
                    dev_type[i][j] = get_banked_type(i, j);
                }
            }
        }
        else {
            dev_type[i][0] = STDPAD;
            dev_type[i][2] = NONE;
            dev_type[i][3] = NONE;

            /* Check banked ctrl on socket 1 */
            dev_type[i][1] = get_basic_type(i, 1);
            if (dev_type[i][1] == BANKED) {
                dev_type[i][1] = get_banked_type(i, 1);
            }
            else {
                dev_type[i][1] = NONE;
            }
        }
    }
}

static void print_none(uint8_t port, uint8_t socket, uint8_t line) {
    rapLocate(144, line - 10);
    js_r_textbuffer = (char *)ee_printf("%20s", "");
    rapPrint();
    rapLocate(48, line);
    js_r_textbuffer = (char *)ee_printf("%32s", "");
    rapPrint();
}

static void print_stdpad_btns(uint8_t port, uint8_t socket, uint8_t line) {
    for (uint8_t i = 0; i < sizeof(btns); i++) {
        if (rows[0][socket][btns_row[i]] & btns_mask[port][i]) {
            jsfSetFontIndx(0);
        }
        else {
            jsfSetFontIndx(1);
        }
        rapLocate(48 + i * 12, line);
        js_r_textbuffer = (char *)ee_printf("%c", btns[i]);
        rapPrint();
    }
}

static void print_6dpad_btns(uint8_t port, uint8_t socket, uint8_t line) {
    get_banked_rows(port, socket);

    int8_t x = ((rows[0][socket][3] >> (axes_shift[port] - 4)) & 0xF0) |
               ((rows[0][socket][0] >> (axes_shift[port])) & 0x0F);
    int8_t y = ((rows[1][socket][3] >> (axes_shift[port] - 4)) & 0xF0) |
               ((rows[0][socket][1] >> (axes_shift[port])) & 0x0F);
    uint8_t z = ((rows[2][socket][3] >> (axes_shift[port] - 4)) & 0xF0) |
               ((rows[0][socket][2] >> (axes_shift[port])) & 0x0F);
    int8_t tx = ((rows[2][socket][0] >> (axes_shift[port] - 4)) & 0xF0) |
               ((rows[1][socket][0] >> (axes_shift[port])) & 0x0F);
    int8_t ty = ((rows[2][socket][1] >> (axes_shift[port] - 4)) & 0xF0) |
               ((rows[1][socket][1] >> (axes_shift[port])) & 0x0F);
    uint8_t tz = ((rows[2][socket][2] >> (axes_shift[port] - 4)) & 0xF0) |
               ((rows[1][socket][2] >> (axes_shift[port])) & 0x0F);
    rapLocate(144, line - 10);
    js_r_textbuffer = (char *)ee_printf("X%+4d", x);
    rapPrint();
    rapLocate(192, line - 10);
    js_r_textbuffer = (char *)ee_printf("TX%+4d", tx);
    rapPrint();
    rapLocate(248, line - 10);
    js_r_textbuffer = (char *)ee_printf(" Z%+4u", z);
    rapPrint();
    for (uint8_t i = 0; i < sizeof(btns_6d); i++) {
        if (rows[btns_bank_6d[i]][socket][btns_row_6d[i]] & bbits_mask[port]) {
            jsfSetFontIndx(0);
        }
        else {
            jsfSetFontIndx(1);
        }
        rapLocate(48 + i * 12, line);
        js_r_textbuffer = (char *)ee_printf("%c", btns_6d[i]);
        rapPrint();
    }
    jsfSetFontIndx(0);
    rapLocate(144, line);
    js_r_textbuffer = (char *)ee_printf("Y%+4d", y);
    rapPrint();
    rapLocate(192, line);
    js_r_textbuffer = (char *)ee_printf("TY%+4d", ty);
    rapPrint();
    rapLocate(248, line);
    js_r_textbuffer = (char *)ee_printf("TZ%+4u", tz);
    rapPrint();
}

static void print_socket(uint8_t port, uint8_t socket, uint8_t *line) {
    jsfSetFontIndx(0);
    rapLocate(16, *line += 10);
    js_r_textbuffer = (char *)ee_printf("S%d: %-11s", socket, dev_name[dev_type[port][socket]]);
    rapPrint();
    if (print_function[dev_type[port][socket]]) {
        print_function[dev_type[port][socket]](port, socket, *line += 10);
    }
}

static void print_port(uint8_t port, uint8_t *line) {
    jsfSetFontIndx(0);
    rapLocate(10, *line);
    js_r_textbuffer = (char *)ee_printf("P%d:", port);
    rapPrint();

    for (uint8_t i = 0; i < MAX_SOCKET; i++) {
        print_socket(port, i, line);
    }
}

void basicmain(void) {
    jsfSetFontSize(0);
    jsfSetFontIndx(0);

    rapLocate(10, 10);
    js_r_textbuffer = (char *)"BlueRetro Test";
    rapPrint();

    while (1) {
        line = 30;

        jsfVsync(0);

        for (uint8_t i = 0; i < MAX_SOCKET; i++) {
            get_socket_rows(i, 0);
        }

#ifdef TEST_MODE
        for (uint8_t i = 0; i < MAX_SOCKET; i++) {
            dev_type[0][i] = (cnt >> (3 + i)) & 0x7;
            dev_type[1][i] = ~(cnt >> (3 + i)) & 0x7;
        }
#else
        detect_ctrl();
#endif

        for (uint8_t i = 0; i < MAX_PORT; i++)  {
            print_port(i, &line);
            line += 20;
        }

        jsfSetFontIndx(0);
        rapLocate(176, 10);
        js_r_textbuffer = (char *)ee_printf("FRAMECNT: %5u", cnt++);
        rapPrint();
    }
}
