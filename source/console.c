/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Gil Mendes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file
 * @brief               Console functions.
 */

#include <lib/printf.h>

#include <console.h>
#include <loader.h>

/** Debug output log. */
char debug_log[DEBUG_LOG_SIZE];
size_t debug_log_start;
size_t debug_log_length;

/** Main console. */
console_t main_console;

/** Debug console. */
console_t debug_console;

/** Helper for console_vprintf().
 * @param ch            Character to display.
 * @param data          Console to use.
 * @param total         Pointer to total character count. */
void console_vprintf_helper(char ch, void *data, int *total) {
    console_t *console = data;

    console_putc(console, ch);

    if (console == &debug_console) {
        /* Store in the log buffer. */
        debug_log[(debug_log_start + debug_log_length) % DEBUG_LOG_SIZE] = ch;
        if (debug_log_length < DEBUG_LOG_SIZE) {
            debug_log_length++;
        } else {
            debug_log_start = (debug_log_start + 1) % DEBUG_LOG_SIZE;
        }
    }

    *total = *total + 1;
}

/** Output a formatted message to a console.
 * @param console       Console to print to.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int console_vprintf(console_t *console, const char *fmt, va_list args) {
    return do_vprintf(console_vprintf_helper, console, fmt, args);
}

/** Output a formatted message to a console.
 * @param console       Console to print to.
 * @param fmt           Format string used to create the message.
 * @param ...           Arguments to substitute into format.
 * @return              Number of characters printed. */
int console_printf(console_t *console, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = console_vprintf(console, fmt, args);
    va_end(args);

    return ret;
}