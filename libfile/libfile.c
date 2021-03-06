/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    libfile.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-07-22 14:17:17
 * updated: 2016-07-22 14:17:17
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "libfile.h"

/*
 * Most of these MAGIC constants are defined in /usr/include/linux/magic.h,
 * and some are hardcoded in kernel sources.
 */
typedef enum fs_type_supported {
    FS_CIFS     = 0xFF534D42,
    FS_CRAMFS   = 0x28cd3d45,
    FS_DEBUGFS  = 0x64626720,
    FS_DEVFS    = 0x1373,
    FS_DEVPTS   = 0x1cd1,
    FS_EXT      = 0x137D,
    FS_EXT2_OLD = 0xEF51,
    FS_EXT2     = 0xEF53,
    FS_EXT3     = 0xEF53,
    FS_EXT4     = 0xEF53,
    FS_FUSE     = 0x65735546,
    FS_JFFS2    = 0x72b6,
    FS_MQUEUE   = 0x19800202,
    FS_MSDOS    = 0x4d44,
    FS_NFS      = 0x6969,
    FS_NTFS     = 0x5346544e,
    FS_PROC     = 0x9fa0,
    FS_RAMFS    = 0x858458f6,
    FS_ROMFS    = 0x7275,
    FS_SELINUX  = 0xf97cff8c,
    FS_SMB      = 0x517B,
    FS_SOCKFS   = 0x534F434B,
    FS_SQUASHFS = 0x73717368,
    FS_SYSFS    = 0x62656572,
    FS_TMPFS    = 0x01021994
} fs_type_supported_t;

static struct {
    const char name[32];
    const int value;
} fs_type_info[] = {
    {"CIFS    ", FS_CIFS    },
    {"CRAMFS  ", FS_CRAMFS  },
    {"DEBUGFS ", FS_DEBUGFS },
    {"DEVFS   ", FS_DEVFS   },
    {"DEVPTS  ", FS_DEVPTS  },
    {"EXT     ", FS_EXT     },
    {"EXT2_OLD", FS_EXT2_OLD},
    {"EXT2    ", FS_EXT2    },
    {"EXT3    ", FS_EXT3    },
    {"EXT4    ", FS_EXT4    },
    {"FUSE    ", FS_FUSE    },
    {"JFFS2   ", FS_JFFS2   },
    {"MQUEUE  ", FS_MQUEUE  },
    {"MSDOS   ", FS_MSDOS   },
    {"NFS     ", FS_NFS     },
    {"NTFS    ", FS_NTFS    },
    {"PROC    ", FS_PROC    },
    {"RAMFS   ", FS_RAMFS   },
    {"ROMFS   ", FS_ROMFS   },
    {"SELINUX ", FS_SELINUX },
    {"SMB     ", FS_SMB     },
    {"SOCKFS  ", FS_SOCKFS  },
    {"SQUASHFS", FS_SQUASHFS},
    {"SYSFS   ", FS_SYSFS   },
    {"TMPFS   ", FS_TMPFS   },
};

extern const struct file_ops io_ops;
extern const struct file_ops fio_ops;


static const struct file_ops *file_ops[] = {
    &io_ops,
    &fio_ops,
    NULL
};

#define SIZEOF(array)       (sizeof(array)/sizeof(array[0]))

static file_backend_type backend = FILE_BACKEND_IO;

void file_backend(file_backend_type type)
{
    backend = type;
}

struct file *file_open(const char *path, file_open_mode_t mode)
{
    struct file *file = (struct file *)calloc(1, sizeof(struct file));
    if (!file) {
        printf("malloc failed!\n");
        return NULL;
    }
    file->ops = file_ops[backend];
    file->fd = file->ops->open(path, mode);
    return file;
}

void file_close(struct file *file)
{
    return file->ops->close(file->fd);
}

ssize_t file_read(struct file *file, void *data, size_t size)
{
    return file->ops->read(file->fd, data, size);
}

ssize_t file_write(struct file *file, const void *data, size_t size)
{
    return file->ops->write(file->fd, data, size);
}

ssize_t file_size(struct file *file)
{
    return file->ops->size(file->fd);
}

int file_sync(struct file *file)
{
    return file->ops->sync(file->fd);
}

off_t file_seek(struct file *file, off_t offset, int whence)
{
    return file->ops->seek(file->fd, offset, whence);
}

ssize_t file_get_size(const char *path)
{
    struct stat st;
    off_t size = 0;
    if (stat(path, &st) < 0) {
        printf("%s stat error: %s\n", path, strerror(errno));
    } else {
        size = st.st_size;
    }
    return size;
}

struct iovec *file_dump(const char *path)
{
    ssize_t size = file_get_size(path);
    if (size == 0) {
        return NULL;
    }
    struct iovec *buf = (struct iovec *)calloc(1, sizeof(struct iovec));
    if (!buf) {
        printf("malloc failed!\n");
        return NULL;
    }
    buf->iov_len = size;
    buf->iov_base = calloc(1, buf->iov_len);
    if (!buf->iov_base) {
        printf("malloc failed!\n");
        return NULL;
    }

    struct file *f = file_open(path, F_RDONLY);
    if (!f) {
        printf("file open failed!\n");
        free(buf->iov_base);
        free(buf);
        return NULL;
    }
    file_read(f, buf->iov_base, buf->iov_len);
    file_close(f);
    return buf;
}


struct file_systat *file_get_systat(const char *path)
{
    if (!path) {
        printf("path can't be null\n");
        return NULL;
    }
    struct statfs stfs;
    if (-1 == statfs(path, &stfs)) {
        printf("statfs %s failed: %s\n", path, strerror(errno));
        return NULL;
    }
    struct file_systat *fi = (struct file_systat *)calloc(1,
                    sizeof(struct file_systat));
    if (!fi) {
        printf("malloc failed!\n");
        return NULL;
    }
    fi->size_total = stfs.f_bsize * stfs.f_blocks;
    fi->size_avail = stfs.f_bsize * stfs.f_bavail;
    fi->size_free  = stfs.f_bsize * stfs.f_bfree;
    for (int i = 0; i < SIZEOF(fs_type_info); i++) {
        if (stfs.f_type == fs_type_info[i].value) {
            stfs.f_type = i;
            strncpy(fi->fs_type_name, fs_type_info[i].name,
                            sizeof(fi->fs_type_name));
            break;
        }
    }
    return fi;
}
