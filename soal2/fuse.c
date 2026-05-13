#define FUSE_USE_VERSION 31
#define _GNU_SOURCE

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <limits.h>

#define XOR_KEY 0x76

static char storage_root[PATH_MAX];

static void xor_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] ^= XOR_KEY;
    }
}

static int has_enc_suffix(const char *name) {
    size_t len = strlen(name);
    return len > 4 && strcmp(name + len - 4, ".enc") == 0;
}

static void remove_enc_suffix(char *name) {
    size_t len = strlen(name);
    if (len > 4 && strcmp(name + len - 4, ".enc") == 0) {
        name[len - 4] = '\0';
    }
}

static int build_dir_path(char out[PATH_MAX], const char *path) {
    int written;

    if (strcmp(path, "/") == 0) {
        written = snprintf(out, PATH_MAX, "%s", storage_root);
    } else {
        written = snprintf(out, PATH_MAX, "%s%s", storage_root, path);
    }

    if (written < 0 || written >= PATH_MAX) {
        return -ENAMETOOLONG;
    }

    return 0;
}

static int build_file_path(char out[PATH_MAX], const char *path) {
    int written = snprintf(out, PATH_MAX, "%s%s.enc", storage_root, path);

    if (written < 0 || written >= PATH_MAX) {
        return -ENAMETOOLONG;
    }

    return 0;
}

static int path_exists(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

static int moo_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    char real_path[PATH_MAX];

    int res = build_dir_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (lstat(real_path, stbuf) == 0) {
        return 0;
    }

    res = build_file_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (lstat(real_path, stbuf) == 0) {
        stbuf->st_mode &= ~S_IFMT;
        stbuf->st_mode |= S_IFREG;
        return 0;
    }

    return -ENOENT;
}

static int moo_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char real_dir[PATH_MAX];

    int res = build_dir_path(real_dir, path);
    if (res != 0) {
        return res;
    }

    DIR *dp = opendir(real_dir);
    if (dp == NULL) {
        return -errno;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        char visible_name[NAME_MAX + 1];
        snprintf(visible_name, sizeof(visible_name), "%s", de->d_name);

        if (has_enc_suffix(visible_name)) {
            remove_enc_suffix(visible_name);
        }

        filler(buf, visible_name, NULL, 0, 0);
    }

    closedir(dp);
    return 0;
}

static int moo_mkdir(const char *path, mode_t mode) {
    char real_path[PATH_MAX];

    int res = build_dir_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (mkdir(real_path, mode) == -1) {
        return -errno;
    }

    return 0;
}

static int moo_rmdir(const char *path) {
    char real_path[PATH_MAX];

    int res = build_dir_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (rmdir(real_path) == -1) {
        return -errno;
    }

    return 0;
}

static int moo_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];

    int res = build_file_path(real_path, path);
    if (res != 0) {
        return res;
    }

    int fd = open(real_path, fi->flags, mode);
    if (fd == -1) {
        return -errno;
    }

    fi->fh = fd;
    return 0;
}

static int moo_open(const char *path, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];

    int res = build_file_path(real_path, path);
    if (res != 0) {
        return res;
    }

    int fd = open(real_path, fi->flags);
    if (fd == -1) {
        return -errno;
    }

    fi->fh = fd;
    return 0;
}

static int moo_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    int fd;
    int close_after = 0;

    if (fi != NULL && fi->fh != 0) {
        fd = (int) fi->fh;
    } else {
        char real_path[PATH_MAX];

        int res = build_file_path(real_path, path);
        if (res != 0) {
            return res;
        }

        fd = open(real_path, O_RDONLY);
        if (fd == -1) {
            return -errno;
        }

        close_after = 1;
    }

    ssize_t res = pread(fd, buf, size, offset);
    if (res == -1) {
        int err = errno;
        if (close_after) {
            close(fd);
        }
        return -err;
    }

    xor_buffer(buf, (size_t) res);

    if (close_after) {
        close(fd);
    }

    return (int) res;
}

