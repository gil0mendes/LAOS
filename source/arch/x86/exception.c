/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2015 Gil Mendes
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
 * @brief       x86 exception handling functions
 */

#include <x86/exception.h>

#include <loader.h>

/**
 * Handle an exception.
 *
 * @param frame		Interrupt frame.
 */
void x86_exception_handler(exception_frame_t *frame) {
	#ifdef __LP64__
    internal_error("Exception %lu (error code 0x%lx)\n"
                   "rip: 0x%016lx  cs:  0x%04lx\n"
                   "rsp: 0x%016lx  ss:  0x%04lx\n"
                   "rax: 0x%016lx  rbx: 0x%016lx  rcx: 0x%016lx\n"
                   "rdx: 0x%016lx  rdi: 0x%016lx  rsi: 0x%016lx\n"
                   "rbp: 0x%016lx  r8:  0x%016lx  r9:  0x%016lx\n"
                   "r10: 0x%016lx  r11: 0x%016lx  r12: 0x%016lx\n"
                   "r13: 0x%016lx  r14: 0x%016lx  r15: 0x%016lx\n"
                   "rfl: 0x%016x",
                   frame->num, frame->err_code, frame->ip, frame->cs, frame->sp,
                   frame->ss, frame->ax, frame->bx, frame->cx, frame->dx, frame->di,
                   frame->si, frame->bp, frame->r8, frame->r9, frame->r10, frame->r11,
                   frame->r12, frame->r13, frame->r14, frame->r15, frame->flags);
    #else
    internal_error("Exception %lu (error code 0x%lx)\n"
                   "eip: 0x%08lx  cs:  0x%04lx\n"
                   "ds:  0x%04lx      es:  0x%04lx      fs:  0x%04lx      gs:  0x%04lx\n"
                   "eax: 0x%08lx  ebx: 0x%08lx  ecx: 0x%08lx  edx: 0x%08lx\n"
                   "edi: 0x%08lx  esi: 0x%08lx  ebp: 0x%08lx  esp: 0x%08lx\n"
                   "efl: 0x%08x",
                   frame->num, frame->err_code, frame->ip, frame->cs, frame->ds,
                   frame->es, frame->fs, frame->gs, frame->ax, frame->bx,
                   frame->cx, frame->dx, frame->di, frame->si, frame->bp,
                   frame->sp, frame->flags);
    #endif
}
