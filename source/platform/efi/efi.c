/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2016 Gil Mendes <gil00mendes@gmail.com>
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
 * @brief             EFI boot services utility functions
 */

#include <efi/console.h>
#include <efi/disk.h>
#include <efi/efi.h>
#include <efi/memory.h>
#include <efi/net.h>
#include <efi/video.h>

#include <lib/charset.h>
#include <lib/string.h>

#include <console.h>
#include <loader.h>
#include <memory.h>

/** Various protocol GUIDs */
static efi_guid_t device_path_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
static efi_guid_t device_path_to_text_guid = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
static efi_guid_t loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

// Device path to text protocol
static efi_device_path_to_text_protocol_t *device_path_to_text;

/**
 * Convert an EFI status code to an internal status.
 *
 * @param  status Status to convert
 * @return        Internal status code
 */
status_t efi_convert_status(efi_status_t status)
{
	switch (status) {
	case EFI_SUCCESS:
		return STATUS_SUCCESS;
	case EFI_UNSUPPORTED:
		return STATUS_NOT_SUPPORTED;
	case EFI_INVALID_PARAMETER:
		return STATUS_INVALID_ARG;
	case EFI_DEVICE_ERROR:
	case EFI_NO_MEDIA:
	case EFI_MEDIA_CHANGED:
		return STATUS_DEVICE_ERROR;
	case EFI_WRITE_PROTECTED:
		return STATUS_READ_ONLY;
	case EFI_VOLUME_CORRUPTED:
		return STATUS_CORRUPT_FS;
	case EFI_NOT_FOUND:
		return STATUS_NOT_FOUND;
	case EFI_TIMEOUT:
		return STATUS_TIMED_OUT;
	default:
		return STATUS_SYSTEM_ERROR;
	}
}

/**
 * Memory allocation services.
 */

/**
 * Allocate EFI pool memory.
 *
 * @param pool_type         Pool memory type to allocate.
 * @param size              Size of memory to allocate.
 * @param _buffer           Where to store pointer to allocated memory.
 * @return                  EFI status code
 */
efi_status_t efi_allocate_pool(efi_memory_type_t pool_type, efi_uintn_t size, void **_buffer)
{
	return efi_call(efi_boot_services->allocate_pool, pool_type, size, _buffer);
}

/**
 * Free EFI pool memory.
 *
 * @param buffer            Pointer to memory to free.
 * @return                  EFI status code
 */
efi_status_t efi_free_pool(void *buffer)
{
	return efi_call(efi_boot_services->free_pool, buffer);
}

/**
 * Get the current memory map.
 *
 * Gets a copy of the current memory map. This function is a wrapper for the
 * EFI GetMemoryMap boot service which handles allocation of an appropriately
 * sized buffer, and ensures that the array entries are contiguous (the
 * descriptor size returned by the firmware can change in future).
 *
 * @param _memory_map   Where to store pointer to memory map.
 * @param _num_entries  Where to store number of entries in memory map.
 * @param _map_key      Where to store the key for the current memory map.
 *
 * @return              EFI status code.
 */
efi_status_t efi_get_memory_map(
	efi_memory_descriptor_t **_memory_map, efi_uintn_t *_num_entries,
	efi_uintn_t *_map_key)
{
	efi_memory_descriptor_t *memory_map = NULL, *orig;
	efi_uintn_t size = 0, descriptor_size, num_entries, i;
	efi_uint32_t descriptor_version;
	efi_status_t ret;

	/* Call a first time to get the needed buffer size. */
	ret = efi_call(
		efi_boot_services->get_memory_map,
		&size, memory_map, _map_key, &descriptor_size, &descriptor_version);
	if (ret != EFI_SUCCESS && ret != EFI_BUFFER_TOO_SMALL)
		return ret;

	num_entries = size / descriptor_size;

	if (ret == EFI_BUFFER_TOO_SMALL) {
		memory_map = malloc(size);

		ret = efi_call(
			efi_boot_services->get_memory_map,
			&size, memory_map, _map_key, &descriptor_size, &descriptor_version);
		if (ret != EFI_SUCCESS) {
			free(memory_map);
			return ret;
		}

		if (descriptor_size != sizeof(*memory_map)) {
			orig = memory_map;
			memory_map = malloc(num_entries * sizeof(*memory_map));

			for (i = 0; i < num_entries; i++) {
				memcpy(&memory_map[i],
				       (void*)orig + (descriptor_size * i),
				       min(descriptor_size, sizeof(*memory_map)));
			}

			free(orig);
		}
	}

	*_memory_map = memory_map;
	*_num_entries = num_entries;
	return ret;
}

