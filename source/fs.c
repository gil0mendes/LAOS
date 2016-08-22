/**
* The MIT License (MIT)
*
* Copyright (c) 2014-2015 Gil Mendes <gil00mendes@gmail.com>
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
 * @brief               Filesystem support.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <config.h>
#include <device.h>
#include <fs.h>
#include <memory.h>

/** Probe a device for filesystems.
 * @param device        Device to probe.
 * @return              Pointer to mount if found, NULL if not. */
fs_mount_t *fs_probe(device_t *device) {
    builtin_foreach(BUILTIN_TYPE_FS, fs_ops_t, ops) {
        fs_mount_t *mount;
        status_t ret;

        ret = ops->mount(device, &mount);
        switch (ret) {
        case STATUS_SUCCESS:
            dprintf("fs: mounted %s on %s ('%s') (uuid: %s)\n", ops->name, device->name, mount->label, mount->uuid);

            mount->ops = ops;
            mount->device = device;

            return mount;
        case STATUS_UNKNOWN_FS:
        case STATUS_END_OF_FILE:
            /* End of file usually means no media. */
            break;
        default:
            dprintf("fs: error while probing device %s: %pS\n", device->name, ret);
            return NULL;
        }
    }

    return NULL;
}

/**
 * Open a handle to a directory entry.
 *
 * Opens a handle given an entry structure provided by fs_iterate(). This is
 * only valid on entry structures provided by that function, as the structure
 * is typically embedded inside some FS-specific structure which contains the
 * information needed to open the file.
 *
 * @param entry         Entry to open.
 * @param type          Required type of the entry, or FILE_TYPE_NONE for any.
 * @param _handle       Where to store pointer to opened handle.
 *
 * @return              Status code describing the result of the operation.
 */
