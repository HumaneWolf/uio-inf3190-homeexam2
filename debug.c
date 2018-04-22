#include "debug.h"

#include <arpa/inet.h>
#include <stdio.h>

char setting_debug = 0;

/**
 * Enables the debug printing for the application.
 */
void enable_debug_print()
{
    setting_debug = 1;
}

/**
 * Formats and prints a line of text if, and only if, debug printing is enabled.
 * For arguments, see the printf documentation. It's exactly the same.
 */
void debug_print(char *str, ...)
{
    if (!setting_debug)
        return;

    va_list args;
    va_start(args, str);
    vprintf(str, args);
}
