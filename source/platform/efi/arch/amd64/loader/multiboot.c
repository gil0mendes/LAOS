/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Gil Mendes <gil00mendes@gmail.com>
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
 * @brief               EFI Multiboot loader functions.
 *
 * TODO:
 *  - Video mode setting.
 */

 #include <efi/services.h>

 #include <lib/string.h>
 #include <lib/utility.h>

 #include <x86/multiboot.h>

 #include <assert.h>
 #include <loader.h>
 #include <memory.h>

/** Get platform-specific Multiboot information.
 * @param loader        Loader internal data. */
void multiboot_platform_load(multiboot_loader_t *loader)
{
	void *efi_mmap __cleanup_free;
	efi_uintn_t efi_entries, desc_size;
	efi_uint32_t desc_version;
	multiboot_mmap_entry_t *mmap __cleanup_free = NULL;
	size_t offset;
	void *dest;

	/* Multiboot requires an E820-style memory map. Exit boot services mode to
	 * get the final memory map and then we convert it into a E820 format. */
	efi_exit_boot_services(&efi_mmap, &efi_entries, &desc_size, &desc_version);

	offset = 0;
	for (efi_uintn_t i = 0; i < efi_entries; i++) {
		efi_memory_descriptor_t *desc = efi_mmap + (i * desc_size);
		uint32_t type;

		switch (desc->type) {
		case EFI_LOADER_CODE:
		case EFI_LOADER_DATA:
		case EFI_BOOT_SERVICES_CODE:
		case EFI_BOOT_SERVICES_DATA:
		case EFI_CONVENTIONAL_MEMORY:
			type = MULTIBOOT_MMAP_FREE;
			break;
		case EFI_UNUSABLE_MEMORY:
			type = MULTIBOOT_MMAP_BAD;
			break;
		case EFI_ACPI_RECLAIM_MEMORY:
			type = MULTIBOOT_MMAP_ACPI_RECLAIM;
			break;
		case EFI_ACPI_MEMORY_NVS:
			type = MULTIBOOT_MMAP_ACPI_NVS;
			break;
		default:
			type = MULTIBOOT_MMAP_RESERVED;
			break;
		}

		/* Coalesce adjacent entries of same type. */
		if (mmap && mmap[offset].type == type && desc->physical_start == mmap[offset].addr + mmap[offset].len) {
			mmap[offset].len += desc->num_pages * EFI_PAGE_SIZE;
		} else {
			if (mmap)
				offset++;

			mmap = realloc(mmap, sizeof(*mmap) * (offset + 1));
			mmap[offset].size = sizeof(*mmap) - 4;
			mmap[offset].addr = desc->physical_start;
			mmap[offset].len = desc->num_pages * EFI_PAGE_SIZE;
			mmap[offset].type = type;
		}
	}

	/* Copy the final memory map into the info area. */
	loader->info->flags |= MULTIBOOT_INFO_MEMORY | MULTIBOOT_INFO_MEM_MAP;
	loader->info->mmap_length = sizeof(*mmap) * (offset + 1);
	dest = multiboot_alloc_info(loader, loader->info->mmap_length, &loader->info->mmap_addr);
	memcpy(dest, mmap, loader->info->mmap_length);

	/* Get upper/lower memory information. */
	for (size_t i = 0; i <= offset; i++) {
		if (mmap[i].type == MULTIBOOT_MMAP_FREE) {
			if (mmap[i].addr <= 0x100000 && mmap[i].addr + mmap[i].len > 0x100000) {
				loader->info->mem_upper = (mmap[i].addr + mmap[i].len - 0x100000) / 1024;
			} else if (mmap[i].addr == 0) {
				loader->info->mem_lower = min(mmap[i].len, 0x100000) / 1024;
			}
		}
	}
}