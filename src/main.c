#include "shell.h"
#include "builtins.h"

// nothing fancy here, just calls the loop and lets it handle the rest
int main(void) {
    shell_loop();
    return final_exit_code;
}
