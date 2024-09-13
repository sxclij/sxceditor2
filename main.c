#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#define term_capacity 65536
#define nodes_capacity 65536

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
struct t_term {
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
    struct t_term term;
    struct nodes nodes;
    enum mode mode;
} global;

size_t term_read(char* dst) {
    return read(STDIN_FILENO, dst, term_capacity);
}
void term_deinit() {
    tcsetattr(STDIN_FILENO, TCSANOW, &global.term.old);
}
void term_init() {
    tcgetattr(STDIN_FILENO, &global.term.old);
    global.term.new = global.term.old;
    global.term.new.c_lflag &= ~(ICANON | ECHO);
    global.term.new.c_cc[VMIN] = 0;
    global.term.new.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &global.term.new);
}

struct node* nodes_insert(struct node* next, char ch) {
    struct node* this = global.nodes.passive[--global.nodes.passive_size];
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
void nodes_delete(struct node* this) {
    struct node* next = this->next;
    struct node* prev = this->prev;
    global.nodes.passive[global.nodes.passive_size++] = this;
    next->prev = prev;
    if (prev != NULL) {
        prev->next = next;
    }
}
void nodes_init() {
    global.nodes.passive_size = nodes_capacity;
    for (uint32_t i = 0; i < nodes_capacity; i++) {
        global.nodes.passive[i] = &global.nodes.data[i];
    }
    global.nodes.insert_selector = global.nodes.passive[--global.nodes.passive_size];
    global.nodes.cmd_selector = global.nodes.passive[--global.nodes.passive_size];
}

enum bool input_cmd(char ch) {
    switch (ch) {
        case 27:
            global.mode = mode_normal;
            return false;
        case '\b':
        case 127:
            if (global.nodes.cmd_selector->prev != NULL) {
                nodes_delete(global.nodes.cmd_selector->prev);
            }
            return false;
        default:
            nodes_insert(global.nodes.cmd_selector, ch);
            return false;
    }
}
enum bool input_normal(char ch) {
    switch (ch) {
        case 'i':
            global.mode = mode_insert;
            return false;
        case ':':
            global.mode = mode_cmd;
            return false;
        case 'q':
            return true;
        case 'h':
            if (global.nodes.insert_selector->prev != NULL) {
                global.nodes.insert_selector = global.nodes.insert_selector->prev;
            }
            return false;
        case 'l':
            if (global.nodes.insert_selector->next != NULL) {
                global.nodes.insert_selector = global.nodes.insert_selector->next;
            }
            return false;
        case 'j':
        default:
            return false;
    }
}
enum bool input_insert(char ch) {
    switch (ch) {
        case 27:
            global.mode = mode_normal;
            return false;
        case '\b':
        case 127:
            if (global.nodes.insert_selector->prev != NULL) {
                nodes_delete(global.nodes.insert_selector->prev);
            }
            return false;
        default:
            nodes_insert(global.nodes.insert_selector, ch);
            return false;
    }
}
enum bool input(char ch) {
    if (global.mode == mode_normal) {
        return input_normal(ch);
    }
    if (global.mode == mode_insert) {
        return input_insert(ch);
    }
    if (global.mode == mode_cmd) {
        return input_cmd(ch);
    }
}
enum bool input_update() {
    char buf[term_capacity];
    size_t n = term_read(buf);
    for (uint32_t i = 0; i < n; i++) {
        if (input(buf[i]) == true) {
            return true;
        }
    }
    return false;
}

void draw_clear() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[1;1H", 7);
}
void draw_text(struct node* this) {
    struct node* node_i = this;
    while (node_i->prev != NULL) {
        node_i = node_i->prev;
    }
    while (node_i != NULL) {
        if (node_i == global.nodes.insert_selector) {
            write(STDOUT_FILENO, "|", 1);
        }
        write(STDOUT_FILENO, &node_i->ch, 1);
        node_i = node_i->next;
    }
}
void draw_info() {
    if (global.mode == mode_insert) {
        write(STDOUT_FILENO, "INSERT_MODE, ", 13);
    } else if (global.mode == mode_normal) {
        write(STDOUT_FILENO, "NORMAL_MODE, ", 13);
    } else if (global.mode == mode_raw) {
        write(STDOUT_FILENO, "RAW_MODE, ", 10);
    } else if (global.mode == mode_cmd) {
        write(STDOUT_FILENO, "CMD_MODE, ", 10);
    }
}
void update_draw() {
    draw_clear();
    draw_info();
        draw_text(global.nodes.cmd_selector);
        write(STDOUT_FILENO, "\n", 1);
    draw_text(global.nodes.insert_selector);
    fflush(stdout);
}
enum bool update() {
    if (input_update() == true) {
        return true;
    }
    update_draw();
    return false;
}
void init() {
    term_init();
    nodes_init();
}
void deinit() {
    term_deinit();
}

int main() {
    init();
    while (1) {
        if (update() == true) {
            break;
        }
        usleep(10000);
    }
    deinit();
    return 0;
}