#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lab.h"

void parse_args(int argc, char **argv) {
    int c;

    while ((c = getopt (argc, argv, "v")) != -1) {
        switch (c) {
            case 'v':
                printf("Version %d.%d", lab_VERSION_MAJOR, lab_VERSION_MINOR);
                return 1;
        }
    }
}