status_t fs_open_entry(const fs_entry_t *entry, file_type_t type, fs_handle_t **_handle) {
    fs_handle_t *handle;
    status_t ret;

    /* Avoid reopening the same directory. */
    if (entry->name[0] == '.' && !entry->name[1]) {
        fs_retain(entry->owner);
        *_handle = entry->owner;
        return STATUS_SUCCESS;
    }

    if (!entry->owner->mount->ops->open_entry)
        return STATUS_NOT_SUPPORTED;

    ret = entry->owner->mount->ops->open_entry(entry, &handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (type != FILE_TYPE_NONE && handle->type != type)
    {
        fs_close(handle);
        return (type == FILE_TYPE_DIR) ? STATUS_NOT_DIR : STATUS_NOT_FILE;
    }

    *_handle = handle;
    return STATUS_SUCCESS;
}

/** Structure containing data for fs_open(). */
typedef struct fs_open_data {
    const char *name;                   /**< Name of entry being searched for. */
    status_t ret;                       /**< Result of opening the entry. */
    fs_handle_t *handle;                /**< Handle to found entry. */
} fs_open_data_t;

/** Directory iteration callback for fs_open().
 * @param entry         Entry that was found.
 * @param _data         Pointer to data structure.
 * @return              Whether to continue iteration. */
static bool fs_open_cb(const fs_entry_t *entry, void *_data) {
    fs_open_data_t *data = _data;
    int result;

    result = (entry->owner->mount->case_insensitive)
        ? strcasecmp(entry->name, data->name)
        : strcmp(entry->name, data->name);

    if (!result) {
        data->ret = fs_open_entry(entry, FILE_TYPE_NONE, &data->handle);
        return false;
    } else {
        return true;
    }
}

/**
 * Open a handle to a file/directory.
 *
 * Looks up a path and returns a handle to it. If the path is a relative path
 * (does not begin with a '/' or a '('), it will be looked up relative to the
 * specified source directory if one is provided, or the working directory of
 * the current environment if not.
 *
 * An absolute path either begins with a '/' character, or a device specifier
 * in the form "(<device name>)" followed by a '/'. If no device specifier is
 * included on an absolute path, the lookup will take place from the root of the
 * current device.
 *
 * @param path          Path to entry to open.
 * @param from          If not NULL, a directory to look up relative to.
 * @param type          Required type of the entry, or FILE_TYPE_NONE for any.
 * @param _handle       Where to store pointer to handle.
 *
 * @return              Status code describing the result of the operation.
 */
status_t fs_open(const char *path, fs_handle_t *from, file_type_t type, fs_handle_t **_handle) {
    char *orig __cleanup_free;
    char *dup, *tok;
    device_t *device;
    fs_mount_t *mount;
    fs_handle_t *handle;
    status_t ret;

    /* Duplicate the path string so we can modify it. */
    dup = orig = strdup(path);

    if (dup[0] == '(') {
        dup++;
        tok = strsep(&dup, ")");
        if (!tok || !tok[0] || dup[0] != '/')
            return STATUS_INVALID_ARG;

        device = device_lookup(tok);
        if (!device || !device->mount)
            return STATUS_NOT_FOUND;

        mount = device->mount;
    } else if (from) {
        mount = from->mount;
    } else {
        device = (current_environ) ? current_environ->device : boot_device;
        if (!device || !device->mount)
            return STATUS_NOT_FOUND;

        mount = device->mount;
    }

    if (dup[0] == '/') {
        from = mount->root;

        /* Strip leading / characters from the path. */
        while (*dup == '/')
            dup++;
    } else if (!from) {
        from = (current_environ && current_environ->directory)
            ? current_environ->directory
            : mount->root;
    }

    /* If an open_path() implementation is provided, use it. */
    if (mount->ops->open_path) {
        ret = mount->ops->open_path(mount, dup, from, &handle);
        if (ret != STATUS_SUCCESS)
            return ret;
    } else {
        handle = from;
        fs_retain(handle);

        assert(from->mount == mount);
        assert(mount->ops->iterate);
        assert(mount->ops->open_entry);

        /* Loop through each element of the path string. */
        while (true) {
            fs_open_data_t data;

            tok = strsep(&dup, "/");
            if (!tok) {
                /* The last token was the last element of the path string,
                 * return the handle we're currently on. */
                break;
            } else if (handle->type != FILE_TYPE_DIR) {
                /* The previous node was not a directory: this means the path
                 * string is trying to treat a non-directory as a directory.
                 * Reject this. */
                fs_close(handle);
                return STATUS_NOT_DIR;
            } else if (!tok[0] || (tok[0] == '.' && !tok[1])) {
                /* Zero-length path component or current directory, do nothing. */
                continue;
            }

            /* Search the directory for the entry. */
            data.name = tok;
            data.ret = STATUS_NOT_FOUND;
            ret = mount->ops->iterate(handle, fs_open_cb, &data);

            fs_close(handle);

            if (ret == STATUS_SUCCESS)
                ret = data.ret;
            if (ret != STATUS_SUCCESS)
                return ret;

            handle = data.handle;
        }
    }

    if (type != FILE_TYPE_NONE && handle->type != type)
    {
        fs_close(handle);
        return (type == FILE_TYPE_DIR) ? STATUS_NOT_DIR : STATUS_NOT_FILE;
    }

    *_handle = handle;
    return STATUS_SUCCESS;
}

/** Close a filesystem handle.
 * @param handle        Handle to close. */
void fs_close(fs_handle_t *handle) {
    assert(handle->count);

    handle->count--;
    if (!handle->count) {
        if (handle->mount->ops->close)
            handle->mount->ops->close(handle);

        free(handle);
    }
}

/** Read from a file.
 * @param handle        Handle to the file.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset into the file.
 * @return              Status code describing the result of the operation. */
status_t fs_read(fs_handle_t *handle, void *buf, size_t count, offset_t offset) {
    if (handle->type != FILE_TYPE_REGULAR)
    {
        return STATUS_NOT_FILE;
    }

    if (offset + count > handle->size)
    {
        return STATUS_END_OF_FILE;
    }

    if (!count)
    {
        return STATUS_SUCCESS;
    }

    return handle->mount->ops->read(handle, buf, count, offset);
}

/** Iterate over entries in a directory.
 * @param handle        Handle to directory.
 * @param cb            Callback to call on each entry.
 * @param arg           Data to pass to callback.
 * @return              Status code describing the result of the operation. */
status_t fs_iterate(fs_handle_t *handle, fs_iterate_cb_t cb, void *arg) {
    if (handle->type != FILE_TYPE_DIR)
    {
        return STATUS_NOT_DIR;
    } else if (!handle->mount->ops->iterate)
    {
        return STATUS_NOT_SUPPORTED;
    }

    return handle->mount->ops->iterate(handle, cb, arg);
}

/**
 * Configuration commands.
 */

/** Set the current directory.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_cd(value_list_t *args) {
    fs_handle_t *handle __cleanup_close = NULL;
    const char *path;
    status_t ret;

    if (args->count != 1 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    path = args->values[0].string;

    ret = fs_open(path, NULL, FILE_TYPE_DIR, &handle);
    if (ret != STATUS_SUCCESS)
    {
        config_error("Error opening directory '%s': %pS", path, ret);
        return false;
    } else if (handle->mount->device != current_environ->device)
    {
        config_error("'%s' is on a different device", path);
        return false;
    }

    swap(current_environ->directory, handle);
    return true;
}

BUILTIN_COMMAND("cd", "Set the current directory", config_cmd_cd);

/**
 * Directory list iteration callback.
 *
 * @param entry         Entry that was found.
 * @param arg           Unused.
 * @return              Whether to continue iteration.
 */
static bool config_cmd_ls_cb(const fs_entry_t *entry, void *arg) {
    fs_handle_t *handle __cleanup_close = NULL;
    status_t ret;

    ret = fs_open_entry(entry, FILE_TYPE_NONE, &handle);
    if (ret != STATUS_SUCCESS)
    {
        printf("ls: warning: Failed to open entry '%s'\n", entry->name);
        return true;
    }

    printf(
        "%-5s %-10" PRIu64 " %s\n",
        (handle->type == FILE_TYPE_DIR) ? "Dir" : "File", handle->size, entry->name);

    return true;
}

/**
 * List the contents of a directory.
 *
 * @param args          Argument list.
 * @return              Whether successful.
 */
static bool config_cmd_ls(value_list_t *args) {
    const char *path;
    fs_handle_t *handle __cleanup_close = NULL;
    status_t ret;

    if (args->count == 0)
    {
        path = ".";
    } else if (args->count == 1 && args->values[0].type == VALUE_TYPE_STRING)
    {
        path = args->values[0].string;
    } else
    {
        config_error("Invalid arguments");
        return false;
    }

    ret = fs_open(path, NULL, FILE_TYPE_DIR, &handle);
    if (ret != STATUS_SUCCESS)
    {
        config_error("Error opening directory '%s': %pS", path, ret);
        return false;
    }

    printf("F/D   Size       Name\n");
    printf("---   ----       ----\n");

    ret = fs_iterate(handle, config_cmd_ls_cb, NULL);
    if (ret != STATUS_SUCCESS)
    {
        config_error("Error iterating directory '%s': %pS", path, ret);
        return false;
    }

    return true;
}

BUILTIN_COMMAND("ls", "List the contents of a directory", config_cmd_ls);

/** Size of the read buffer for cat. */
#define CAT_READ_SIZE       512

/**
 * Read the contents of one or more files.
 *
 * @param args          Argument list.
 * @return              Whether successful.
 */
static bool config_cmd_cat(value_list_t *args) {
    char *buf __cleanup_free = NULL;

    if (!args->count)
    {
        config_error("Invalid arguments");
        return false;
    }

    buf = malloc(CAT_READ_SIZE + 1);

    for (size_t i = 0; i < args->count; i++)
    {
        const char *path;
        fs_handle_t *handle __cleanup_close = NULL;
        offset_t offset;
        status_t ret;

        if (args->values[i].type != VALUE_TYPE_STRING)
        {
            config_error("Invalid arguments");
            return false;
        }

        path = args->values[i].string;
        ret = fs_open(path, NULL, FILE_TYPE_REGULAR, &handle);
        if (ret != STATUS_SUCCESS)
        {
            config_error("Error opening file '%s': %pS", path, ret);
            return false;
        }

        offset = 0;
        while (offset < handle->size)
        {
            size_t size = min(CAT_READ_SIZE, handle->size - offset);

            ret = fs_read(handle, buf, size, offset);
            if (ret != STATUS_SUCCESS)
            {
                config_error("Error reading file '%s': %pS", path, ret);
                return false;
            }

            buf[size] = 0;

            printf("%s", buf);

            offset += size;
        }
    }

    return true;
}

BUILTIN_COMMAND("cat", "Output the contents of one or more files", config_cmd_cat);
