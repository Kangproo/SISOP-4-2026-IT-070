#define _DEFAULT_SOURCE
#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <linux/limits.h>

static char source_dir[PATH_MAX];

#define VIRTUAL_FILE "/tujuan.txt"
#define KOORD_PREFIX "KOORD:"

static void make_full_path(char full_path[PATH_MAX], const char *path) {
    snprintf(full_path, PATH_MAX, "%s%s", source_dir, path);
}

static char *read_whole_file(const char *file_path) {
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    char *content = malloc(file_size + 1);
    if (content == NULL) {
        fclose(fp);
        return NULL;
    }

    fread(content, 1, file_size, fp);
    content[file_size] = '\0';

    fclose(fp);
    return content;
}

static void remove_newline_only(char *text) {
    int len = strlen(text);

    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[len - 1] = '\0';
        len--;
    }
}

static char *extract_koord(const char *content) {
    char *koord_pos = strstr(content, KOORD_PREFIX);

    if (koord_pos == NULL) {
        return strdup("");
    }

    koord_pos += strlen(KOORD_PREFIX);

    while (*koord_pos == ' ' || *koord_pos == '\t') {
        koord_pos++;
    }

    char *line_end = strchr(koord_pos, '\n');

    size_t length;
    if (line_end != NULL) {
        length = line_end - koord_pos;
    } else {
        length = strlen(koord_pos);
    }

    char *fragment = malloc(length + 1);
    if (fragment == NULL) {
        return NULL;
    }

    strncpy(fragment, koord_pos, length);
    fragment[length] = '\0';

    remove_newline_only(fragment);

    return fragment;
}

static char *create_tujuan_content(void) {
    char combined[8192];
    combined[0] = '\0';

    for (int i = 1; i <= 7; i++) {
        char file_path[PATH_MAX + 20];
        snprintf(file_path, sizeof(file_path), "%s/%d.txt", source_dir, i);

        char *content = read_whole_file(file_path);
        if (content == NULL) {
            continue;
        }

        char *fragment = extract_koord(content);
        if (fragment != NULL) {
            strcat(combined, fragment);
            free(fragment);
        }

        free(content);
    }

    char *result = malloc(strlen("Tujuan_Mas_Amba: ") + strlen(combined) + 2);
    if (result == NULL) {
        return NULL;
    }

    sprintf(result, "Tujuan_Mas_Amba: %s\n", combined);

    return result;
}

static int kenz_getattr(const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi) {
    (void) fi;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, VIRTUAL_FILE) == 0) {
        char *content = create_tujuan_content();

        if (content == NULL) {
            return -ENOMEM;
        }

        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(content);

        free(content);
        return 0;
    }

    char full_path[PATH_MAX];
    make_full_path(full_path, path);

    int result = lstat(full_path, stbuf);

    if (result == -1) {
        return -errno;
    }

    return 0;
}

static int kenz_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char full_path[PATH_MAX];
    make_full_path(full_path, path);

    DIR *dir = opendir(full_path);

    if (dir == NULL) {
        return -errno;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct stat st;
        memset(&st, 0, sizeof(st));

        st.st_ino = entry->d_ino;
        st.st_mode = entry->d_type << 12;

        filler(buf, entry->d_name, &st, 0, 0);
    }

    if (strcmp(path, "/") == 0) {
        char *content = create_tujuan_content();

        struct stat st;
        memset(&st, 0, sizeof(st));

        st.st_mode = S_IFREG | 0444;
        st.st_nlink = 1;

        if (content != NULL) {
            st.st_size = strlen(content);
            free(content);
        } else {
            st.st_size = 0;
        }

        filler(buf, "tujuan.txt", &st, 0, 0);
    }

    closedir(dir);

    return 0;
}

static int kenz_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, VIRTUAL_FILE) == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) {
            return -EACCES;
        }

        return 0;
    }

    char full_path[PATH_MAX];
    make_full_path(full_path, path);

    int fd = open(full_path, fi->flags);

    if (fd == -1) {
        return -errno;
    }

    close(fd);

    return 0;
}

static int kenz_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    (void) fi;

    if (strcmp(path, VIRTUAL_FILE) == 0) {
        char *content = create_tujuan_content();

        if (content == NULL) {
            return -ENOMEM;
        }

        size_t content_length = strlen(content);

        if ((size_t) offset < content_length) {
            if (offset + size > content_length) {
                size = content_length - offset;
            }

            memcpy(buf, content + offset, size);
        } else {
            size = 0;
        }

        free(content);

        return size;
    }

    char full_path[PATH_MAX];
    make_full_path(full_path, path);

    int fd = open(full_path, O_RDONLY);

    if (fd == -1) {
        return -errno;
    }

    int result = pread(fd, buf, size, offset);

    if (result == -1) {
        result = -errno;
    }

    close(fd);

    return result;
}

static const struct fuse_operations kenz_operations = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open = kenz_open,
    .read = kenz_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_directory> <mount_directory>\n", argv[0]);
        return 1;
    }

    if (realpath(argv[1], source_dir) == NULL) {
        perror("realpath");
        return 1;
    }

    char *fuse_argv[argc];
    int fuse_argc = 0;

    fuse_argv[fuse_argc++] = argv[0];

    for (int i = 2; i < argc; i++) {
        fuse_argv[fuse_argc++] = argv[i];
    }

    return fuse_main(fuse_argc, fuse_argv, &kenz_operations, NULL);
}