/**
 * Protocol handler services.
 */

//============================================================================
// Return an array of handles that support a protocol.
//
// Returns an array of handles that support a specified protocol. This is a
// wrapper for the EFI LocateHandle boot service that handles the allocation
// of a sufficiently sized buffer. The returned buffer should be freed with
// free() once it is no longer needed.
//
// @param search_type  Specifies which handles are to be returned.
// @param protocol     The protocol to search for.
// @param search_key   Search key.
// @param _handles     Where to store pointer to handle array.
// @param _num_handles Where to store the number of handles returned.
//
// @return             EFI status code.
efi_status_t
efi_locate_handle(
	efi_locate_search_type_t search_type, efi_guid_t *protocol,
	void *search_key, efi_handle_t **_handles, efi_uintn_t *_num_handles)
{
	efi_handle_t *handles = NULL;
	efi_uintn_t size = 0;
	efi_status_t ret;

	// Call a first time to get the needed buffer size
	ret = efi_call(efi_boot_services->locate_handle,
		       search_type, protocol, search_key, &size, handles);
	if (ret == EFI_BUFFER_TOO_SMALL) {
		handles = malloc(size);

		ret = efi_call(efi_boot_services->locate_handle,
			       search_type, protocol, search_key, &size, handles);
		if (ret != EFI_SUCCESS)
			free(handles);
	}

	*_handles = handles;
	*_num_handles = size / sizeof(efi_handle_t);
	return ret;
}

// ============================================================================
// Open a protocol supported by a handle.
//
// This function is a wrapper for the EFI OpenProtocol boot service which
// passes the correct values for certain arguments.
//
// @param handle       Handle to open on.
// @param protocol     Protocol to open.
// @param attributes   Open mode of the protocol interface.
// @param _interface   Where to store pointer to opened interface.
//
// @return             EFI status code.
efi_status_t
efi_open_protocol(
	efi_handle_t handle, efi_guid_t *protocol, efi_uint32_t attributes,
	void **interface)
{
	return efi_call(efi_boot_services->open_protocol, handle,
			protocol, interface, efi_image_handle, NULL, attributes);
}

/**
 * Image services.
 */

/** Exit the loader.
 * @param status        Exit status.
 * @param data          Pointer to null-terminated string giving exit reason.
 * @param data_size     Size of the exit data in bytes. */
__noreturn void efi_exit(efi_status_t status, efi_char16_t *data, efi_uintn_t data_size)
{
	efi_status_t ret;

	/* Reset everything to default state. */
	efi_video_reset();
	efi_console_reset();
	efi_memory_cleanup();

	ret = efi_call(efi_boot_services->exit, efi_image_handle, status, data_size, data);
	internal_error("EFI exit failed (0x%zx)", ret);
}

/**
 * Exit boot services.
 *
 * Exit EFI boot services mode and return the final memory map. After this
 * function has completed no I/O can be performed, and the debug console will
 * be disabled as it may be driven by an EFI driver.
 *
 * @param _memory_map   Where to store pointer to memory map.
 * @param _num_entries  Where to store number of entries in memory map.
 * @param _desc_size    Where to store descriptor size.
 * @param _desc_version Where to store descriptor version.
 */
void efi_exit_boot_services(
	void **_memory_map, efi_uintn_t *_num_entries, efi_uintn_t *_desc_size,
	efi_uint32_t *_desc_version)
{
	efi_status_t ret;

	/* Try multiple times to call ExitBootServices, it can change the memory map
	 * the first time. This should not happen more than once however, so only
	 * do it twice. */
	for (unsigned i = 0; i < 2; i++) {
		efi_uintn_t size, map_key, desc_size;
		efi_uint32_t desc_version;
		void *buf;

		/* Call a first time to get the needed buffer size. */
		size = 0;
		ret = efi_call(efi_boot_services->get_memory_map, &size, NULL, &map_key, &desc_size, &desc_version);
		if (ret != EFI_BUFFER_TOO_SMALL)
			internal_error("Failed to get memory map size (0x%zx)", ret);

		buf = malloc(size);

		ret = efi_call(efi_boot_services->get_memory_map, &size, buf, &map_key, &desc_size, &desc_version);
		if (ret != EFI_SUCCESS)
			internal_error("Failed to get memory map (0x%zx)", ret);

		/* Try to exit boot services. */
		ret = efi_call(efi_boot_services->exit_boot_services, efi_image_handle, map_key);
		if (ret == EFI_SUCCESS) {
			/* Disable the debug console, it could now be invalid. FIXME: Only
			 * do this if the debug console is an EFI serial console. */
			console_set_debug(NULL);

			*_memory_map = buf;
			*_num_entries = size / desc_size;
			*_desc_size = desc_size;
			*_desc_version = desc_version;
			return;
		}

		free(buf);
	}

	internal_error("Failed to exit boot services (0x%zx)", ret);
}

