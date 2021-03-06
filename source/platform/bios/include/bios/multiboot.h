/**
* The MIT License (MIT)
*
* Copyright (c) 2014 Gil Mendes <gil00mendes@gmail.com>
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
 * @brief               BIOS platform Multiboot support.
 */

#ifndef __BIOS_MULTIBOOT_H
#define __BIOS_MULTIBOOT_H

#include <x86/multiboot.h>

extern uint32_t multiboot_magic;
extern multiboot_info_t multiboot_info;

/** @return Whether booted via Multiboot. */
static inline bool multiboot_valid(void) {
    return multiboot_magic == MULTIBOOT_LOADER_MAGIC;
}

extern void multiboot_init(void);

#endif /* __BIOS_MULTIBOOT_H */
