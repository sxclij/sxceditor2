#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define nodes_capacity (1<<18)
#define script_capacity (1<<16)
#define term_capacity (1<<16)
#define buf_capacity (1<<16)

enum result {
    ok = 0,
    err = 1,
};
enum bool {
    false = 0,
    true = 1,
};
enum mode {
    mode_normal,
    mode_insert,
    mode_cmd,
    mode_raw,
};
struct term {
    struct winsize ws;
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
    struct node* free_itr;
    struct node* text_itr;
    struct node* cmd_itr;
    struct node* message_itr;
};
struct script {
    int32_t mem[script_capacity];
    uint32_t pc;
    uint32_t sp;
    uint32_t bp;  
};
struct global {
    struct term term;
    struct nodes nodes;
    struct script script;
    enum mode mode;
};
uint32_t term_read(char* dst) {
    return read(STDIN_FILENO, dst, term_capacity);
}
void term_deinit(struct term* term) {
    tcsetattr(STDIN_FILENO, TCSANOW, &term->old);
}
void term_update(struct term* term) {
    ioctl(STDIN_FILENO, TIOCGWINSZ, &term->ws);
}
void term_init(struct term* term) {
    tcgetattr(STDIN_FILENO, &term->old);
    term->new = term->old;
    term->new.c_lflag &= ~(ICANON | ECHO);
    term->new.c_cc[VMIN] = 0;
    term->new.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &term->new);
}

