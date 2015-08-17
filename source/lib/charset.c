/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Gil Mendes
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
 * @brief               Character set conversion functions.
 */

#include <lib/charset.h>

/**
 * Convert a UTF-16 string to UTF-8.
 *
 * Converts a UTF-16 or UCS-2 string in native endian to a UTF-8 string. The
 * supplied destination buffer must be at least (src_len * MAX_UTF8_PER_UTF16)
 * bytes long. The converted string will NOT be NULL-terminated.
 *
 * @param dest          Destination buffer.
 * @param src           Source string.
 * @param src_len       Maximum source length, will return early if a zero
 *                      character is encountered.
 *
 * @return              Length of converted string.
 */
size_t utf16_to_utf8(uint8_t *dest, const uint16_t *src, size_t src_len) {
    uint32_t surrogate = 0;
    size_t len = 0;

    while (src_len--) {
        uint32_t code = *src++;

        if (surrogate) {
            /* Have a high surrogate, this is the low surrogate. */
            if (code < 0xdc00 || code > 0xdfff) {
                dest[len++] = '?';
            } else {
                code = 0x10000 + (((surrogate - 0xd800) << 10) | (code - 0xdc00));

                dest[len++] = 0xf0 | (code >> 18);
                dest[len++] = 0x80 | ((code >> 12) & 0x3f);
                dest[len++] = 0x80 | ((code >> 6) & 0x3f);
                dest[len++] = 0x80 | (code & 0x3f);
            }

            surrogate = 0;
        } else if (!code) {
            break;
        } else if (code <= 0x7f) {
            dest[len++] = code;
        } else if (code <= 0x7ff) {
            dest[len++] = 0xc0 | (code >> 6);
            dest[len++] = 0x80 | (code & 0x3f);
        } else if (code >= 0xd800 && code <= 0xdbff) {
            /* High surrogate. */
            surrogate = code;
        } else if (code >= 0xdc00 && code <= 0xdfff) {
            /* This is a low surrogate code but we haven't had a high. */
            dest[len++] = '?';
        } else {
            dest[len++] = 0xe0 | (code >> 12);
            dest[len++] = 0x80 | ((code >> 6) & 0x3f);
            dest[len++] = 0x80 | (code & 0x3f);
        }
    }

    return len;
}