/**
 * Get the loaded image protocol from an image handle.
 *
 * @param handle        Image handle.
 * @param _image        Where to store loaded image pointer.
 * @return              EFI status code.
 */
efi_status_t efi_get_loaded_image(efi_handle_t handle, efi_loaded_image_t **_image)
{
	return efi_open_protocol(handle, &loaded_image_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, (void**)_image);
}

/**
 * Device utility functions.
 */

/**
 * Open the device path protocol for a handle.
 *
 * @param handle            Handle to open for.
 * @return                  Pointer to device path protocol on success,
 *                          NULL on failure.
 */
efi_device_path_t *efi_get_device_path(efi_handle_t handle)
{
	efi_device_path_t *path;
	efi_status_t ret;

	ret = efi_open_protocol(handle, &device_path_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, (void**)&path);
	if (ret != EFI_SUCCESS) {
		return NULL;
	}

	if (path->type == EFI_DEVICE_PATH_TYPE_END) {
		return NULL;
	}

	return path;
}

/**
 * Helper to print a string representation of a device path.
 *
 * @param path          Device path protocol to print.
 * @param cb            Helper function to print with
 * @param data          Data to pass to helper function
 */
void efi_print_device_path(efi_device_path_t *path, void (*cb)(void *data, char ch), void *data)
{
	efi_char16_t *str;
	char *buf __cleanup_free = NULL;

	/* For now this only works on UEFI 2.0+, previous versions do not have the
	 * device path to text protocol. */
	if (!device_path_to_text) {
		efi_handle_t *handles __cleanup_free = NULL;
		efi_uintn_t num_handles;
		efi_status_t ret;

		/* Get the device path to text protocol. */
		ret = efi_locate_handle(EFI_BY_PROTOCOL, &device_path_to_text_guid, NULL, &handles, &num_handles);
		if (ret == EFI_SUCCESS) {
			efi_open_protocol(
				handles[0], &device_path_to_text_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL,
				(void**)&device_path_to_text);
		}
	}

	/* Get the device path string. */
	str = (path && device_path_to_text)
	      ? efi_call(device_path_to_text->convert_device_path_to_text, path, false, false)
	      : NULL;
	if (str) {
		size_t len = 0;

		while (str[len])
			len++;

		buf = malloc((len * MAX_UTF8_PER_UTF16) + 1);
		len = utf16_to_utf8((uint8_t*)buf, str, len);
		buf[len] = 0;

		efi_free_pool(str);
	} else {
		buf = strdup("Unknown");
	}

	for (size_t i = 0; buf[i]; i++)
		cb(data, buf[i]);
}

/**
 * Determine if a device path is a child of another.
 *
 * @param  parent Parent device path
 * @param  child  Child device path
 * @return        Whether child is a child of parent
 */
bool efi_is_child_device_node(efi_device_path_t *parent, efi_device_path_t *child)
{
	while (parent) {
		if (memcmp(child, parent, min(parent->length, child->length)) != 0) {
			return false;
		}

		parent = efi_next_device_node(parent);
		child = efi_next_device_node(child);
	}

	return child != NULL;
}

/**
 * Gets an EFI handle from a device.
 *
 * If the given device is an EFI disk, a partition on an EFI disk, or an EFI
 * network device, tries to find a handle corresponding to that device.
 *
 * @param  device Device to get handle for.
 * @return        Handle to device, or NULL if not found.
 */
efi_handle_t efi_device_get_handle(device_t *device)
{
	switch (device->type) {
	case DEVICE_TYPE_DISK:
		return efi_disk_get_handle((disk_device_t*)device);
	case DEVICE_TYPE_NET:
		return efi_net_get_handle((net_device_t*)device);
	default:
		return NULL;
	}
}
