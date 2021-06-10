#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>



int main(int argc, char *argv[]) {


    /** Save cursor position */
    printf("\033[s");




    /** Restore cursor position */
    printf("\033[u");

    

    return 0;
}