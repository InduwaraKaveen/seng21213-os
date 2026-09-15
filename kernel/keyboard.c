/* =============================================================================
 * SENG21213-OS :: PS/2 Keyboard Driver Implementation
 * File   : kernel/keyboard.c
 * ============================================================================*/
#include "keyboard.h"
#include "vga.h"
#include "../include/types.h"

/* I/O ports */
#define KB_DATA_PORT   0x60    /* Read scan code / write command */
#define KB_STATUS_PORT 0x64    /* Read status / write command */
#define KB_STATUS_OBF  0x01    /* Output Buffer Full bit */

/* Inline port I/O */
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* ---------------------------------------------------------------------------
 * Scancode Set 1 → ASCII translation table (unshifted)
 * Index = scancode. 0 = non-printable / not mapped.
 * --------------------------------------------------------------------------*/
static const char sc_ascii[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',
    0,   '*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0,  /* F1-F10 */
    0, 0,                  /* NumLock, ScrollLock */
    '7','8','9','-','4','5','6','+','1','2','3','0','.', /* numpad */
    0,0,0,                 /* filler */
    0,0                    /* F11,F12 */
};

static const char sc_ascii_shift[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?',
    0,   '*', 0, ' ', 0,
};

static bool shift_held = false;

#define KB_HISTORY_SIZE 20
#define KB_HISTORY_LINE_SIZE KB_BUF_SIZE

static char kb_history[KB_HISTORY_SIZE][KB_HISTORY_LINE_SIZE];
static int kb_history_count = 0;
static int kb_history_next = 0;

static void kb_history_add(const char *line)
{
    if (!line || line[0] == '\0') return;

    if (kb_history_count > 0) {
        int last = (kb_history_next + KB_HISTORY_SIZE - 1) % KB_HISTORY_SIZE;
        int same = 1;

        for (int i = 0; i < KB_HISTORY_LINE_SIZE; i++) {
            if (kb_history[last][i] != line[i]) {
                same = 0;
                break;
            }
            if (line[i] == '\0') break;
        }

        if (same) return;
    }

    int i = 0;
    while (i < KB_HISTORY_LINE_SIZE - 1 && line[i] != '\0') {
        kb_history[kb_history_next][i] = line[i];
        i++;
    }
    kb_history[kb_history_next][i] = '\0';

    kb_history_next = (kb_history_next + 1) % KB_HISTORY_SIZE;

    if (kb_history_count < KB_HISTORY_SIZE) {
        kb_history_count++;
    }
}

static const char *kb_history_get(int offset)
{
    if (offset < 0 || offset >= kb_history_count) return 0;

    int index = kb_history_next - 1 - offset;
    while (index < 0) index += KB_HISTORY_SIZE;

    return kb_history[index];
}

static void kb_clear_line(char *buf, int *len)
{
    while (*len > 0) {
        vga_putchar('\b');
        (*len)--;
    }

    buf[0] = '\0';
}

void kb_init(void) {
    /* Flush any stale data in the keyboard buffer */
    while (inb(KB_STATUS_PORT) & KB_STATUS_OBF) {
        inb(KB_DATA_PORT);
    }
}

int kb_getchar(void) {
    uint8_t sc;
    while (true) {
        /* Wait until output buffer is full (key available) */
        while (!(inb(KB_STATUS_PORT) & KB_STATUS_OBF));
        sc = inb(KB_DATA_PORT);

        if (sc == 0xE0) {
            while (!(inb(KB_STATUS_PORT) & KB_STATUS_OBF));
            uint8_t extended = inb(KB_DATA_PORT);

            if (extended == 0x48) return KB_KEY_UP;
            if (extended == 0x50) return KB_KEY_DOWN;
            continue;
        }

        if (sc & 0x80) {
            /* Key release: bit 7 set, clear modifier state */
            uint8_t release = sc & 0x7F;
            if (release == 0x2A || release == 0x36) shift_held = false;
            continue;
        }

        /* Key press */
        if (sc == 0x2A || sc == 0x36) { shift_held = true; continue; }

        /* Caps lock / ctrl / alt – ignored in Stage 0 */

        char c = shift_held ? sc_ascii_shift[sc] : sc_ascii[sc];
        if (c) return c;
    }
}

int kb_readline(char *buf, int len, kb_completion_fn completion) {
    int i = 0;
    int history_offset = -1;

    while (i < len - 1) {
        int c = kb_getchar();

        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            buf[i] = '\0';
            kb_history_add(buf);
            break;
        }

        if (c == '\b') {
            if (i > 0) {
                i--;
                buf[i] = '\0';
                vga_putchar('\b');
            }
            continue;
        }

        if (c == KB_KEY_UP) {
            if (kb_history_count > 0) {
                if (history_offset < kb_history_count - 1) {
                    history_offset++;
                }

                const char *history_line = kb_history_get(history_offset);

                if (history_line) {
                    kb_clear_line(buf, &i);

                    int history_len = 0;
                    while (history_line[history_len] &&
                           history_len < len - 1) {
                        history_len++;
                    }

                    for (int j = 0; j < history_len; j++) {
                        buf[j] = history_line[j];
                        vga_putchar(history_line[j]);
                    }

                    i = history_len;
                    buf[i] = '\0';
                }
            }
            continue;
        }

        if (c == KB_KEY_DOWN) {
            if (history_offset >= 0) {
                if (history_offset > 0) {
                    history_offset--;
                } else {
                    history_offset = -1;
                }

                kb_clear_line(buf, &i);

                if (history_offset >= 0) {
                    const char *history_line = kb_history_get(history_offset);

                    if (history_line) {
                        int history_len = 0;
                        while (history_line[history_len] &&
                               history_len < len - 1) {
                            history_len++;
                        }

                        for (int j = 0; j < history_len; j++) {
                            buf[j] = history_line[j];
                            vga_putchar(history_line[j]);
                        }

                        i = history_len;
                        buf[i] = '\0';
                    }
                }
            }
            continue;
        }

        if (c == KB_KEY_TAB) {
            if (completion) {
                const char *match = completion(buf, i);

                if (match) {
                    int match_len = 0;
                    while (match[match_len] && match_len < len - 1) {
                        match_len++;
                    }

                    while (i > 0) {
                        vga_putchar('\b');
                        i--;
                    }

                    for (int j = 0; j < match_len; j++) {
                        buf[j] = match[j];
                        vga_putchar(match[j]);
                    }

                    i = match_len;
                    buf[i] = '\0';
                }
            }
            continue;
        }

        if (i < len - 1) {
            buf[i++] = c;
            buf[i] = '\0';
            vga_putchar(c);
        }

        history_offset = -1;
    }

    buf[i] = '\0';
    return i;
}
