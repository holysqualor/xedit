#include <stdio.h>
#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include "slpool.h"

#define LINE_INIT_CAP 32

typedef struct {
        char *buffer;
        int length, capacity;
} line_s;

static sl_pool lines;
static sl_id offset, cursor;
static int width = 0, height = 0;
static int crx = 0, vrx = 0, tarx = 0, cry = 0, line_number = 1;
static line_s *curline;

static const int tab = 4;

void update_vrx() {
        vrx = 0;
        for(int i = 0; i < crx; i++)
                vrx += curline->buffer[i] == '\t' ? tab - (vrx % tab) : 1;
        tarx = vrx;
}

void update_crx() {
        crx = 0;
        vrx = 0;
        for(int i ;; crx++, vrx += i) {
                i = curline->buffer[crx] == '\t' ? tab - (vrx % tab) : 1;
                if(crx == curline->length || vrx + i > tarx)
                        return;
        }
}

void cursor_left() {
        if(crx) {
                crx--;
                update_vrx();
        }
}

void cursor_right() {
        if(crx < curline->length) {
                crx++;
                update_vrx();
        }
}

void cursor_up() {
        if(cursor == sl_first(lines))
                return;
        if(cry == 0) {
                offset = sl_prev(lines, offset);
                line_number--;
        } else cry--;
        cursor = sl_prev(lines, cursor);
        curline = sl_get(lines, cursor);
        update_crx();
}

void cursor_down() {
        if(cursor == sl_last(lines))
                return;
        if(cry == height - 1) {
                offset = sl_next(lines, offset);
                line_number++;
        } else cry++;
        cursor = sl_next(lines, cursor);
        curline = sl_get(lines, cursor);
        update_crx();
}

int update_size(line_s *l, int length, int is_new) {
        if(!is_new && length < l->capacity) {
                l->length = length;
                return 1;
        }
        int capacity = length < LINE_INIT_CAP ? LINE_INIT_CAP : length * 2;
        char *buffer = is_new || !l->capacity ? malloc(capacity) : realloc(l->buffer, capacity);
        if(!buffer)
                return 0;
        if(!is_new && !l->capacity && l->length)
                memcpy(buffer, l->buffer, l->length);
        l->capacity = capacity;
        l->buffer = buffer;
        l->length = length;
        return 1;
}

void remove_symbol() {
        if(crx) {
                cursor_left();
                curline->length--;
                for(int i = crx; i < curline->length; i++)
                        curline->buffer[i] = curline->buffer[i + 1];
        } else {
                sl_id prev = sl_prev(lines, cursor);
                if(prev == nullid)
                        return;
                line_s *l = sl_get(lines, prev);
                int length = l->length;
                if(!update_size(l, curline->length + length, 0))
                        return;
                if(curline->length)
                        memcpy(l->buffer + length, curline->buffer, curline->length);
                curline->length = crx;
                crx = length;
                if(offset == cursor) {
                        offset = prev;
                        line_number--;
                } else cry--;
                sl_remove(lines, cursor);
                cursor = prev;
                curline = l;
                update_vrx();
        }
}

void add_symbol(int c) {
        if(c == '\n') {
                sl_id id = sl_add(lines, NULL, sl_next(lines, cursor));
                if(id == nullid)
                        return;
                line_s *l = sl_get(lines, id);
                if(!update_size(l, curline->length - crx, sl_exp(lines)))
                        return;
                memcpy(l->buffer, curline->buffer + crx, l->length);
                curline->length = crx;
                cursor = id;
                if(cry == height - 1) {
                        offset = sl_next(lines, offset);
                        line_number++;
                } else cry++;
                curline = l;
                crx = 0;
                tarx = 0;
                vrx = 0;
        } else if(isprint(c) || c == '\t') {
                if(!update_size(curline, curline->length + 1, 0))
                        return;
                for(int i = curline->length; i > crx; i--)
                        curline->buffer[i] = curline->buffer[i - 1];
                curline->buffer[crx] = c;
                cursor_right();
        }
}

