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
 * @brief       x86 descriptor table functions
 */

#include <x86/descriptor.h>

#include <loader.h>

extern uint8_t isr_array[IDT_ENTRY_COUNT][16];

// Array of GDT descriptors
static gdt_entry_t loader_gdt[GDT_ENTRY_COUNT] = {
	// NULL descriptor (0x0)
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },

	// 32-bit code (0x8)
	{ 0xffff, 0, 0xa, 1, 0, 1, 0xf, 0, 1, 1, 0 },

	// 32-bit data (0x10)
	{ 0xffff, 0, 0x2, 1, 0, 1, 0xf, 0, 1, 1, 0 },

	// 64-bit code (0x18)
	{ 0xffff, 0, 0xa, 1, 0, 1, 0xf, 1, 0, 1, 0 },

	// 64-bit data (0x20)
	{ 0xffff, 0, 0x2, 1, 0, 1, 0xf, 0, 0, 1, 0 },

	// 16-bit code (0x28)
	{ 0xffff, 0x10000, 0xa, 1, 0, 1, 0, 0, 0, 0, 0 },

	// 16-bit data (0x30)
	{ 0xffff, 0x10000, 0x2, 1, 0, 1, 0, 0, 0, 0, 0 },
};

// GDT pointer to the loader GDT
gdt_pointer_t loader_gdtp __section(".init.data") = {
	.limit = sizeof(loader_gdt) - 1,
	.base = (ptr_t)&loader_gdt,
};

// Interrupt descriptor table
static idt_entry_t loader_idt[IDT_ENTRY_COUNT];

// IDT pointer to the loader IDT
idt_pointer_t loader_idtp __section(".init.data") = {
	.limit = sizeof(loader_idt) - 1,
	.base = (ptr_t)&loader_idt,
};

/**
 * Initialize descriptor tables
 */
void x86_descriptor_init(void) {
	ptr_t addr;
	size_t i;

	// Fill out the handlers in the IDT
	for(i = 0; i < IDT_ENTRY_COUNT; i++) {
		addr = (ptr_t)&isr_array[i];
		loader_idt[i].base0 = (addr & 0xffff);
		loader_idt[i].base1 = ((addr >> 16) & 0xffff);
#ifdef __LP64__
		loader_idt[i].base2 = ((addr >> 32) & 0xffffffff);
		loader_idt[i].ist = 0;
#endif
		loader_idt[i].sel = SEGMENT_CS;
		loader_idt[i].flags = 0x8e;
	}

	// Load the new IDT pointer. We don't need to load the GDT, should
	// already have been done by platform init code.
	x86_lidt((ptr_t)&loader_idt, sizeof(loader_idt) - 1);
}
