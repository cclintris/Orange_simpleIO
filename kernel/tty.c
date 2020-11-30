
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               tty.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

#define TTY_FIRST    (tty_table)
#define TTY_END        (tty_table + NR_CONSOLES)

PRIVATE void init_tty(TTY *p_tty);

PRIVATE void tty_do_read(TTY *p_tty);

PRIVATE void tty_do_write(TTY *p_tty);

PRIVATE void put_key(TTY *p_tty, u32 key);

u32 inputBuffer[256];
int inputBufferIndex = -1;
int inputCharPos[256];
int inputCharPosIndex = -1;

/*======================================================================*
                           task_tty
 *======================================================================*/
PUBLIC void task_tty() {
    TTY *p_tty = TTY_FIRST;

    init_keyboard();
    init_tty(p_tty);
    select_console(0);

    int clock = get_ticks();
    int time = 8000; // 20s
    escMode = 0;

    // 1234
    // [word][color]

    while (1) {
        if(escMode) {
            clock = get_ticks();
        } else{
            // clear screen every 20s
            if (get_ticks() - clock > time) {
                // disp_int(clock);
                clear_screen(V_MEM_BASE + p_tty->p_console->cursor*2, V_MEM_BASE + p_tty->p_console->original_addr);
                p_tty->p_console->cursor = p_tty->p_console->original_addr;
                clock = get_ticks();
            }
        }
        tty_do_read(p_tty);
        // keyboard_read()
        // in_process() -> put_key()
        tty_do_write(p_tty);
    }
}

/*======================================================================*
			   init_tty
 *======================================================================*/
PRIVATE void init_tty(TTY *p_tty) {
    p_tty->inbuf_count = 0;
    p_tty->p_inbuf_head = p_tty->p_inbuf_tail = p_tty->in_buf;

    init_screen(p_tty);
}

/*======================================================================*
				in_process
 *======================================================================*/
PUBLIC void in_process(TTY *p_tty, u32 key) {
    char output[2] = {'\0', '\0'};

    if(escFinal && (key & MASK_RAW) != ESC) {
        return;
    }

    if (!(key & FLAG_EXT)) {
        // if((/*is Z*/(key & MASK_RAW) == 'z' || (key & MASK_RAW) == 'Z') &&
        //     (/*FLAG== Ctrl*/(key & FLAG_CTRL_L) || (key & FLAG_CTRL_R))){
            // disp_str("<ctrlz>");
            // inputBufferIndex--;
        // }else{
            put_key(p_tty, key);
            inputBuffer[++inputBufferIndex] = key;
            inputCharPos[++inputCharPosIndex] = p_tty->p_console->cursor;
        // }
    } else {
        int raw_code = key & MASK_RAW;
        switch (raw_code) {
            case ENTER:
                put_key(p_tty, '\n');
                inputBuffer[++inputBufferIndex] = '\n';
                inputCharPos[++inputCharPosIndex] = p_tty->p_console->cursor;
                break;
            case BACKSPACE:
                put_key(p_tty, '\b');
                inputBuffer[++inputBufferIndex] = '\b';
                inputCharPos[++inputCharPosIndex] = p_tty->p_console->cursor;
                break;
            case TAB:
                put_key(p_tty, '\t');
                inputBuffer[++inputBufferIndex] = '\t';
                inputCharPos[++inputCharPosIndex] = p_tty->p_console->cursor;
                break;
            case ESC:
                // change mode
                if(escMode == 0) {
                    escPos = p_tty->p_console->cursor;
                    escFinal = 0;
                    escTop = inputBufferIndex;
                }else{
                    for(u8* i = V_MEM_BASE + p_tty->p_console->cursor*2; i >= V_MEM_BASE + escPos*2; i-=2) {
                        *(i) = ' ';
                        *(i+1) = DEFAULT_CHAR_COLOR;
                    }
                    for(u8* i = V_MEM_BASE + escPos*2 - 2; i >= V_MEM_BASE + p_tty->p_console->original_addr; i-=2) {
                        *(i+1) = DEFAULT_CHAR_COLOR;
                    }
                    p_tty->p_console->cursor = escPos;
                    set_cursor(escPos);
                    escFinal = 0;
                    inputBufferIndex = escTop;
                    inputCharPosIndex = escTop;
                }
                escMode = !escMode;
                break;
            case UP:
                if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
                    scroll_screen(p_tty->p_console, SCR_DN);
                }
                break;
            case DOWN:
                if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
                    scroll_screen(p_tty->p_console, SCR_UP);
                }
                break;
            case F1:
            case F2:
            case F3:
            case F4:
            case F5:
            case F6:
            case F7:
            case F8:
            case F9:
            case F10:
            case F11:
            case F12:
                /* Alt + F1~F12 */
                if ((key & FLAG_ALT_L) || (key & FLAG_ALT_R)) {
                    select_console(raw_code - F1);
                }
                break;
            default:
                break;
        }
    }
}

/*======================================================================*
			      put_key
*======================================================================*/
PRIVATE void put_key(TTY *p_tty, u32 key) {
    if (p_tty->inbuf_count < TTY_IN_BYTES) {
        *(p_tty->p_inbuf_head) = key;
        p_tty->p_inbuf_head++;
        if (p_tty->p_inbuf_head == p_tty->in_buf + TTY_IN_BYTES) {
            p_tty->p_inbuf_head = p_tty->in_buf;
        }
        p_tty->inbuf_count++;
    }
}


/*======================================================================*
			      tty_do_read
 *======================================================================*/
PRIVATE void tty_do_read(TTY *p_tty) {
    if (is_current_console(p_tty->p_console)) {
        keyboard_read(p_tty);
    }
}


/*======================================================================*
			      tty_do_write
 *======================================================================*/
PRIVATE void tty_do_write(TTY *p_tty) {
    if (p_tty->inbuf_count) {
        char ch = *(p_tty->p_inbuf_tail);
        p_tty->p_inbuf_tail++;
        if (p_tty->p_inbuf_tail == p_tty->in_buf + TTY_IN_BYTES) {
            p_tty->p_inbuf_tail = p_tty->in_buf;
        }
        p_tty->inbuf_count--;

        out_char(p_tty->p_console, ch);
    }
}

/*======================================================================*
                              tty_write
*======================================================================*/
PUBLIC void tty_write(TTY *p_tty, char *buf, int len) {
    char *p = buf;
    int i = len;

    while (i) {
        out_char(p_tty->p_console, *p++);
        i--;
    }
}

/*======================================================================*
                              sys_write
*======================================================================*/
PUBLIC int sys_write(char *buf, int len, PROCESS *p_proc) {
    tty_write(&tty_table[p_proc->nr_tty], buf, len);
    return 0;
}