void update_screen() {
        static char num[5];
        erase();
        sl_id line = offset;
        attron(COLOR_PAIR(1));
        for(int i = 0; i < height && line != nullid; i++, line = sl_next(lines, line)) {
                sprintf(num, "%3d ", line_number + i);
                mvaddstr(i, 0, num);
                line_s *l = sl_get(lines, line);
                for(int j = 0, v = 4; v < width && j < l->length; j++) {
                        if(l->buffer[j] == '\t') {
                                for(int k = 0, l = tab - (v % tab); k < l; k++)
                                        mvaddch(i, v++, ' ');
                        } else mvaddch(i, v++, l->buffer[j]);
                }
        }
        attroff(COLOR_PAIR(1));
        attron(COLOR_PAIR(2));
        mvaddch(cry, vrx + 4, crx == curline->length || curline->buffer[crx] == '\t' ? ' ' : curline->buffer[crx]);
        attroff(COLOR_PAIR(2));
        refresh();
}

void xedit_start() {
        int key;
        offset = sl_first(lines);
        cursor = offset;
        curline = sl_get(lines, cursor);
        while((key = getch()) != KEY_END) {
                update_screen();
                switch(key) {
                        case KEY_LEFT: cursor_left();
                                break;
                        case KEY_RIGHT: cursor_right();
                                break;
                        case KEY_UP: cursor_up();
                                break;
                        case KEY_DOWN: cursor_down();
                                break;
                        case '\b': remove_symbol();
                                break;
                        default: add_symbol(key);
                }
                usleep(10000);
        }
}

void xedit_end(const char *filename) {
        attron(COLOR_PAIR(2));
        for(int i = 0; i < width; i++)
                mvaddch(height - 1, i, ' ');
        mvaddstr(height - 1, 0, "Save file? Y/n");
        attroff(COLOR_PAIR(2));
        refresh();
        int c;
        do {
                c = getch();
        } while(c != 'y' && c != 'Y' && c != 'n' && c != 'N');
        if(c == 'n' || c == 'N')
                return;
        FILE *file = fopen(filename, "w");
        for(sl_id line = sl_first(lines); line != nullid; line = sl_next(lines, line)) {
                line_s *l = sl_get(lines, line);
                if(l->length)
                        fwrite(l->buffer, 1, l->length, file);
                fputc('\n', file);
        }
        fclose(file);
}

void destructor(void *line) {
        line_s *l = line;
        if(l->capacity)
                free(l->buffer);
}

int parse(char *str, line_s *l) {
        static char *s = NULL, *p = NULL;
        if(s != str) {
                s = str;
                p = str;
        }
        if(!p || !*p)
                return 0;
        l->buffer = p;
        l->length = 0;
        while(*p && *p != '\n') {
                p++;
                l->length++;
        }
        if(*p)
                p++;
        return 1;
}

int main(int argc, char **argv) {
        if(argc != 2)
                return 1;
        if(!(lines = sl_create(sizeof(line_s))))
                return 1;
        FILE *file = fopen(argv[1], "r");
        char *buffer = NULL;
        if(file) {
                fseek(file, 0L, SEEK_END);
                size_t size = ftell(file);
                if(size) {
                        rewind(file);
                        if(!(buffer = malloc(size + 1)) || fread(buffer, 1, size, file) != size) {
                                if(buffer)
                                        free(buffer);
                                sl_destroy(lines, NULL);
                                fclose(file);
                                return 1;
                        }
                        buffer[size] = 0;
                        line_s l = {};
                        while(parse(buffer, &l))
                                sl_push_back(lines, &l);
                }
                fclose(file);
        }
        if(sl_is_empty(lines) && !update_size(sl_get(lines, sl_push_back(lines, NULL)), 0, 1)) {
                sl_destroy(lines, destructor);
                free(buffer);
                return 1;
        }
        initscr();
        noecho();
        curs_set(FALSE);
        nodelay(stdscr, TRUE);
        keypad(stdscr, TRUE);
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_WHITE);
        getmaxyx(stdscr, height, width);
        xedit_start();
        xedit_end(argv[1]);
        endwin();
        lines = sl_destroy(lines, destructor);
        if(buffer)
                free(buffer);
        return 0;
}
