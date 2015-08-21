/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2012-2015 Gil Mendes
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
 * @brief       x86 Initium kernel loader.
 */

#include <arch/page.h>

#include <lib/string.h>

#include <loader/initium.h>

#include <x86/cpu.h>
#include <x86/mmu.h>

#include <loader.h>

/** Entry arguments for the kernel. */
typedef struct entry_args {
    uint64_t trampoline_cr3;             /**< Trampoline address space CR3. */
    uint64_t trampoline_virt;            /**< Virtual location of trampoline. */
    uint64_t kernel_cr3;                 /**< Kernel address space CR3. */
    uint64_t sp;                         /**< Stack pointer for the kernel. */
    uint64_t entry;                      /**< Entry point for kernel. */
    uint64_t tags;                       /**< Tag list virtual address. */

    char trampoline[];
} entry_args_t;

extern void initium_arch_enter_64(entry_args_t *args) __noreturn;
extern void initium_arch_enter_32(entry_args_t *args) __noreturn;

extern char initium_trampoline_64[], initium_trampoline_32[];
extern uint32_t initium_trampoline_64_size, initium_trampoline_32_size;

/**
 * Check whether a kernel image is supported.
 *
 * @param loader        Loader internal data.
 */
void initium_arch_check_kernel(initium_loader_t *loader) {
    /* Check if long mode is supported if the kernel is 64-bit. */
    if (loader->mode == LOAD_MODE_64BIT) {
	    x86_cpuid_t cpuid;

	    x86_cpuid(X86_CPUID_EXT_MAX, &cpuid);
	    if (cpuid.eax & (1 << 31)) {
		    x86_cpuid(X86_CPUID_EXT_FEATURE, &cpuid);
		    if (cpuid.edx & X86_EXT_FEATURE_LM) {
			    return;
			}
		}

	    boot_error("64-bit kernel requires 64-bit CPU");
	}
}

/**
 * Validate kernel load parameters.
 *
 * @param loader        Loader internal data.
 * @param load          Load image tag.
 */
void initium_arch_check_load_params(initium_loader_t *loader, initium_itag_load_t *load) {
    if (!(load->flags & INITIUM_LOAD_FIXED) && !load->alignment) {
	    /* Set default alignment parameters. Try to align to the large page size
	     * so we can map using large pages, but fall back to 1MB if we're tight
	     * on memory. */
	    load->alignment = (loader->mode == LOAD_MODE_64BIT) ? LARGE_PAGE_SIZE_64 : LARGE_PAGE_SIZE_32;
	    load->min_alignment = 0x100000;
	}

    if (loader->mode == LOAD_MODE_64BIT) {
	    if (load->virt_map_base || load->virt_map_size) {
		    if (!is_canonical_range(load->virt_map_base, load->virt_map_size)) {
                boot_error("Kernel specifies invalid virtual map range");
            }
		} else {
		    /* On 64-bit we can't default to the whole 48-bit address space so
		     * just use the bottom half. */
		    load->virt_map_base = 0;
		    load->virt_map_size = 0x800000000000ull;
		}
	}
}

/**
 * Perform architecture-specific setup tasks.
 *
 * @param loader        Loader internal data.
 */
void initium_arch_setup(initium_loader_t *loader) {
    initium_itag_load_t *load = loader->load;
    unsigned i, vm_start, vm_end;

    /* Find a location to recursively map the pagetables at. */
    if (loader->mode == LOAD_MODE_64BIT) {
	    uint64_t *pml4 = (uint64_t *)phys_to_virt(loader->mmu->cr3);

	    /* Search back from the end of the address space for a free region,
	     * avoiding the virtual map area and any existing allocations. */
	    vm_start = (load->virt_map_base / X86_PDPT_RANGE_64) % 512;
	    vm_end = ((load->virt_map_base + load->virt_map_size - 1) / X86_PDPT_RANGE_64) % 512;
	    i = 512;
	    while (i--) {
		    if (!(pml4[i] & X86_PTE_PRESENT) && (i < vm_start || i > vm_end)) {
			    initium_tag_pagetables_amd64_t *tag;

			    pml4[i] = loader->mmu->cr3 | X86_PTE_PRESENT | X86_PTE_WRITE;

			    tag = initium_alloc_tag(loader, INITIUM_TAG_PAGETABLES, sizeof(*tag));
			    tag->pml4 = loader->mmu->cr3;
			    tag->mapping = i * X86_PDPT_RANGE_64 | ((i >= 256) ? 0xffff000000000000ull : 0);

			    dprintf("initium: recursive PML4 mapping at 0x%" PRIx64 "\n", tag->mapping);
			    return;
			}
		}
	} else {
	    uint32_t *pdir = (uint32_t *)phys_to_virt(loader->mmu->cr3);

	    vm_start = load->virt_map_base / X86_PTBL_RANGE_32;
	    vm_end = (load->virt_map_base + load->virt_map_size - 1) / X86_PTBL_RANGE_32;
	    i = 1024;
	    while (i--) {
		    if (!(pdir[i] & X86_PTE_PRESENT) && (i < vm_start || i > vm_end)) {
			    initium_tag_pagetables_ia32_t *tag;

			    pdir[i] = loader->mmu->cr3 | X86_PTE_PRESENT | X86_PTE_WRITE;

			    tag = initium_alloc_tag(loader, INITIUM_TAG_PAGETABLES, sizeof(*tag));
			    tag->page_dir = loader->mmu->cr3;
			    tag->mapping = i * X86_PTBL_RANGE_32;

			    dprintf("initium: recursive page directory mapping at 0x%" PRIx64 "\n", tag->mapping);
			    return;
			}
		}
	}

    boot_error("Unable to allocate page table mapping space");
}

/**
 * Enter the kernel.
 *
 * @param loader        Loader internal data.
 */
__noreturn void initium_arch_enter(initium_loader_t *loader) {
    entry_args_t *args;

    /* Enter with interrupts disabled. */
    __asm__ volatile ("cli");

    /* Flush cached data to memory. This is needed to ensure that the log buffer
     * set up is written to memory and can be detected again after a reset. */
    __asm__ volatile ("wbinvd");

    /* Store information for the entry code. */
    args = (entry_args_t *)phys_to_virt(loader->trampoline_phys);
    args->trampoline_cr3 = loader->trampoline_mmu->cr3;
    args->trampoline_virt = loader->trampoline_virt;
    args->kernel_cr3 = loader->mmu->cr3;
    args->sp = loader->core->stack_base + loader->core->stack_size;
    args->entry = loader->entry;
    args->tags = loader->tags_virt;

    /* Copy the trampoline and call the entry code. */
    if (loader->mode == LOAD_MODE_64BIT) {
	    memcpy(args->trampoline, initium_trampoline_64, initium_trampoline_64_size);
	    initium_arch_enter_64(args);
	} else {
	    memcpy(args->trampoline, initium_trampoline_32, initium_trampoline_32_size);
	    initium_arch_enter_32(args);
	}
}
