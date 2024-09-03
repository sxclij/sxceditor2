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
    normal,
    insert,
    cmd,
    raw,
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
    struct node* head;
    struct node* selector;
    uint32_t passive_size;
};
struct global {
    struct t_term term;
    struct nodes nodes;
} global;

void term_deinit() {
    tcsetattr(STDIN_FILENO, TCSANOW, &global.term.old);
}
size_t term_read(char* dst) {
    return read(STDIN_FILENO, dst, term_capacity);
}
void term_init() {
    tcgetattr(STDIN_FILENO, &global.term.old);
    global.term.new = global.term.old;
    global.term.new.c_lflag &= ~(ICANON | ECHO);
    global.term.new.c_cc[VMIN] = 0;
    global.term.new.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &global.term.new);
}

struct node* nodes_insert(char ch) {
    struct node* this = global.nodes.passive[--global.nodes.passive_size];
    struct node* next = global.nodes.selector;
    struct node* prev = global.nodes.selector->prev;
    this->ch = ch;
    this->next = global.nodes.selector;
    this->prev = global.nodes.selector->prev;
    next->prev = this;
    if (prev != NULL) {
        prev->next = this;
    }
    return this;
}
void nodes_delete(struct node* this) {
    if (this == NULL) {
        return;
    }
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
    global.nodes.head = global.nodes.passive[--global.nodes.passive_size];
    global.nodes.selector = global.nodes.head;
}

enum bool cmd(char ch) {
    switch (ch) {
        case 'q':
            return true;
        default:
            return false;
    }
}

enum bool input(char ch) {
    nodes_insert(ch);
    if (ch == 'q') {
        return true;
    }
    return false;
}

enum bool update_input() {
    char buf[term_capacity];
    size_t n = term_read(buf);
    for (uint32_t i = 0; i < n; i++) {
        if (input(buf[i]) == true) {
            return true;
        }
    }
    return false;
}
void update_draw() {
    write(STDOUT_FILENO, "\x1b[1;1H", 7);
    struct node* node_i = global.nodes.head;
    while (node_i->prev != NULL) {
        node_i = node_i->prev;
    }
    for (; node_i != NULL; node_i = node_i->next) {
        write(STDOUT_FILENO, &node_i->ch, 1);
    }
    fflush(stdout);
}
enum bool update() {
    if (update_input() == true) {
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
        usleep(1000);
    }
    deinit();
    return 0;
}