void nodes_free(struct nodes* nodes, struct node* this) {
    this->prev = nodes->free_itr;
    nodes->free_itr->next = this;
    nodes->free_itr = this;
}
struct node* nodes_allocate(struct nodes* nodes) {
    struct node* this = nodes->free_itr;
    nodes->free_itr = nodes->free_itr->prev;
    return this;
}
struct node* nodes_insert(struct nodes* nodes, struct node* next, char ch) {
    struct node* this = nodes_allocate(nodes);
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
    nodes_free(nodes, this);
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
void nodes_insert_str(struct nodes* nodes, struct node* next, const char* src) {
    for (uint32_t i = 0; src[i] != '\0'; i++) {
        nodes_insert(nodes, next, src[i]);
    }
}
void nodes_replace_str(struct nodes* nodes, struct node* this, const char* src) {
    nodes_clear(nodes, this);
    nodes_insert_str(nodes, this, src);
}
uint32_t nodes_line_left(struct node* this) {
    struct node* itr = this->prev;
    uint32_t i = 0;
    while (itr != NULL && itr->ch != '\n') {
        itr = itr->prev;
        i++;
    }
    return i;
}
struct node* nodes_line_begin(struct node* this) {
    struct node* itr = this;
    while (itr->prev != NULL) {
        if (itr->prev->ch == '\n') {
            break;
        }
        itr = itr->prev;
    }
    return itr;
}
struct node* nodes_line_rbegin(struct node* this) {
    struct node* itr = this;
    while (itr->next != NULL && itr->ch != '\n') {
        itr = itr->next;
    }
    return itr;
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
void nodes_init(struct nodes* nodes) {
    nodes->free_itr = nodes->data;
    for(uint32_t i = 0; i < nodes_capacity-1; i++) {
        nodes_free(nodes, &nodes->data[i+1]);
    }
    nodes->text_itr = nodes_allocate(nodes);
    nodes->text_itr->ch = '\0';
    nodes->text_itr->prev = NULL;
    nodes->text_itr->next = NULL;
    nodes->cmd_itr = nodes_allocate(nodes);
    nodes->cmd_itr->ch = '\0';
    nodes->cmd_itr->prev = NULL;
    nodes->cmd_itr->next = NULL;
    nodes->message_itr = nodes_allocate(nodes);
    nodes->message_itr->ch = '\0';
    nodes->message_itr->prev = NULL;
    nodes->message_itr->next = NULL;
}
enum result file_read(struct nodes* nodes, struct node* dst, const char* path) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return err;
    }
    while (1) {
        int ch = fgetc(fp);
        if (ch == EOF) {
            break;
        }
        nodes_insert(nodes, dst, ch);
    }
    fclose(fp);
    return ok;
}
enum result file_write(const char* path, struct node* src) {
    struct node* itr = src;
    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        return err;
    }
    while (itr->prev != NULL) {
        itr = itr->prev;
    }
    for (; itr->next != NULL; itr = itr->next) {
        fputc(itr->ch, fp);
    }
    fclose(fp);
    return ok;
}
enum result cmd_openfile(struct nodes* nodes, const char* path) {
    nodes_clear(nodes, nodes->text_itr);
    return file_read(nodes, nodes->text_itr, path);
}
enum result cmd_savefile(struct nodes* nodes, const char* path) {
    return file_write(path, nodes->text_itr);
}
enum result cmd_exec(struct global* global, struct node* this) {
    char buf[buf_capacity];
    char* option;
    uint32_t i = 0;
    nodes_to_str(buf, this);
    nodes_clear(&global->nodes, global->nodes.message_itr);
    while (buf[i] != ' ' && buf[i] != '\0') {
        i++;
    }
    buf[i++] = '\0';
    option = buf + i;
    if (strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0 || strcmp(buf, "q") == 0) {
        return err;
    } else if (strcmp(buf, "open") == 0) {
        if (cmd_openfile(&global->nodes, option) == ok) {
            nodes_replace_str(&global->nodes, global->nodes.message_itr, "open succeeded.");
        } else {
            nodes_replace_str(&global->nodes, global->nodes.message_itr, "open failed.");
        }
        return ok;
    } else if (strcmp(buf, "save") == 0) {
        if (cmd_savefile(&global->nodes, option) == ok) {
            nodes_replace_str(&global->nodes, global->nodes.message_itr, "save succeeded.");
        } else {
            nodes_replace_str(&global->nodes, global->nodes.message_itr, "save failed.");
        }
        return ok;
    } else {
        nodes_replace_str(&global->nodes, global->nodes.message_itr, "command not found.");
        return ok;
    }
}
void input_normal_h(struct nodes* nodes) {
    if (nodes->text_itr->prev != NULL) {
        nodes->text_itr = nodes->text_itr->prev;
    }
}
void input_normal_l(struct nodes* nodes) {
    if (nodes->text_itr->next != NULL) {
        nodes->text_itr = nodes->text_itr->next;
    }
}
void input_normal_j(struct nodes* nodes) {
    uint32_t x = nodes_line_left(nodes->text_itr);
    nodes->text_itr = nodes_line_rbegin(nodes->text_itr);
    input_normal_l(nodes);
    for (uint32_t i = 0; i < x && nodes->text_itr != NULL && nodes->text_itr->ch != '\n'; i++) {
        input_normal_l(nodes);
    }
}
void input_normal_k(struct nodes* nodes) {
    uint32_t x = nodes_line_left(nodes->text_itr);
    nodes->text_itr = nodes_line_begin(nodes->text_itr);
    input_normal_h(nodes);
    nodes->text_itr = nodes_line_begin(nodes->text_itr);
    for (uint32_t i = 0; i < x && nodes->text_itr != NULL && nodes->text_itr->ch != '\n'; i++) {
        input_normal_l(nodes);
    }
}
void input_normal(struct global* global, char ch) {
    switch (ch) {
        case 'i':
            global->mode = mode_insert;
            return;
        case ':':
            global->mode = mode_cmd;
            return;
        case 'h':
            input_normal_h(&global->nodes);
            return;
        case 'l':
            input_normal_l(&global->nodes);
            return;
        case 'j':
            input_normal_j(&global->nodes);
            return;
        case 'k':
            input_normal_k(&global->nodes);
            return;
        default:
            return;
    }
}
enum result input_cmd(struct global* global, char ch) {
    switch (ch) {
        case 27:
            global->mode = mode_normal;
            return ok;
        case '\n':
            if (cmd_exec(global, global->nodes.cmd_itr) == ok) {
                nodes_clear(&global->nodes, global->nodes.cmd_itr);
                global->mode = mode_normal;
                return ok;
            } else {
                return err;
            }
        case '\b':
        case 127:
            if (global->nodes.cmd_itr->prev != NULL) {
                nodes_delete(&global->nodes, global->nodes.cmd_itr->prev);
            }
            return ok;
        default:
            nodes_insert(&global->nodes, global->nodes.cmd_itr, ch);
            return ok;
    }
}
void input_insert(struct global* global, char ch) {
    switch (ch) {
        case 27:
            global->mode = mode_normal;
            return;
        case '\b':
        case 127:
            if (global->nodes.text_itr->prev != NULL) {
                nodes_delete(&global->nodes, global->nodes.text_itr->prev);
            }
            return;
        default:
            nodes_insert(&global->nodes, global->nodes.text_itr, ch);
            return;
    }
}
enum result input_ch(struct global* global, char ch) {
    if (global->mode == mode_normal) {
        input_normal(global, ch);
        return ok;
    }
    if (global->mode == mode_insert) {
        input_insert(global, ch);
        return ok;
    }
    if (global->mode == mode_cmd) {
        return input_cmd(global, ch);
    } else {
        return err;
    }
}
enum result input_update(struct global* global) {
    char buf[term_capacity];
    uint32_t n = term_read(buf);
    for (uint32_t i = 0; i < n; i++) {
        if (input_ch(global, buf[i]) == ok) {
            continue;
        } else {
            return err;
        }
    }
    return ok;
}
void draw_clear() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[1;1H", 7);
}
void draw_text(struct node* this, uint32_t size_y, enum bool is_cursor) {
    struct node* itr = this;
    uint32_t i;
    for (i = 0; itr->prev != NULL && i < size_y / 2 + 1;) {
        itr = itr->prev;
        if (itr->ch == '\n') {
            i++;
        }
    }
    if (itr->ch == '\n' && itr->prev != NULL) {
        itr = itr->next;
    }
    for (i = 0; i < size_y && itr != NULL;) {
        if (itr == this && is_cursor == true) {
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
        draw_text(message_selector, 1, false);
        write(STDOUT_FILENO, "]", 1);
    }
}
void draw_cmd(struct node* cmd_selector) {
    if (cmd_selector->prev != NULL) {
        write(STDOUT_FILENO, ", cmd: ", 7);
        draw_text(cmd_selector, 1, false);
    }
}
void draw_update(struct global* global) {
    draw_clear();
    draw_info(global->mode);
    draw_message(global->nodes.message_itr);
    draw_cmd(global->nodes.cmd_itr);
    write(STDOUT_FILENO, "\n", 1);
    draw_text(global->nodes.text_itr, global->term.ws.ws_row - 2, true);
}
void draw_deinit() {
    draw_clear();
    write(STDOUT_FILENO, "sxceditor exit.\n", 17);
}
enum result update(struct global* global) {
    if (input_update(global) == err) {
        return err;
    }
    term_update(&global->term);
    draw_update(global);
    return ok;
}
void init(struct global* global) {
    term_init(&global->term);
    nodes_init(&global->nodes);
}
void deinit(struct global* global) {
    draw_deinit();
    term_deinit(&global->term);
}
int main() {
    static struct global global;
    init(&global);
    while (1) {
        if (update(&global) == ok) {
            usleep(10000);
        } else {
            break;
        }
    }
    deinit(&global);
    return 0;
}