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
    next->prev = prev;
    if (prev != NULL) {
        prev->next = next;
    }
}
void nodes_clear(struct nodes* nodes, struct node* this) {
    struct node* itr = this;
    while (itr->next != NULL) {
        itr = itr->next;
    }
    while (itr->prev != NULL) {
        nodes_delete(nodes, itr->prev);
    }
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
    nodes->passive_size = nodes_capacity;
    for (uint32_t i = 0; i < nodes_capacity; i++) {
        nodes->passive[i] = &nodes->data[i];
    }
    nodes->insert_selector = nodes->passive[--nodes->passive_size];
    nodes->cmd_selector = nodes->passive[--nodes->passive_size];
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
    char buf[buf_capacity];
    nodes_to_str(buf, src);
    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        return true;
    }
    fwrite(buf, 1, strlen(buf), fp);
    fclose(fp);
    return false;
}

enum bool cmd_exec(struct global* global, struct node* this) {
    char buf1[buf_capacity];
    char buf2[buf_capacity];
    uint32_t i;
    nodes_to_str(buf1, this);
    for (i = 0; buf1[i] != ' ' && buf1[i] != '\0'; i++) {
        buf2[i] = buf1[i];
    }
    buf2[i++] = '\0';
    if (strcmp(buf2, "exit") == 0 || strcmp(buf2, "quit") == 0) {
        return true;
    }
    if (strcmp(buf2, "open") == 0) {
        nodes_clear(&global->nodes, global->nodes.insert_selector);
        return file_read(&global->nodes, global->nodes.insert_selector, buf1 + i);
    }
    if (strcmp(buf2, "save") == 0) {
        return file_write(buf1 + i, global->nodes.insert_selector);
    }
    return false;
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
            if (global->nodes.insert_selector->prev != NULL) {
                global->nodes.insert_selector = global->nodes.insert_selector->prev;
            }
            return false;
        case 'l':
            if (global->nodes.insert_selector->next != NULL) {
                global->nodes.insert_selector = global->nodes.insert_selector->next;
            }
            return false;
        case 'j':
            for(i=0;global->nodes.insert_selector->prev != NULL; i++) {
                if (global->nodes.insert_selector->prev->ch == '\n') {
                    break;
                }
                global->nodes.insert_selector = global->nodes.insert_selector->prev;
            }
            if (global->nodes.insert_selector->prev == NULL) {
                return false;
            }
            global->nodes.insert_selector = global->nodes.insert_selector->prev;
            while (global->nodes.insert_selector->prev != NULL) {
                if (global->nodes.insert_selector->prev->ch == '\n') {
                    break;
                }
                global->nodes.insert_selector = global->nodes.insert_selector->prev;
            }
            for(j=0; j<i && global->nodes.insert_selector->next != NULL && global->nodes.insert_selector->next->ch != '\n'; j++) {
                global->nodes.insert_selector = global->nodes.insert_selector->next;
            }
            return false;
        case 'k':
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
enum bool input(struct global* global, char ch) {
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
enum bool input_update(struct global* global) {
    char buf[term_capacity];
    uint32_t n = term_read(buf);
    for (uint32_t i = 0; i < n; i++) {
        if (input(global, buf[i]) == true) {
            return true;
        }
    }
    return false;
}

void draw_clear() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[1;1H", 7);
}
void draw_text(struct node* insert_selector) {
    struct node* itr = insert_selector;
    while (itr->prev != NULL) {
        itr = itr->prev;
    }
    while (itr != NULL) {
        if (itr == insert_selector) {
            write(STDOUT_FILENO, "|", 1);
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
void draw_cmd(struct node* cmd_selector) {
    if (cmd_selector->prev != NULL) {
        write(STDOUT_FILENO, ", cmd: ", 7);
        draw_text(cmd_selector);
    }
}
void update_draw(struct global* global) {
    draw_clear();
    draw_info(global->mode);
    draw_cmd(global->nodes.cmd_selector);
    write(STDOUT_FILENO, "\n", 1);
    draw_text(global->nodes.insert_selector);
    fflush(stdout);
}
enum bool update(struct global* global) {
    if (input_update(global) == true) {
        return true;
    }
    update_draw(global);
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
    deinit(&global);
    return 0;
}