#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#define term_capacity 65536

struct t_term {
    struct termios old;
    struct termios new;
};
struct global {
    struct t_term term;
} global;

void term_exit() {
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

void main() {
    term_init();
    
    while(1) {
        char buf[term_capacity];
        size_t n = term_read(buf);
        write(STDOUT_FILENO, "\x1b[1;1H", 7);
        write(STDOUT_FILENO, buf, n);
        fflush(stdout);
        if(buf[0] == 'q') break;
    }
    
    term_exit();
}