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
 * FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file
 * @brief               Memory management.
 */

#include <lib/utility.h>

#include <memory.h>
#include <test.h>

/** Size of the heap. */
#define HEAP_SIZE       32768

/** Physical memory allocation range. */
static phys_ptr_t phys_next;
static phys_size_t phys_size;

/** Virtual memory allocation range. */
static ptr_t virt_next;
static size_t virt_size;

/** Statically allocated heap. */
static uint8_t heap[HEAP_SIZE] __aligned(PAGE_SIZE);
static size_t heap_offset;

INITIUM_LOAD(0, 0, 0, VIRT_MAP_BASE, VIRT_MAP_SIZE);

#ifdef PHYS_MAP_BASE
INITIUM_MAPPING(PHYS_MAP_BASE, 0, PHYS_MAP_SIZE);
#endif

/** Map physical memory.
 * @param addr          Physical address to map.
 * @param size          Size of range to map.
 * @return              Pointer to virtual mapping. */
void *phys_map(phys_ptr_t addr, size_t size)
{
	assert(!(addr % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));

    #ifdef PHYS_MAP_BASE
	assert(addr + size - 1 <= PHYS_MAP_BASE + PHYS_MAP_SIZE - 1);
	return (void*)(addr + PHYS_MAP_BASE);
    #else
	ptr_t virt = virt_alloc(size);
	mmu_map(virt, addr, size);
	return (void*)virt;
    #endif
}

/** Allocate physical memory.
 * @param size          Size of range to allocate. */
phys_ptr_t phys_alloc(phys_size_t size)
{
	phys_ptr_t ret;

	assert(!(size % PAGE_SIZE));

	if (size > phys_size)
		internal_error("Exhausted physical memory");

	ret = phys_next;
	phys_next += size;
	phys_size -= size;
	return ret;
}

/**
 * Allocate a range of physical memory.
 *
 * @param size          Size of the range (multiple of PAGE_SIZE).
 * @param align         Alignment of the range (power of 2, at least PAGE_SIZE).
 * @param min_addr      Minimum address for the start of the allocated range.
 * @param max_addr      Maximum address of the last byte of the allocated range.
 * @param type          Type to give the allocated range.
 * @param flags         Behaviour flags.
 * @param _phys         Where to store physical address of allocation.
 * @return              Virtual address of allocation on success, NULL on failure.
 */
void *memory_alloc(
	phys_size_t size, phys_size_t align, phys_ptr_t min_addr, phys_ptr_t max_addr,
	uint8_t type, unsigned flags, phys_ptr_t *_phys)
{
	phys_ptr_t phys;

	if (min_addr || max_addr || align > PAGE_SIZE)
		internal_error("Unsupported allocation constraints");

	phys = phys_alloc(size);
	if (_phys)
		*_phys = phys;

	return phys_map(phys, size);
}

/**
 * Free a range of physical memory.
 *
 * @param addr          Virtual address of allocation.
 * @param size          Size of range to free.
 */
void memory_free(void *addr, phys_size_t size)
{
	/* Nothing happens. */
}

/** Initialize the physical memory manager.
 * @param tags          Tag list. */
static void phys_init(initium_tag_t *tags)
{
	/* Look for the largest accessible memory range. */
	while (tags->type != INITIUM_TAG_NONE) {
		if (tags->type == INITIUM_TAG_MEMORY) {
			initium_tag_memory_t *tag = (initium_tag_memory_t*)tags;
			initium_paddr_t end;

			end = tag->start + tag->size - 1;

			if (end <= PHYS_MAX && tag->size >= phys_size) {
				phys_next = tag->start;
				phys_size = tag->size;
			}
		}

		tags = (initium_tag_t*)round_up((ptr_t)tags + tags->size, 8);
	}

	if (!phys_size)
		internal_error("No usable physical memory range found");

	printf("phys_next = 0x%" PRIxPHYS ", phys_size = 0x%" PRIxPHYS "\n", phys_next, phys_size);
}

/** Allocate virtual address space.
 * @param size          Size of range to allocate. */
ptr_t virt_alloc(size_t size)
{
	ptr_t ret;

	assert(!(size % PAGE_SIZE));

	if (size > virt_size)
		internal_error("Exhausted virtual address space");

	ret = virt_next;
	virt_next += size;
	virt_size -= size;
	return ret;
}

/** Initialize the virtual memory manager.
 * @param tags          Tag list. */
static void virt_init(initium_tag_t *tags)
{
	/* Move the range after any KBoot allocations. */
	virt_next = VIRT_MAP_BASE;
	while (tags->type != INITIUM_TAG_NONE) {
		if (tags->type == INITIUM_TAG_VMEM) {
			initium_tag_vmem_t *tag = (initium_tag_vmem_t*)tags;
			ptr_t end;

			end = tag->start + tag->size;

			if (tag->start >= VIRT_MAP_BASE && end - 1 <= VIRT_MAP_BASE + VIRT_MAP_SIZE - 1) {
				if (tag->start != virt_next)
					internal_error("Virtual ranges are non-contiguous");

				virt_next = end;
			}
		}

		tags = (initium_tag_t*)round_up((ptr_t)tags + tags->size, 8);
	}

	virt_size = VIRT_MAP_SIZE - (virt_next - VIRT_MAP_BASE);

	if (!virt_next || !virt_size)
		internal_error("No usable virtual memory range found");

	printf("virt_next = %p, virt_size = 0x%zx\n", virt_next, virt_size);
}

/** Allocate memory from the heap.
 * @param size          Size of allocation to make.
 * @return              Address of allocation. */
void *malloc(size_t size)
{
	size_t offset = heap_offset;

	size = round_up(size, 8);
	heap_offset += size;
	return (void*)(heap + offset);
}

/**
 * Resize a memory allocation.
 *
 * @param  addr Address of old allocation.
 * @param  size New size of allocation.
 * @return      Address of new allocation, or NULL if size if 0.
 */
void *realloc(void *addr, size_t size) {
  return NULL;
}

/** Free memory from the heap.
 * @param addr          Address to free. */
void free(void *addr)
{
	/* Nope. */
}

/** Initialize the memory manager.
 * @param tags          Tag list. */
void mm_init(initium_tag_t *tags)
{
	phys_init(tags);
	virt_init(tags);
	mmu_init(tags);
}
