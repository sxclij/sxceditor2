#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define term_capacity 65536
#define nodes_capacity 65536
#define buf_capacity 65536

enum bool {
    false = 0,
    true = 1
};
enum mode {
    mode_normal,
    mode_insert,
    mode_cmd,
    mode_raw,
};
struct term {
    struct termios old;
    struct termios new;
};
struct node {
    struct node* next;
    struct node* prev;
    char ch;
};
struct nodes {
    struct node data[nodes_capacity];
    struct node* passive[nodes_capacity];
    struct node* insert_selector;
    struct node* cmd_selector;
    struct node* message_selector;
    uint32_t passive_size;
};
struct global {
    struct term term;
    struct nodes nodes;
    enum mode mode;
};
uint32_t term_read(char* dst) {
    return read(STDIN_FILENO, dst, term_capacity);
}
void term_deinit(struct term* term) {
    tcsetattr(STDIN_FILENO, TCSANOW, &term->old);
}
void term_init(struct term* term) {
    tcgetattr(STDIN_FILENO, &term->old);
    term->new = term->old;
    term->new.c_lflag &= ~(ICANON | ECHO);
    term->new.c_cc[VMIN] = 0;
    term->new.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &term->new);
}
struct node* nodes_insert(struct nodes* nodes, struct node* next, char ch) {
    struct node* this = nodes->passive[--nodes->passive_size];
    struct node* prev = next->prev;
    this->ch = ch;
    this->next = next;
    this->prev = prev;
    next->prev = this;
    if (prev != NULL) {
        prev->next = this;
    }
    return this;
}
void nodes_delete(struct nodes* nodes, struct node* this) {
    struct node* next = this->next;
    struct node* prev = this->prev;
    nodes->passive[nodes->passive_size++] = this;
    if (next != NULL) {
        next->prev = prev;
    }
    if (prev != NULL) {
        prev->next = next;
    }
}
void nodes_clear(struct nodes* nodes, struct node* this) {
    struct node* itr = this;
    while (itr->next != NULL) {
        nodes_delete(nodes, itr->next);
    }
    while (itr->prev != NULL) {
        nodes_delete(nodes, itr->prev);
    }
    itr->ch = '\0';
}
void nodes_to_str(char* dst, struct node* src) {
    struct node* itr = src;
    uint32_t i;
    while (itr->prev != NULL) {
        itr = itr->prev;
    }
    for (i = 0; itr != NULL; i++) {
        dst[i] = itr->ch;
        itr = itr->next;
    }
    dst[i + 1] = '\0';
}
void nodes_insert_str(struct nodes* nodes, struct node* next, const char* src) {
    for (uint32_t i = 0; src[i] != '\0'; i++) {
        nodes_insert(nodes, next, src[i]);
    }
}
void nodes_replace_str(struct nodes* nodes, struct node* this, const char* src) {
    nodes_clear(nodes, this);
    nodes_insert_str(nodes, this, src);
}
void nodes_init(struct nodes* nodes) {
    nodes->passive_size = nodes_capacity;
    for (uint32_t i = 0; i < nodes_capacity; i++) {
        nodes->passive[i] = &nodes->data[i];
    }
    nodes->insert_selector = nodes->passive[--nodes->passive_size];
    nodes->cmd_selector = nodes->passive[--nodes->passive_size];
    nodes->message_selector = nodes->passive[--nodes->passive_size];
}
enum bool file_read(struct nodes* nodes, struct node* dst, const char* path) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return true;
    }
    while (1) {
        int ch = fgetc(fp);
        if (ch == EOF) {
            break;
        }
        nodes_insert(nodes, dst, ch);
    }
    fclose(fp);
    return false;
}
enum bool file_write(const char* path, struct node* src) {
    struct node* itr = src;
    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        return true;
    }
    while (itr->prev != NULL) {
        itr = itr->prev;
    }
    for (; itr->next != NULL; itr = itr->next) {
        fputc(itr->ch, fp);
    }
    return false;
}
enum bool cmd_openfile(struct nodes* nodes, const char* path) {
    nodes_clear(nodes, nodes->insert_selector);
    return file_read(nodes, nodes->insert_selector, path);
}
enum bool cmd_exec(struct global* global, struct node* this) {
    char buf1[buf_capacity];
    char buf2[buf_capacity];
    char buf3[buf_capacity];
    uint32_t i;
    nodes_to_str(buf1, this);
    nodes_clear(&global->nodes, global->nodes.message_selector);
    for (i = 0; buf1[i] != ' ' && buf1[i] != '\0'; i++) {
        buf2[i] = buf1[i];
    }
    buf2[i++] = '\0';
    if (strcmp(buf2, "exit") == 0 || strcmp(buf2, "quit") == 0 || strcmp(buf2, "q") == 0) {
        return true;
    } else if (strcmp(buf2, "open") == 0) {
        if (cmd_openfile(&global->nodes, buf1 + i)) {
            sprintf(buf3, "open %s failed.", buf1 + i);
            nodes_replace_str(&global->nodes, global->nodes.message_selector, buf3);
        } else {
            sprintf(buf3, "open %s succeeded.", buf1 + i);
            nodes_replace_str(&global->nodes, global->nodes.message_selector, buf3);
        }
        return false;
    } else if (strcmp(buf2, "save") == 0) {
        if (file_write(buf1 + i, global->nodes.insert_selector)) {
            nodes_replace_str(&global->nodes, global->nodes.message_selector, "save failed.");
        } else {
            nodes_replace_str(&global->nodes, global->nodes.message_selector, "save succeeded.");
        }
        return false;
    } else {
        nodes_replace_str(&global->nodes, global->nodes.message_selector, "command not found.");
        return false;
    }
}
void input_normal_h(struct nodes* nodes) {
    if (nodes->insert_selector->prev != NULL) {
        nodes->insert_selector = nodes->insert_selector->prev;
    }
    if (nodes->insert_selector->prev != NULL) {
        if (nodes->insert_selector->ch == '\n')
            nodes->insert_selector = nodes->insert_selector->prev;
    }
}
void input_normal_l(struct nodes* nodes) {
    if (nodes->insert_selector->next != NULL) {
        nodes->insert_selector = nodes->insert_selector->next;
    }
    if (nodes->insert_selector->next != NULL) {
        if (nodes->insert_selector->ch == '\r')
            nodes->insert_selector = nodes->insert_selector->next;
    }
}
void input_normal_j(struct nodes* nodes) {
    uint32_t i, j;
    for (i = 0; nodes->insert_selector->prev != NULL; i++) {
        if (nodes->insert_selector->prev->ch == '\n') {
            break;
        }
        input_normal_h(nodes);
    }
    if (nodes->insert_selector->prev == NULL) {
        return;
    }
    input_normal_h(nodes);
    while (nodes->insert_selector->prev != NULL) {
        if (nodes->insert_selector->prev->ch == '\n') {
            break;
        }
        input_normal_h(nodes);
    }
    for (j = 0; j < i && nodes->insert_selector->next != NULL && nodes->insert_selector->next->ch != '\n'; j++) {
        input_normal_l(nodes);
    }
}
void input_normal_k(struct nodes* nodes) {
    uint32_t i, j;
    for (i = 0; nodes->insert_selector->prev != NULL; i++) {
        if (nodes->insert_selector->prev->ch == '\n') {
            break;
        }
        input_normal_h(nodes);
    }
    while (nodes->insert_selector->next != NULL) {
        if (nodes->insert_selector->ch == '\n') {
            break;
        }
        input_normal_l(nodes);
    }
    for (j = 0; j < i + 1 && nodes->insert_selector->next != NULL && nodes->insert_selector->next->ch != '\n'; j++) {
        input_normal_l(nodes);
    }
}
enum bool input_normal(struct global* global, char ch) {
    uint32_t i, j;
    switch (ch) {
        case 'i':
            global->mode = mode_insert;
            return false;
        case ':':
            global->mode = mode_cmd;
            return false;
        case 'q':
            return true;
        case 'h':
            input_normal_h(&global->nodes);
            return false;
        case 'l':
            input_normal_l(&global->nodes);
            return false;
        case 'j':
            input_normal_j(&global->nodes);
            return false;
        case 'k':
            input_normal_k(&global->nodes);
            return false;
        default:
            return false;
    }
}
enum bool input_cmd(struct global* global, char ch) {
    switch (ch) {
        case 27:
            global->mode = mode_normal;
            return false;
        case '\n':
            if (cmd_exec(global, global->nodes.cmd_selector)) {
                return true;
            } else {
                nodes_clear(&global->nodes, global->nodes.cmd_selector);
                global->mode = mode_normal;
                return false;
            }
        case '\b':
        case 127:
            if (global->nodes.cmd_selector->prev != NULL) {
                nodes_delete(&global->nodes, global->nodes.cmd_selector->prev);
            }
            return false;
        default:
            nodes_insert(&global->nodes, global->nodes.cmd_selector, ch);
            return false;
    }
}
enum bool input_insert(struct global* global, char ch) {
    switch (ch) {
        case 27:
            global->mode = mode_normal;
            return false;
        case '\b':
        case 127:
            if (global->nodes.insert_selector->prev != NULL) {
                nodes_delete(&global->nodes, global->nodes.insert_selector->prev);
            }
            return false;
        default:
            nodes_insert(&global->nodes, global->nodes.insert_selector, ch);
            return false;
    }
}
enum bool input_ch(struct global* global, char ch) {
    if (global->mode == mode_normal) {
        return input_normal(global, ch);
    }
    if (global->mode == mode_insert) {
        return input_insert(global, ch);
    }
    if (global->mode == mode_cmd) {
        return input_cmd(global, ch);
    }
}
enum bool input(struct global* global) {
    char buf[term_capacity];
    uint32_t n = term_read(buf);
    for (uint32_t i = 0; i < n; i++) {
        if (input_ch(global, buf[i]) == true) {
            return true;
        }
    }
    return false;
}
void draw_clear() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[1;1H", 7);
}
void draw_text(struct node* this, enum bool is_cursor) {
    struct node* itr = this;
    uint32_t i;
    for (i = 0; itr->prev != NULL && i < 6;) {
        if (itr->ch == '\n') {
            i++;
        }
        itr = itr->prev;
    }
    while (i < 18 && itr != NULL) {
        if (itr == this && is_cursor) {
            write(STDOUT_FILENO, "|", 1);
        }
        if (itr->ch == '\n') {
            i++;
        }
        write(STDOUT_FILENO, &itr->ch, 1);
        itr = itr->next;
    }
}
void draw_info(enum mode mode) {
    if (mode == mode_insert) {
        write(STDOUT_FILENO, "[INSERT_MODE]", 13);
    } else if (mode == mode_normal) {
        write(STDOUT_FILENO, "[NORMAL_MODE]", 13);
    } else if (mode == mode_raw) {
        write(STDOUT_FILENO, "[RAW_MODE]", 10);
    } else if (mode == mode_cmd) {
        write(STDOUT_FILENO, "[CMD_MODE]", 10);
    }
}
void draw_message(struct node* message_selector) {
    if (message_selector->prev != NULL) {
        write(STDOUT_FILENO, ", message: [", 12);
        draw_text(message_selector, false);
        write(STDOUT_FILENO, "]", 1);
    }
}
void draw_cmd(struct node* cmd_selector) {
    if (cmd_selector->prev != NULL) {
        write(STDOUT_FILENO, ", cmd: ", 7);
        draw_text(cmd_selector, false);
    }
}
void draw(struct global* global) {
    draw_clear();
    draw_info(global->mode);
    draw_message(global->nodes.message_selector);
    draw_cmd(global->nodes.cmd_selector);
    write(STDOUT_FILENO, "\n", 1);
    draw_text(global->nodes.insert_selector, true);
    fflush(stdout);
}
enum bool update(struct global* global) {
    if (input(global)) {
        return true;
    }
    draw(global);
    return false;
}
void init(struct global* global) {
    term_init(&global->term);
    nodes_init(&global->nodes);
}
void deinit(struct global* global) {
    term_deinit(&global->term);
}
int main() {
    static struct global global;
    init(&global);
    while (1) {
        if (update(&global) == true) {
            break;
        }
        usleep(10000);
    }
    draw_clear();
    write(STDOUT_FILENO, "sxceditor exit.\n", 17);
    deinit(&global);
    return 0;
}