static int moo_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    int fd;
    int close_after = 0;

    if (fi != NULL && fi->fh != 0) {
        fd = (int) fi->fh;
    } else {
        char real_path[PATH_MAX];

        int res = build_file_path(real_path, path);
        if (res != 0) {
            return res;
        }

        fd = open(real_path, O_WRONLY);
        if (fd == -1) {
            return -errno;
        }

        close_after = 1;
    }

    char *tmp = malloc(size);
    if (tmp == NULL) {
        if (close_after) {
            close(fd);
        }
        return -ENOMEM;
    }

    memcpy(tmp, buf, size);
    xor_buffer(tmp, size);

    ssize_t res = pwrite(fd, tmp, size, offset);
    int saved_errno = errno;

    free(tmp);

    if (close_after) {
        close(fd);
    }

    if (res == -1) {
        return -saved_errno;
    }

    return (int) res;
}

static int moo_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    if (fi != NULL && fi->fh != 0) {
        if (ftruncate((int) fi->fh, size) == -1) {
            return -errno;
        }

        return 0;
    }

    char real_path[PATH_MAX];

    int res = build_file_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (truncate(real_path, size) == -1) {
        return -errno;
    }

    return 0;
}

static int moo_unlink(const char *path) {
    char real_path[PATH_MAX];

    int res = build_file_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (unlink(real_path) == -1) {
        return -errno;
    }

    return 0;
}

static int moo_access(const char *path, int mask) {
    char real_path[PATH_MAX];

    int res = build_dir_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (path_exists(real_path)) {
        return access(real_path, mask) == -1 ? -errno : 0;
    }

    res = build_file_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (path_exists(real_path)) {
        return access(real_path, mask) == -1 ? -errno : 0;
    }

    return -ENOENT;
}

static int moo_utimens(const char *path, const struct timespec tv[2],
                       struct fuse_file_info *fi) {
    (void) fi;

    char real_path[PATH_MAX];

    int res = build_dir_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (path_exists(real_path)) {
        return utimensat(AT_FDCWD, real_path, tv, 0) == -1 ? -errno : 0;
    }

    res = build_file_path(real_path, path);
    if (res != 0) {
        return res;
    }

    if (path_exists(real_path)) {
        return utimensat(AT_FDCWD, real_path, tv, 0) == -1 ? -errno : 0;
    }

    return -ENOENT;
}

static int moo_release(const char *path, struct fuse_file_info *fi) {
    (void) path;

    if (fi != NULL) {
        close((int) fi->fh);
    }

    return 0;
}

static struct fuse_operations moo_oper = {
    .getattr = moo_getattr,
    .readdir = moo_readdir,
    .mkdir = moo_mkdir,
    .rmdir = moo_rmdir,
    .create = moo_create,
    .open = moo_open,
    .read = moo_read,
    .write = moo_write,
    .truncate = moo_truncate,
    .unlink = moo_unlink,
    .access = moo_access,
    .utimens = moo_utimens,
    .release = moo_release,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <encrypted_storage> <fuse_mount> [FUSE options]\n", argv[0]);
        fprintf(stderr, "Example: %s ./encrypted_storage ./fuse_mount -f\n", argv[0]);
        return 1;
    }

    if (realpath(argv[1], storage_root) == NULL) {
        perror("realpath encrypted_storage");
        return 1;
    }

    char **fuse_argv = calloc((size_t) argc, sizeof(char *));
    if (fuse_argv == NULL) {
        perror("calloc");
        return 1;
    }

    fuse_argv[0] = argv[0];
    fuse_argv[1] = argv[2];

    for (int i = 3; i < argc; i++) {
        fuse_argv[i - 1] = argv[i];
    }

    int fuse_argc = argc - 1;

    int ret = fuse_main(fuse_argc, fuse_argv, &moo_oper, NULL);

    free(fuse_argv);
    return ret;
}