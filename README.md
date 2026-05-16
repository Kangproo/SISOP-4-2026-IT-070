# Laporan Praktikum Sistem Operasi

## Identitas

Nama: Sultan Ahmad Maulana Bahyshidqi

NRP: 5027251070

---

# Soal 1 - Save Asisten Kenz

## Deskripsi Soal

Pada soal ini, saya diminta membuat program FUSE bernama `kenz_rescue.c`. Program ini menerima dua argumen, yaitu:

```bash
./kenz_rescue <source_directory> <mount_directory>
```

Dalam soal ini, `source_directory` berisi tujuh file catatan ekspedisi Mas Amba, yaitu `1.txt` sampai `7.txt`. Setelah program FUSE dijalankan, semua file tersebut harus muncul di `mount_directory` dan bisa dibaca seperti file biasa.

Selain itu, program juga harus membuat satu file virtual bernama `tujuan.txt` di root mount directory. File ini tidak boleh benar-benar ada di source directory, tetapi harus muncul saat `ls` dilakukan di folder mount. Isi `tujuan.txt` dibuat secara otomatis dari potongan koordinat `KOORD:` yang ada pada file `1.txt` sampai `7.txt`.

---

## Struktur Folder

Struktur folder yang digunakan pada soal ini adalah sebagai berikut:

```text
soal_1/
├── kenz_rescue.c
├── amba_files/
│   ├── 1.txt
│   ├── 2.txt
│   ├── 3.txt
│   ├── 4.txt
│   ├── 5.txt
│   ├── 6.txt
│   └── 7.txt
└── mnt/
```

Keterangan:

* `kenz_rescue.c` adalah program utama FUSE.
* `amba_files/` adalah folder sumber yang berisi file asli.
* `mnt/` adalah folder mount point FUSE.
* `tujuan.txt` tidak dibuat manual di `amba_files/`, tetapi dibuat secara virtual oleh program.

---

## Penjelasan Solusi

Solusi dibuat menggunakan FUSE dengan konsep **passthrough filesystem**. Artinya, sebagian besar file yang tampil di folder `mnt/` sebenarnya berasal langsung dari folder `amba_files/`.

Program mengimplementasikan beberapa callback utama FUSE:

* `getattr` untuk mengambil informasi file atau folder.
* `readdir` untuk menampilkan isi folder.
* `open` untuk membuka file.
* `read` untuk membaca isi file.

Untuk file `1.txt` sampai `7.txt`, program hanya meneruskan operasi ke file asli di `amba_files/`.

Namun untuk `tujuan.txt`, program membuat file tersebut secara virtual. Saat user menjalankan:

```bash
cat mnt/tujuan.txt
```

program akan membaca semua file `1.txt` sampai `7.txt`, mengambil bagian setelah kata `KOORD:`, menggabungkannya, lalu menampilkan hasilnya sebagai isi `tujuan.txt`.

---

## Kode Program

File: `kenz_rescue.c`

```c
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
```

---

## Penjelasan Kode per Bagian

### 1. Header dan konfigurasi FUSE

```c
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
```

Bagian ini berisi library yang dibutuhkan program.

`FUSE_USE_VERSION 31` dipakai karena program menggunakan FUSE 3. Library seperti `dirent.h`, `fcntl.h`, dan `unistd.h` digunakan untuk operasi file dan folder di Linux.

---

### 2. Variabel global dan konstanta

```c
static char source_dir[PATH_MAX];

#define VIRTUAL_FILE "/tujuan.txt"
#define KOORD_PREFIX "KOORD:"
```

`source_dir` digunakan untuk menyimpan path asli dari folder sumber. Pada program ini, folder sumbernya adalah `amba_files`.

`VIRTUAL_FILE` berisi nama file virtual yang akan ditampilkan di mount directory, yaitu `/tujuan.txt`.

`KOORD_PREFIX` digunakan sebagai kata kunci untuk mencari bagian koordinat di setiap file catatan.

---

### 3. Membuat path lengkap file sumber

```c
static void make_full_path(char full_path[PATH_MAX], const char *path) {
    snprintf(full_path, PATH_MAX, "%s%s", source_dir, path);
}
```

Fungsi ini menggabungkan `source_dir` dengan path dari FUSE.

Contohnya, jika:

```text
source_dir = /home/user/soal_1/amba_files
path       = /1.txt
```

maka hasilnya menjadi:

```text
/home/user/soal_1/amba_files/1.txt
```

Fungsi ini penting karena FUSE hanya menerima path dari mount directory, sedangkan program perlu mengakses file asli di source directory.

---

### 4. Membaca seluruh isi file

```c
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
```

Fungsi ini dipakai untuk membaca isi file `1.txt` sampai `7.txt`.

Alurnya:

1. File dibuka dengan `fopen`.
2. Ukuran file dicari menggunakan `fseek` dan `ftell`.
3. Memori dialokasikan dengan `malloc`.
4. Isi file dibaca menggunakan `fread`.
5. String ditutup dengan karakter `\0`.

Fungsi ini mengembalikan isi file dalam bentuk string.

---

### 5. Menghapus newline di akhir teks

```c
static void remove_newline_only(char *text) {
    int len = strlen(text);

    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[len - 1] = '\0';
        len--;
    }
}
```

Fungsi ini digunakan untuk membersihkan karakter newline `\n` dan carriage return `\r` di akhir teks.

Ini penting karena file teks bisa memiliki format newline berbeda, terutama jika file pernah dibuat atau diedit di Windows. Kalau newline tidak dibersihkan, hasil gabungan koordinat bisa terlihat berantakan.

---

### 6. Mengambil potongan koordinat

```c
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
```

Fungsi ini mencari teks `KOORD:` di dalam file.

Contohnya pada file `1.txt` terdapat:

```text
KOORD: -7.957
```

Maka fungsi ini hanya mengambil:

```text
-7.957
```

Fungsi ini juga menghapus spasi di awal setelah `KOORD:` dan mengambil isi sampai akhir baris.

---

### 7. Membuat isi file virtual `tujuan.txt`

```c
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
```

Fungsi ini adalah inti dari pembuatan `tujuan.txt`.

Program melakukan looping dari file `1.txt` sampai `7.txt`, lalu mengambil isi `KOORD:` dari setiap file. Semua potongan koordinat digabung menjadi satu.

Potongan koordinat dari file adalah:

```text
1.txt = -7.957
2.txt = 382728
3.txt = 443728,
4.txt = 112.469
5.txt = 8688227961,
6.txt = 23:
7.txt = 59 WIB
```

Jika digabung, hasilnya menjadi:

```text
Tujuan_Mas_Amba: -7.957382728443728, 112.4698688227961, 23:59 WIB
```

---

### 8. Callback `getattr`

```c
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
```

`getattr` dipanggil saat sistem membutuhkan informasi file, misalnya saat menjalankan `ls -l` atau `stat`.

Untuk `tujuan.txt`, program membuat metadata sendiri:

* `S_IFREG` artinya file biasa.
* `0444` artinya file hanya bisa dibaca.
* `st_size` diisi sesuai panjang isi virtual.

Untuk file lain, program memakai `lstat()` agar metadata diambil dari file asli di `amba_files`.

---

### 9. Callback `readdir`

```c
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
```

`readdir` dipanggil saat user menjalankan:

```bash
ls mnt
```

Program akan membaca isi folder asli `amba_files`, lalu menampilkan semua file di dalamnya.

Setelah itu, jika path yang dibaca adalah root `/`, program menambahkan satu entry tambahan:

```text
tujuan.txt
```

Bagian ini yang membuat `tujuan.txt` muncul saat `ls mnt/`, walaupun file itu tidak ada secara fisik di `amba_files`.

---

### 10. Callback `open`

```c
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
```

`open` dipanggil saat file akan dibuka.

Untuk `tujuan.txt`, program hanya mengizinkan mode baca. Jika user mencoba membuka dengan mode tulis, program mengembalikan:

```c
-EACCES
```

Untuk file lain, program membuka file asli di `amba_files`.

---

### 11. Callback `read`

```c
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
```

`read` dipanggil saat user menjalankan `cat`.

Untuk `tujuan.txt`, isi file dibuat saat itu juga menggunakan `create_tujuan_content()`. Karena isi dibuat saat dibutuhkan, file ini disebut virtual file yang dibuat secara on-the-fly.

Untuk file lain, program menggunakan `pread()` agar isi file asli bisa dibaca berdasarkan offset.

---

### 12. Mendaftarkan operasi FUSE

```c
static const struct fuse_operations kenz_operations = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open = kenz_open,
    .read = kenz_read,
};
```

Bagian ini menghubungkan callback FUSE dengan fungsi yang sudah dibuat.

Artinya:

* saat sistem butuh metadata, panggil `kenz_getattr`
* saat sistem membaca isi folder, panggil `kenz_readdir`
* saat sistem membuka file, panggil `kenz_open`
* saat sistem membaca file, panggil `kenz_read`

---

### 13. Fungsi `main`

```c
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
```

Fungsi `main` memeriksa apakah argumen sudah lengkap. Program membutuhkan dua argumen utama:

```text
source_directory
mount_directory
```

`realpath()` digunakan untuk menyimpan path absolut dari source directory.

Setelah itu, argumen untuk FUSE disusun ulang. Argumen source directory tidak diteruskan ke FUSE karena source directory hanya dipakai oleh program. Yang diteruskan ke FUSE adalah mount directory dan opsi-opsi FUSE lainnya.

Terakhir, program menjalankan:

```c
fuse_main(fuse_argc, fuse_argv, &kenz_operations, NULL);
```

yang membuat filesystem FUSE mulai berjalan.

---

## Cara Menjalankan

### 1. Install dependency FUSE

Jika belum ada library FUSE 3, jalankan:

```bash
sudo apt update
sudo apt install libfuse3-dev fuse3 pkg-config
```

### 2. Siapkan folder

```bash
mkdir -p amba_files mnt
```

Lalu pastikan file `1.txt` sampai `7.txt` berada di folder `amba_files`.

### 3. Compile program

```bash
gcc kenz_rescue.c -o kenz_rescue $(pkg-config fuse3 --cflags --libs)
```

### 4. Jalankan FUSE

```bash
./kenz_rescue amba_files mnt
```

Jika ingin melihat proses FUSE di foreground, bisa menggunakan:

```bash
./kenz_rescue amba_files mnt -f
```

### 5. Cek isi mount directory

```bash
ls mnt
```

Output yang diharapkan:

```text
1.txt  2.txt  3.txt  4.txt  5.txt  6.txt  7.txt  tujuan.txt
```

### 6. Cek file passthrough

```bash
cat mnt/1.txt
cat amba_files/1.txt
```

Output keduanya harus sama.

Untuk validasi semua file passthrough:

```bash
for i in 1 2 3 4 5 6 7; do
    diff mnt/$i.txt amba_files/$i.txt && echo "$i.txt OK"
done
```

Jika semua sama, output yang diharapkan:

```text
1.txt OK
2.txt OK
3.txt OK
4.txt OK
5.txt OK
6.txt OK
7.txt OK
```

### 7. Cek file virtual `tujuan.txt`

```bash
cat mnt/tujuan.txt
```

Output yang diharapkan:

```text
Tujuan_Mas_Amba: -7.957382728443728, 112.4698688227961, 23:59 WIB
```

### 8. Cek bahwa `tujuan.txt` tidak ada di source directory

```bash
ls amba_files
```

Output yang benar hanya berisi:

```text
1.txt  2.txt  3.txt  4.txt  5.txt  6.txt  7.txt
```

Jika `tujuan.txt` hanya muncul di `mnt/` dan tidak muncul di `amba_files/`, berarti file virtual berhasil dibuat.

### 9. Unmount FUSE

Setelah selesai, lepaskan mount dengan:

```bash
fusermount3 -u mnt
```

Jika gagal, gunakan:

```bash
sudo umount -l mnt
```

---

## Hasil Output

### Output `ls mnt`

```text
1.txt  2.txt  3.txt  4.txt  5.txt  6.txt  7.txt  tujuan.txt
```

### Output validasi passthrough

```text
1.txt OK
2.txt OK
3.txt OK
4.txt OK
5.txt OK
6.txt OK
7.txt OK
```

### Output `cat mnt/tujuan.txt`

```text
Tujuan_Mas_Amba: -7.957382728443728, 112.4698688227961, 23:59 WIB
```

---

## Validasi Hasil

Validasi dilakukan dengan dua cara.

Pertama, file `1.txt` sampai `7.txt` yang muncul di `mnt/` dibandingkan dengan file asli di `amba_files/` menggunakan `diff`. Jika tidak ada perbedaan dan muncul pesan `OK`, berarti passthrough berhasil.

Kedua, file `tujuan.txt` dicek dengan `cat`. File ini harus muncul di `mnt/`, tetapi tidak boleh muncul di `amba_files/`. Jika isi `tujuan.txt` berisi gabungan koordinat dari tujuh file, berarti virtual file berhasil dibuat.

Potongan koordinat yang digunakan adalah:

```text
1.txt -> -7.957
2.txt -> 382728
3.txt -> 443728,
4.txt -> 112.469
5.txt -> 8688227961,
6.txt -> 23:
7.txt -> 59 WIB
```

Hasil gabungannya:

```text
Tujuan_Mas_Amba: -7.957382728443728, 112.4698688227961, 23:59 WIB
```

---

## Kendala yang Dihadapi

Beberapa kendala yang saya hadapi saat mengerjakan soal ini:

### 1. Error saat compile FUSE

Awalnya program dicompile seperti program C biasa:

```bash
gcc kenz_rescue.c -o kenz_rescue
```

Namun muncul error:

```text
undefined reference to `fuse_main_real`
```

Penyebabnya adalah library FUSE belum ikut dilink saat proses compile. Solusinya adalah compile menggunakan `pkg-config`:

```bash
gcc kenz_rescue.c -o kenz_rescue $(pkg-config fuse3 --cflags --libs)
```

Jika `pkg-config fuse3` belum tersedia, maka dependency FUSE perlu diinstall terlebih dahulu:

```bash
sudo apt update
sudo apt install libfuse3-dev fuse3 pkg-config
```

### 2. Mount point masih aktif

Saat program pernah dijalankan lalu dihentikan tidak bersih, folder `mnt` bisa masih dianggap sebagai mount point aktif. Akibatnya, saat menjalankan ulang program bisa muncul error atau isi folder tidak berubah.

Solusinya adalah melakukan unmount terlebih dahulu:

```bash
fusermount3 -u mnt
```

Jika masih gagal:

```bash
sudo umount -l mnt
```

### 3. File `tujuan.txt` tidak muncul

Kendala ini terjadi ketika `readdir` hanya menampilkan isi folder asli dari `amba_files`, tetapi belum menambahkan entry virtual `tujuan.txt`.

Solusinya adalah menambahkan `filler(buf, "tujuan.txt", &st, 0, 0);` pada callback `readdir` khusus ketika path yang dibaca adalah root `/`.

### 4. `cat mnt/tujuan.txt` gagal atau kosong

Masalah ini dapat terjadi jika callback `getattr` sudah membuat metadata `tujuan.txt`, tetapi callback `read` belum menangani path `/tujuan.txt`.

Solusinya adalah menambahkan kondisi khusus pada `kenz_read`:

```c
if (strcmp(path, VIRTUAL_FILE) == 0) {
    char *content = create_tujuan_content();
    ...
}
```

Dengan begitu, saat file virtual dibaca, isinya dibuat langsung oleh program.

### 5. Folder mount tidak bisa dipakai untuk membuat file baru

Saat mencoba membuat file baru di `mnt`, operasi tersebut tidak berjalan karena program hanya mengimplementasikan:

```text
getattr
readdir
open
read
```

Program belum mengimplementasikan:

```text
create
write
mkdir
unlink
```

Jadi filesystem ini memang bersifat read-only. Hal ini tidak menjadi masalah karena kebutuhan soal hanya membaca file dan membuat virtual file `tujuan.txt`.

---

## Kesimpulan

Pada soal ini, saya belajar cara membuat filesystem sederhana menggunakan FUSE. Program `kenz_rescue.c` bekerja sebagai passthrough filesystem, yaitu menampilkan file asli dari `amba_files` ke dalam mount directory `mnt`.

Selain itu, program juga membuat file virtual `tujuan.txt` yang tidak ada secara fisik di `amba_files`, tetapi tetap muncul dan bisa dibaca dari `mnt`. Isi file tersebut dibuat secara otomatis dari gabungan potongan koordinat `KOORD:` pada file `1.txt` sampai `7.txt`.

Dengan hasil tersebut, program sudah memenuhi kebutuhan soal karena:

* file `1.txt` sampai `7.txt` muncul di mount directory
* isi file passthrough sama dengan file asli
* `tujuan.txt` muncul secara virtual di mount directory
* `tujuan.txt` tidak ada di source directory
* isi `tujuan.txt` berisi koordinat tujuan Mas Amba

---

# Soal 2 - Poke MOO

## Deskripsi Soal

Pada soal ini, saya diminta membuat sistem mini database service bernama **Poke MOO**. Sistem ini terdiri dari beberapa bagian utama, yaitu:

1. FUSE filesystem sebagai translator antara file normal dan file terenkripsi.
2. Server database yang berjalan di dalam Docker container.
3. Client TCP untuk mengirim command ke server.
4. Bind mount antara folder FUSE dan folder database di container.

Program database berjalan pada port `9000`. Client dapat terhubung ke server melalui TCP connection dan menjalankan command database seperti:

```text
HELP
CREATE DATABASE <db>
CREATE TABLE <db> <table> <col1> <col2> ...
INSERT <db> <table> <value1> <value2> ...
SELECT <db> <table>
LIST DATABASE
LIST TABLE <db>
DROP DATABASE <db>
```

Pada soal ini, FUSE tidak hanya menjadi pure passthrough. FUSE berperan sebagai penerjemah antara dua direktori:

```text
fuse_mount/          = tampilan file normal yang dibaca user/server
encrypted_storage/  = penyimpanan asli dalam bentuk terenkripsi
```

Setiap file yang dibuat melalui `fuse_mount` akan disimpan di `encrypted_storage` dalam bentuk terenkripsi dengan tambahan ekstensi `.enc`.

---

## Struktur Folder

Struktur folder akhir untuk Soal 2 adalah sebagai berikut:

```text
soal_2/
├── soal_2/
│   ├── Dockerfile
│   ├── client.c
│   ├── encrypted_storage/
│   ├── fuse.c
│   ├── fuse_mount/
│   └── server
```

Keterangan:

* `Dockerfile` digunakan untuk membuat image server database.
* `client.c` digunakan untuk membuat program client TCP.
* `encrypted_storage/` adalah folder asli tempat file terenkripsi disimpan.
* `fuse.c` adalah program FUSE untuk enkripsi dan dekripsi file.
* `fuse_mount/` adalah mount point yang menampilkan file normal.
* `server` adalah binary server database yang sudah disediakan dari release soal.

Karena folder kosong tidak masuk ke GitHub, saya menambahkan `.gitkeep` pada folder:

```bash
touch encrypted_storage/.gitkeep
touch fuse_mount/.gitkeep
```

---

## Penjelasan Solusi

Solusi pada soal ini dibagi menjadi tiga bagian besar.

### 1. FUSE sebagai translator terenkripsi

FUSE menghubungkan `fuse_mount` dengan `encrypted_storage`.

Jika user atau server membuat file berikut:

```text
fuse_mount/tests/users.csv
```

maka file asli yang disimpan adalah:

```text
encrypted_storage/tests/users.csv.enc
```

Isi file di `encrypted_storage` tidak berupa teks biasa, tetapi sudah dienkripsi menggunakan XOR dengan key:

```c
#define XOR_KEY 0x76
```

Saat file ditulis dari `fuse_mount`, isi file dienkripsi dulu sebelum masuk ke `encrypted_storage`.

Saat file dibaca dari `fuse_mount`, isi file dari `encrypted_storage` didekripsi dulu agar tampil normal.

---

### 2. Containerization dengan Docker

Server database dijalankan di dalam Docker container. Docker image dibuat dari `ubuntu:latest`, lalu file `server` dicopy ke `/app/server`.

Server menggunakan `/app/db` sebagai folder database. Nantinya, `/app/db` di dalam container dihubungkan dengan folder `fuse_mount` di host menggunakan bind mount.

Jadi server merasa sedang membaca dan menulis file biasa di `/app/db`, padahal file tersebut masuk ke FUSE dan disimpan terenkripsi di `encrypted_storage`.

---

### 3. Integration dengan client TCP

Setelah container berjalan, client `client.c` digunakan untuk terhubung ke server pada:

```text
127.0.0.1:9000
```

Client mengirim command database ke server, lalu server memproses command tersebut. Hasil perubahan file akan masuk ke `/app/db`, yang sebenarnya adalah bind mount dari `fuse_mount`.

Alur lengkapnya:

```text
client
  ↓
server di Docker
  ↓
/app/db
  ↓
fuse_mount
  ↓
FUSE fuse.c
  ↓
encrypted_storage/*.enc
```

---

## Kode Program

## 1. File `fuse.c`

```c
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
```

---

## 2. File `client.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 9000
#define BUF_SIZE 4096

static int connect_to_server(const char *host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", host);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

int main(int argc, char *argv[]) {
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    int sock = connect_to_server(host, port);
    if (sock < 0) return 1;

    printf("Connected to DB Server on %s:%d\n", host, port);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n\n");

    char input[BUF_SIZE];
    char response[BUF_SIZE + 1];

    while (1) {
        printf("db > ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\r\n")] = '\0';
        if (strcmp(input, "EXIT") == 0 || strcmp(input, "exit") == 0) {
            break;
        }

        strcat(input, "\n");

        if (send(sock, input, strlen(input), 0) < 0) {
            perror("send");
            break;
        }

        fd_set readfds;
        struct timeval tv;

        while (1) {
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            tv.tv_sec = 0;
            tv.tv_usec = 200000;

            int ready = select(sock + 1, &readfds, NULL, NULL, &tv);
            if (ready < 0) {
                perror("select");
                close(sock);
                return 1;
            }
            if (ready == 0) {
                break;
            }

            ssize_t n = recv(sock, response, BUF_SIZE, 0);
            if (n < 0) {
                perror("recv");
                close(sock);
                return 1;
            }
            if (n == 0) {
                printf("Server closed connection\n");
                close(sock);
                return 0;
            }

            response[n] = '\0';
            printf("%s", response);
            if (response[n - 1] != '\n') printf("\n");
        }
    }

    close(sock);
    return 0;
}

```

---

## 3. File `Dockerfile`

```dockerfile
FROM ubuntu:latest

WORKDIR /app

COPY server /app/server

RUN chmod +x /app/server && mkdir -p /app/db

EXPOSE 9000

CMD ["/app/server"]

```

---

## Penjelasan Kode `fuse.c` per Bagian

### 1. Header, konfigurasi FUSE, dan XOR key

```c
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
```

Bagian ini mendefinisikan bahwa program menggunakan FUSE versi 3. `XOR_KEY` adalah kunci enkripsi yang digunakan untuk mengenkripsi dan mendekripsi isi file.

Karena XOR bersifat reversible, operasi yang sama bisa digunakan untuk enkripsi dan dekripsi.

---

### 2. Menyimpan root storage

```c
static char storage_root[PATH_MAX];
```

Variabel ini menyimpan path absolut ke folder `encrypted_storage`. Semua file asli akan disimpan berdasarkan root ini.

---

### 3. Fungsi XOR buffer

```c
static void xor_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] ^= XOR_KEY;
    }
}
```

Fungsi ini melakukan XOR pada setiap byte dalam buffer.

Saat menulis file:

```text
plain text -> XOR -> encrypted text
```

Saat membaca file:

```text
encrypted text -> XOR -> plain text
```

Karena key yang digunakan sama, fungsi ini bisa digunakan untuk dua arah.

---

### 4. Mengecek dan menghapus ekstensi `.enc`

```c
static int has_enc_suffix(const char *name)
static void remove_enc_suffix(char *name)
```

Dua fungsi ini digunakan untuk mengatur nama file yang tampil di `fuse_mount`.

Di `encrypted_storage`, file disimpan dengan tambahan `.enc`.

Contoh:

```text
encrypted_storage/tests/notes.csv.enc
```

Namun di `fuse_mount`, file harus terlihat sebagai:

```text
fuse_mount/tests/notes.csv
```

Maka saat `readdir`, ekstensi `.enc` dihapus sebelum nama file ditampilkan.

---

### 5. Membuat path direktori asli

```c
static int build_dir_path(char out[PATH_MAX], const char *path)
```

Fungsi ini membentuk path direktori asli dari path FUSE.

Contoh:

```text
path FUSE     = /tests
storage_root  = /home/user/soal_2/encrypted_storage
hasil         = /home/user/soal_2/encrypted_storage/tests
```

Fungsi ini digunakan untuk operasi direktori seperti `mkdir`, `rmdir`, dan `readdir`.

---

### 6. Membuat path file terenkripsi

```c
static int build_file_path(char out[PATH_MAX], const char *path)
```

Fungsi ini membentuk path file asli dengan tambahan ekstensi `.enc`.

Contoh:

```text
path FUSE     = /tests/users.csv
storage_root  = /home/user/soal_2/encrypted_storage
hasil         = /home/user/soal_2/encrypted_storage/tests/users.csv.enc
```

Fungsi ini digunakan untuk operasi file seperti `open`, `read`, `write`, `truncate`, dan `unlink`.

---

### 7. Callback `getattr`

```c
static int moo_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
```

Callback ini dipakai saat sistem membutuhkan metadata file atau folder.

Program pertama-tama mengecek apakah path adalah folder di `encrypted_storage`. Jika ada, metadata folder dikembalikan.

Jika bukan folder, program mengecek apakah path tersebut adalah file dengan tambahan `.enc`.

Contoh ketika user mengecek:

```bash
stat fuse_mount/tests/notes.csv
```

program akan mengecek file asli:

```text
encrypted_storage/tests/notes.csv.enc
```

Jika file ditemukan, mode file diatur sebagai file biasa menggunakan `S_IFREG`.

---

### 8. Callback `readdir`

```c
static int moo_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags)
```

Callback ini dipakai saat user menjalankan:

```bash
ls fuse_mount
```

Program membaca isi folder asli di `encrypted_storage`. Jika ada file dengan ekstensi `.enc`, ekstensi tersebut dihapus sebelum ditampilkan ke user.

Contoh:

```text
encrypted_storage/tests/notes.csv.enc
```

akan tampil sebagai:

```text
fuse_mount/tests/notes.csv
```

---

### 9. Callback `mkdir` dan `rmdir`

```c
static int moo_mkdir(const char *path, mode_t mode)
static int moo_rmdir(const char *path)
```

`mkdir` digunakan untuk membuat folder baru dari `fuse_mount`, sedangkan `rmdir` digunakan untuk menghapus folder.

Jika user menjalankan:

```bash
mkdir fuse_mount/KAMPUS
```

maka folder asli yang dibuat adalah:

```text
encrypted_storage/KAMPUS
```

Folder tidak diberi `.enc` karena yang dienkripsi hanya file, bukan nama folder.

---

### 10. Callback `create` dan `open`

```c
static int moo_create(const char *path, mode_t mode, struct fuse_file_info *fi)
static int moo_open(const char *path, struct fuse_file_info *fi)
```

`create` dipakai saat file baru dibuat, sedangkan `open` dipakai saat file dibuka.

Saat user membuat:

```text
fuse_mount/KAMPUS/MAHASISWA.csv
```

program sebenarnya membuka/membuat:

```text
encrypted_storage/KAMPUS/MAHASISWA.csv.enc
```

File descriptor disimpan di:

```c
fi->fh = fd;
```

Tujuannya agar callback lain seperti `read`, `write`, dan `truncate` bisa memakai file descriptor yang sama.

---

### 11. Callback `read`

```c
static int moo_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
```

Callback ini digunakan saat file dibaca.

Alurnya:

1. Membuka file asli `.enc` di `encrypted_storage`.
2. Membaca isi file menggunakan `pread`.
3. Melakukan XOR pada buffer.
4. Mengembalikan hasil yang sudah didekripsi ke user.

Jadi walaupun file asli terenkripsi, user tetap melihat isi normal dari `fuse_mount`.

---

### 12. Callback `write`

```c
static int moo_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
```

Callback ini digunakan saat file ditulis.

Alurnya:

1. Menerima data normal dari user/server.
2. Menyalin data ke buffer sementara.
3. Melakukan XOR pada buffer sementara.
4. Menulis hasil terenkripsi ke file `.enc`.

Dengan cara ini, isi file asli di `encrypted_storage` tidak berbentuk plain text.

---

### 13. Callback `truncate`

```c
static int moo_truncate(const char *path, off_t size, struct fuse_file_info *fi)
```

Callback ini digunakan untuk mengubah ukuran file.

Operasi ini penting karena banyak program ketika menulis file melakukan `truncate` lebih dulu untuk mengosongkan file lama, lalu menulis ulang isi baru.

---

### 14. Callback `unlink`

```c
static int moo_unlink(const char *path)
```

Callback ini dipakai untuk menghapus file.

Jika user menghapus:

```text
fuse_mount/tests/notes.csv
```

maka yang dihapus sebenarnya:

```text
encrypted_storage/tests/notes.csv.enc
```

---

### 15. Callback `access`

```c
static int moo_access(const char *path, int mask)
```

Callback ini digunakan untuk mengecek hak akses file atau folder.

Program mengecek dua kemungkinan:

1. path sebagai folder biasa di `encrypted_storage`
2. path sebagai file `.enc` di `encrypted_storage`

Jika tidak ditemukan, program mengembalikan `-ENOENT`.

---

### 16. Callback `utimens`

```c
static int moo_utimens(const char *path, const struct timespec tv[2],
                       struct fuse_file_info *fi)
```

Callback ini digunakan untuk mengubah timestamp file, misalnya waktu akses dan waktu modifikasi.

Callback ini dibutuhkan supaya metadata file tetap bisa dikelola dengan baik.

---

### 17. Callback `release`

```c
static int moo_release(const char *path, struct fuse_file_info *fi)
```

Callback ini dipanggil saat file ditutup. Program menutup file descriptor yang sebelumnya disimpan di `fi->fh`.

---

### 18. Mendaftarkan operasi FUSE

```c
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
```

Bagian ini menghubungkan callback FUSE dengan fungsi yang telah dibuat.

Berbeda dari Soal 1 yang hanya read-only, pada Soal 2 FUSE harus mendukung operasi baca dan tulis, karena database server perlu membuat folder, membuat file, menulis data, membaca data, dan menghapus data.

---

### 19. Fungsi `main` pada `fuse.c`

```c
int main(int argc, char *argv[])
```

Fungsi ini memeriksa argumen program.

Format penggunaan:

```bash
./fuse_mount_prog <encrypted_storage> <fuse_mount> [FUSE options]
```

Contoh:

```bash
./fuse_mount_prog encrypted_storage fuse_mount -f -o allow_other
```

Argumen pertama setelah nama program disimpan sebagai `storage_root`, yaitu folder asli `encrypted_storage`. Argumen kedua diteruskan ke FUSE sebagai mount point.

---

## Penjelasan Kode `client.c` per Bagian

### 1. Header dan konfigurasi awal

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 9000
#define BUF_SIZE 4096
```

Client menggunakan socket TCP untuk terhubung ke server. Default host adalah `127.0.0.1` dan default port adalah `9000`.

---

### 2. Fungsi koneksi ke server

```c
static int connect_to_server(const char *host, int port)
```

Fungsi ini membuat socket dengan:

```c
socket(AF_INET, SOCK_STREAM, 0)
```

Artinya client menggunakan IPv4 dan TCP.

Kemudian `inet_pton` mengubah alamat IP dari string menjadi format binary yang dibutuhkan socket. Setelah itu, `connect` digunakan untuk menghubungkan client ke server.

---

### 3. Fungsi `main` pada client

```c
int main(int argc, char *argv[])
```

Jika user tidak memberi argumen, client menggunakan:

```text
host = 127.0.0.1
port = 9000
```

Jika user menjalankan:

```bash
./client 127.0.0.1 9000
```

maka client akan terhubung ke server pada alamat tersebut.

---

### 4. Loop input command

```c
while (1) {
    printf("db > ");
    fflush(stdout);

    if (fgets(input, sizeof(input), stdin) == NULL) {
        break;
    }
```

Bagian ini membuat prompt:

```text
db >
```

User bisa mengetik command database. Command kemudian dikirim ke server menggunakan:

```c
send(sock, input, strlen(input), 0)
```

---

### 5. Menerima response dari server

Client memakai `select()` untuk menunggu response dari server dalam waktu singkat. Jika server mengirim data, client menerima dengan:

```c
recv(sock, response, BUF_SIZE, 0)
```

Lalu response ditampilkan ke terminal.

---

## Penjelasan Dockerfile

```dockerfile
FROM ubuntu:latest

WORKDIR /app

COPY server /app/server

RUN chmod +x /app/server && mkdir -p /app/db

EXPOSE 9000

CMD ["/app/server"]
```

Penjelasan:

* `FROM ubuntu:latest` berarti image dibuat dari Ubuntu.
* `WORKDIR /app` membuat folder kerja container di `/app`.
* `COPY server /app/server` menyalin binary server ke dalam container.
* `RUN chmod +x /app/server && mkdir -p /app/db` membuat server bisa dieksekusi dan membuat folder database.
* `EXPOSE 9000` menandakan server memakai port `9000`.
* `CMD ["/app/server"]` menjalankan server saat container dinyalakan.

---

## Cara Menjalankan

### 1. Masuk ke folder Soal 2

```bash
cd soal_2/soal_2
```

Atau sesuaikan dengan lokasi folder di laptop.

---

### 2. Siapkan folder

```bash
mkdir -p encrypted_storage fuse_mount
touch encrypted_storage/.gitkeep
touch fuse_mount/.gitkeep
```

---

### 3. Compile FUSE dan client

```bash
gcc fuse.c -o fuse_mount_prog $(pkg-config fuse3 --cflags --libs)
gcc client.c -o client
```

Jika `pkg-config fuse3` belum tersedia, install dependency:

```bash
sudo apt update
sudo apt install libfuse3-dev fuse3 pkg-config
```

---

### 4. Atur `allow_other` jika dibutuhkan

Karena FUSE akan diakses oleh Docker, opsi `allow_other` dibutuhkan.

Buka file konfigurasi:

```bash
sudo nano /etc/fuse.conf
```

Pastikan ada baris:

```text
user_allow_other
```

Jika masih seperti ini:

```text
#user_allow_other
```

hapus tanda `#`.

---

### 5. Jalankan FUSE

```bash
./fuse_mount_prog encrypted_storage fuse_mount -f -o allow_other
```

Terminal ini jangan ditutup karena FUSE sedang berjalan.

Keterangan:

* `encrypted_storage` adalah folder penyimpanan asli.
* `fuse_mount` adalah folder mount point.
* `-f` membuat FUSE berjalan di foreground.
* `-o allow_other` membuat Docker bisa mengakses mount point.

---

### 6. Build Docker image

Buka terminal baru, lalu jalankan:

```bash
sudo docker build -t soal-2-modul-4-sisop .
```

---

### 7. Jalankan container

```bash
sudo docker rm -f db_app 2>/dev/null

sudo docker run -d --name db_app -p 9000:9000 \
  --mount type=bind,source="$(realpath fuse_mount)",target=/app/db \
  soal-2-modul-4-sisop
```

Penjelasan:

* `--name db_app` memberi nama container.
* `-p 9000:9000` menghubungkan port host dan container.
* `--mount type=bind` menghubungkan folder `fuse_mount` ke `/app/db`.
* `/app/db` adalah folder database yang digunakan server.

---

### 8. Cek container

```bash
sudo docker ps
```

Container yang diharapkan muncul:

```text
db_app
```

---

### 9. Jalankan client

```bash
./client 127.0.0.1 9000
```

Jika berhasil, akan muncul tampilan seperti:

```text
Connected to DB Server on 127.0.0.1:9000
Type HELP for available commands
Type EXIT to quit

db >
```

---

## Contoh Pengujian Database

Pada prompt `db >`, jalankan command berikut:

```text
HELP
CREATE DATABASE KAMPUS
CREATE TABLE KAMPUS MAHASISWA nama nrp jurusan
INSERT KAMPUS MAHASISWA Sultan 5027251070 IT
SELECT KAMPUS MAHASISWA
LIST DATABASE
LIST TABLE KAMPUS
EXIT
```

Setelah itu, file hasil database bisa dicek dari terminal:

```bash
cat fuse_mount/KAMPUS/MAHASISWA.csv
```

Output yang diharapkan:

```csv
nama,nrp,jurusan
Sultan,5027251070,IT
```

File asli yang tersimpan di `encrypted_storage` adalah:

```text
encrypted_storage/KAMPUS/MAHASISWA.csv.enc
```

---

## Pengujian Checker `notes.csv.enc`

Untuk memastikan FUSE berhasil melakukan translasi dan dekripsi, saya menggunakan file testing:

```text
notes.csv.enc
```

File ini diletakkan di:

```text
encrypted_storage/tests/notes.csv.enc
```

Langkah pengujian:

```bash
mkdir -p encrypted_storage/tests
cp notes.csv.enc encrypted_storage/tests/notes.csv.enc
```

Pastikan FUSE sudah berjalan, lalu baca dari mount point:

```bash
cat fuse_mount/tests/notes.csv
```

Output yang berhasil muncul:

```csv
author,notes
admin,TEST_SUCCESS
```

Output tersebut membuktikan bahwa:

* file asli berada di `encrypted_storage` dengan nama `notes.csv.enc`
* file tampil di `fuse_mount` sebagai `notes.csv`
* isi file berhasil didekripsi oleh FUSE
* translasi dari `.enc` ke file normal berjalan dengan benar

---

## Validasi Hasil

Validasi dilakukan dengan beberapa cara.

### 1. Validasi nama file

File asli:

```text
encrypted_storage/tests/notes.csv.enc
```

harus tampil sebagai:

```text
fuse_mount/tests/notes.csv
```

Jika `.enc` tidak muncul di `fuse_mount`, berarti fungsi `readdir` berhasil menyembunyikan ekstensi `.enc`.

---

### 2. Validasi isi file

Ketika menjalankan:

```bash
cat fuse_mount/tests/notes.csv
```

hasilnya harus berupa teks normal:

```csv
author,notes
admin,TEST_SUCCESS
```

Jika file dibuka langsung dari `encrypted_storage`, isinya tidak terbaca normal karena masih terenkripsi.

---

### 3. Validasi Docker dan server

Setelah container berjalan, command:

```bash
sudo docker ps
```

harus menampilkan container `db_app`.

Client juga harus bisa tersambung ke server:

```bash
./client 127.0.0.1 9000
```

Jika muncul prompt `db >`, berarti koneksi client-server berhasil.

---

### 4. Validasi database

Command database seperti `CREATE DATABASE`, `CREATE TABLE`, `INSERT`, dan `SELECT` harus bisa dijalankan melalui client.

File hasil database harus terlihat normal di:

```text
fuse_mount/<database>/<table>.csv
```

dan tersimpan terenkripsi di:

```text
encrypted_storage/<database>/<table>.csv.enc
```

---

## Kendala yang Dihadapi

Kendala pertama adalah folder `encrypted_storage` dan `fuse_mount` awalnya kosong, sehingga berpotensi tidak ikut masuk saat repository di-upload ke GitHub. Padahal kedua folder tersebut wajib ada sesuai struktur soal. Untuk mengatasinya, saya menambahkan file `.gitkeep` pada masing-masing folder.

```bash
touch encrypted_storage/.gitkeep
touch fuse_mount/.gitkeep
```

Kendala kedua adalah urutan menjalankan program. Pada awalnya cukup membingungkan apakah Docker dijalankan terlebih dahulu atau FUSE terlebih dahulu. Setelah diuji, FUSE harus dijalankan lebih dulu karena folder `fuse_mount` harus sudah menjadi mount point sebelum di-bind ke container sebagai `/app/db`. Jika Docker dijalankan sebelum FUSE aktif, maka server tidak membaca filesystem terenkripsi melalui FUSE.

Urutan yang benar adalah menjalankan FUSE terlebih dahulu:

```bash
./fuse_mount_prog encrypted_storage fuse_mount -f -o allow_other
```

Setelah FUSE aktif, baru container Docker dijalankan:

```bash
sudo docker run -d --name db_app -p 9000:9000 \
  --mount type=bind,source="$(realpath fuse_mount)",target=/app/db \
  soal-2-modul-4-sisop
```

Kendala ketiga terjadi saat membaca hasil database. Saya sempat mengetik langsung path file seperti berikut:

```bash
fuse_mount/KAMPUS/MAHASISWA.csv
```

Hal tersebut menyebabkan muncul error:

```text
Permission denied
```

Masalah ini terjadi karena Linux menganggap file `.csv` tersebut ingin dijalankan sebagai program. Solusinya adalah membaca isi file menggunakan `cat`.

```bash
cat fuse_mount/KAMPUS/MAHASISWA.csv
```

Dengan cara tersebut, isi tabel dapat ditampilkan dengan benar.

Selain itu, saat testing ulang, FUSE perlu di-unmount terlebih dahulu agar mount point tidak bentrok. Jika mount masih aktif, saya menggunakan:

```bash
fusermount3 -u fuse_mount
```

Jika masih gagal, saya menggunakan:

```bash
sudo umount -l fuse_mount
```


## Cleanup

Jika ingin membersihkan hasil testing sebelum upload:

```bash
sudo docker rm -f db_app 2>/dev/null

fusermount3 -u fuse_mount 2>/dev/null
sudo umount -l fuse_mount 2>/dev/null

rm -f fuse_mount_prog client
rm -rf encrypted_storage/*
rm -rf fuse_mount/*

touch encrypted_storage/.gitkeep
touch fuse_mount/.gitkeep
```

Jika hanya ingin mematikan container tanpa menghapus data:

```bash
sudo docker rm -f db_app
```

Jika ingin melepas FUSE:

```bash
fusermount3 -u fuse_mount
```

---

## Kesimpulan

Pada soal ini, saya belajar membuat filesystem FUSE yang tidak hanya membaca file, tetapi juga mendukung operasi tulis, hapus, pembuatan folder, dan perubahan metadata.

FUSE pada Soal 2 bekerja sebagai translator antara file normal dan file terenkripsi. User dan server melihat file biasa di `fuse_mount`, sedangkan file asli disimpan di `encrypted_storage` dengan ekstensi `.enc` dan isi terenkripsi menggunakan XOR key `0x76`.

Selain itu, saya juga belajar mengintegrasikan FUSE dengan Docker. Server database berjalan di container dan menggunakan `/app/db` sebagai database directory. Folder tersebut dihubungkan ke `fuse_mount`, sehingga semua operasi database otomatis melewati FUSE dan tersimpan terenkripsi.

Hasil checker `notes.csv.enc` yang terbaca sebagai:

```csv
author,notes
admin,TEST_SUCCESS
```

menunjukkan bahwa translasi dan dekripsi FUSE sudah berhasil.

---

# Soal 3 - LibraryIT

## Deskripsi Soal

Pada soal ini, saya diminta membangun infrastruktur **IT Library Nusantara** menggunakan **Docker** dan **Samba**. Sistem ini berfungsi sebagai server file sharing untuk menyimpan beberapa jenis koleksi digital, yaitu:

* `ebooks`
* `papers`
* `sourcecode`
* `docs`

Server harus berjalan di dalam container bernama `libraryit-server`. Selain itu, sistem juga harus memiliki logger bernama `libraryit-logger` yang mencatat aktivitas akses file.

Terdapat tiga user Samba yang harus dibuat secara otomatis saat container dijalankan:

```text
member      : member123
contributor : contrib456
librarian   : lib789
```

Ketiga user tersebut dibagi ke dalam dua group:

```text
member      -> readonly
contributor -> staff
librarian   -> staff
```

Aturan akses setiap koleksi berbeda. `member` hanya boleh membaca koleksi tertentu, sedangkan `contributor` dan `librarian` memiliki hak akses lebih besar sesuai aturan soal.

---

## Struktur Folder

Struktur folder akhir sesuai ketentuan soal adalah sebagai berikut:

```text
soal_3/
├── Dockerfile
├── docker-compose.yml
├── smb.conf
├── entrypoint.sh
├── data/
│   ├── ebooks/
│   ├── papers/
│   ├── sourcecode/
│   └── docs/
└── logs/
    └── libraryit.log
```

Keterangan:

* `Dockerfile` digunakan untuk membuat image Samba server.
* `docker-compose.yml` digunakan untuk menjalankan service `libraryit-server` dan `libraryit-logger`.
* `smb.conf` berisi konfigurasi Samba dan hak akses share.
* `entrypoint.sh` digunakan untuk membuat user, group, permission folder, dan menjalankan Samba.
* `data/` adalah folder koleksi yang akan dibagikan melalui Samba.
* `logs/libraryit.log` adalah file log aktivitas LibraryIT.
* `logger.sh` digunakan oleh service logger untuk memantau aktivitas dan menulis log.

Karena folder kosong tidak masuk ke GitHub, folder koleksi yang kosong bisa diberi `.gitkeep`, misalnya:

```bash
touch data/ebooks/.gitkeep
touch data/papers/.gitkeep
touch data/sourcecode/.gitkeep
touch data/docs/.gitkeep
```

---

## Penjelasan Solusi

Solusi pada soal ini dibuat dengan membagi sistem menjadi dua service utama:

```text
libraryit-server = menjalankan Samba server
libraryit-logger = mencatat aktivitas akses file
```

Samba server menyediakan beberapa share:

```text
ebooks
papers
SourceCode
docs
```

Hak akses diatur berdasarkan user dan group:

| Koleksi | Hak Akses |
|---|---|
| `ebooks` | `readonly` bisa baca, `staff` bisa baca dan tulis |
| `papers` | `readonly` bisa baca, `staff` bisa baca dan tulis |
| `SourceCode` | hanya `staff` yang bisa akses |
| `docs` | semua group bisa baca, tetapi hanya `librarian` yang bisa menulis lewat Samba |

Selain itu, permission host juga diatur agar folder tertentu tidak bisa sembarangan ditulis langsung dari host. Contohnya, folder `docs` dibuat read-only dari sisi host, sehingga penulisan hanya dapat dilakukan melalui Samba oleh user `librarian`.

---

## Kode Program

## 1. File `Dockerfile`

```dockerfile
FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        samba \
        smbclient \
        rsyslog \
        coreutils \
        procps \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /entrypoint.sh
COPY logger.sh /usr/local/bin/logger.sh

RUN chmod +x /entrypoint.sh /usr/local/bin/logger.sh && \
    mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs /logs /run/samba && \
    printf 'local5.*    /logs/samba_audit.log\n& stop\n' > /etc/rsyslog.d/20-samba-audit.conf

EXPOSE 445 139

ENTRYPOINT ["/entrypoint.sh"]

```

---

## 2. File `docker-compose.yml`

```yaml
services:
  libraryit-server:
    build: .
    container_name: libraryit-server
    ports:
      - "1445:445"
    volumes:
      - ./data:/libraryit
      - ./logs:/logs
    restart: unless-stopped

  libraryit-logger:
    build: .
    container_name: libraryit-logger
    command: ["/usr/local/bin/logger.sh"]
    volumes:
      - ./logs:/logs
    depends_on:
      - libraryit-server
    restart: unless-stopped

```

---

## 3. File `smb.conf`

```conf
[global]
   workgroup = WORKGROUP
   server string = LibraryIT Server
   server role = standalone server
   security = user
   map to guest = never
   passdb backend = tdbsam

   smb ports = 445 139
   disable netbios = no
   load printers = no
   printing = bsd
   printcap name = /dev/null

   log file = /var/log/samba/log.%m
   max log size = 1000

   vfs objects = full_audit
   full_audit:prefix = USER=%u|SHARE=%S|IP=%I
   full_audit:success = connect disconnect opendir open mkdir rmdir rename unlink pwrite write
   full_audit:failure = connect opendir open mkdir rmdir rename unlink pwrite write
   full_audit:facility = LOCAL5
   full_audit:priority = NOTICE

[ebooks]
   path = /libraryit/ebooks
   browsable = yes
   valid users = @readonly @staff
   read only = yes
   write list = @staff
   create mask = 0664
   directory mask = 2775
   force group = staff

[papers]
   path = /libraryit/papers
   browsable = yes
   valid users = @readonly @staff
   read only = yes
   write list = @staff
   create mask = 0664
   directory mask = 2775
   force group = staff

[SourceCode]
   path = /libraryit/sourcecode
   browsable = yes
   valid users = @staff
   read only = no
   write list = @staff
   create mask = 0660
   directory mask = 2770
   force group = staff

[docs]
   path = /libraryit/docs
   browsable = yes
   valid users = @readonly @staff
   read only = yes
   write list = librarian
   create mask = 0644
   directory mask = 0755
   force user = root
   force group = staff

```

---

## 4. File `entrypoint.sh`

```bash
#!/bin/bash
set -e

mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs /logs /run/samba /var/log/samba

groupadd -f readonly
groupadd -f staff

create_user() {
    local username="$1"
    local password="$2"
    local groupname="$3"

    if ! id "$username" >/dev/null 2>&1; then
        useradd -M -s /usr/sbin/nologin -G "$groupname" "$username"
    else
        usermod -aG "$groupname" "$username"
    fi

    echo "$username:$password" | chpasswd

    if ! pdbedit -L 2>/dev/null | cut -d: -f1 | grep -qx "$username"; then
        printf '%s\n%s\n' "$password" "$password" | smbpasswd -s -a "$username" >/dev/null
    else
        printf '%s\n%s\n' "$password" "$password" | smbpasswd -s "$username" >/dev/null
    fi
    smbpasswd -e "$username" >/dev/null
}

create_user member member123 readonly
create_user contributor contrib456 staff
create_user librarian lib789 staff

# Permission koleksi.
# ebooks dan papers: staff bisa tulis, readonly hanya baca lewat Samba.
chown -R root:staff /libraryit/ebooks /libraryit/papers
chmod 2775 /libraryit/ebooks /libraryit/papers

# sourcecode: di host hanya owner dan grup yang bisa akses.
chown -R root:staff /libraryit/sourcecode
chmod 2750 /libraryit/sourcecode

# docs: dari host read-only. Penulisan hanya lewat Samba oleh user librarian.
chown -R root:staff /libraryit/docs
chmod 0555 /libraryit/docs

chown -R root:root /logs
chmod 0777 /logs
touch /logs/libraryit.log /logs/samba_audit.log
chmod 0666 /logs/libraryit.log /logs/samba_audit.log

rsyslogd || true

exec smbd -F --no-process-group

```

---

## 5. File `logger.sh`

```bash
#!/bin/bash
set -e

RAW_LOG="/logs/samba_audit.log"
OUT_LOG="/logs/libraryit.log"

mkdir -p /logs
touch "$RAW_LOG" "$OUT_LOG"
chmod 0666 "$RAW_LOG" "$OUT_LOG"

echo "libraryit-logger started; watching $RAW_LOG"

tail -n 0 -F "$RAW_LOG" | while IFS= read -r line; do
    [[ "$line" != *"smbd_audit:"* ]] && continue

    payload="${line#*smbd_audit: }"

    username="unknown"
    share="unknown"
    action="UNKNOWN"
    target="-"
    level="INFO"

    if [[ "$payload" =~ USER=([^\|]+) ]]; then
        username="${BASH_REMATCH[1]}"
    fi
    if [[ "$payload" =~ SHARE=([^\|]+) ]]; then
        share="${BASH_REMATCH[1]}"
    fi

    # Format umum full_audit: USER=...|SHARE=...|IP=...|operation|result|path
    IFS='|' read -ra parts <<< "$payload"
    op=""
    result=""
    path=""

    for i in "${!parts[@]}"; do
        case "${parts[$i]}" in
            connect|disconnect|opendir|open|mkdir|rmdir|rename|unlink|pwrite|write)
                op="${parts[$i]}"
                result="${parts[$((i+1))]:-}"
                path="${parts[$((i+2))]:-}"
                break
                ;;
        esac
    done

    if [[ "$result" == fail* || "$payload" == *"|fail"* || "$payload" == *"NT_STATUS_ACCESS_DENIED"* ]]; then
        level="WARNING"
        action="DENIED"
    else
        case "$op" in
            connect) action="CONNECT" ;;
            disconnect) action="DISCONNECT" ;;
            opendir|open) action="READ" ;;
            pwrite|write|mkdir|rmdir|rename|unlink) action="WRITE" ;;
            *) action="ACCESS" ;;
        esac
    fi

    if [[ -n "$path" && "$path" != "-" ]]; then
        target="$path"
    else
        target="$share"
    fi

    timestamp="$(date '+%Y-%m-%d %H:%M:%S')"
    printf '[%s] [%s] [%s] [%s] [%s]\n' "$timestamp" "$level" "$username" "$action" "$target" | tee -a "$OUT_LOG"
done

```

---

## Penjelasan Kode per Bagian

## A. Penjelasan `Dockerfile`

`Dockerfile` digunakan untuk membuat image server LibraryIT.

Secara umum, Dockerfile melakukan beberapa hal:

1. Menggunakan base image Linux.
2. Menginstall Samba dan dependency yang dibutuhkan.
3. Menyalin file konfigurasi seperti `smb.conf`, `entrypoint.sh`, dan `logger.sh`.
4. Memberikan permission execute ke script.
5. Menjalankan entrypoint saat container aktif.

Dengan Dockerfile ini, environment Samba dapat dibuat otomatis tanpa perlu konfigurasi manual satu per satu di host.

---

## B. Penjelasan `docker-compose.yml`

`docker-compose.yml` digunakan untuk menjalankan beberapa service sekaligus.

Service utama yang digunakan adalah:

```text
libraryit-server
libraryit-logger
```

`libraryit-server` bertugas menjalankan Samba. Service ini melakukan bind mount folder `data/` dari host ke dalam container agar data tetap tersimpan walaupun container dimatikan.

`libraryit-logger` bertugas memantau log aktivitas dan menuliskannya ke file log LibraryIT.

Dengan Docker Compose, kedua service dapat dijalankan cukup dengan satu command:

```bash
sudo docker-compose up -d --build
```

---

## C. Penjelasan `entrypoint.sh`

File `entrypoint.sh` adalah script awal yang dijalankan saat container `libraryit-server` menyala.

### 1. Membuat folder yang dibutuhkan

```bash
mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs /logs /run/samba /var/log/samba
```

Baris ini memastikan semua folder koleksi dan log tersedia di dalam container.

---

### 2. Membuat group

```bash
groupadd -f readonly
groupadd -f staff
```

Dua group dibuat sesuai kebutuhan soal:

* `readonly` untuk user `member`
* `staff` untuk user `contributor` dan `librarian`

Opsi `-f` membuat command tidak error jika group sudah ada.

---

### 3. Fungsi `create_user`

```bash
create_user() {
    local username="$1"
    local password="$2"
    local groupname="$3"
    ...
}
```

Fungsi ini digunakan untuk membuat user Linux sekaligus user Samba.

Di dalam fungsi ini, program melakukan:

* membuat user jika belum ada
* memasukkan user ke group yang sesuai
* mengatur password Linux
* menambahkan user ke database Samba menggunakan `smbpasswd`
* mengaktifkan user Samba

---

### 4. Membuat tiga user soal

```bash
create_user member member123 readonly
create_user contributor contrib456 staff
create_user librarian lib789 staff
```

Bagian ini membuat user sesuai ketentuan soal.

Hasilnya:

```text
member      masuk group readonly
contributor masuk group staff
librarian   masuk group staff
```

---

### 5. Permission folder `ebooks` dan `papers`

```bash
chown -R root:staff /libraryit/ebooks /libraryit/papers
chmod 2775 /libraryit/ebooks /libraryit/papers
```

Folder `ebooks` dan `papers` dimiliki oleh `root:staff`.

Permission `2775` berarti:

* owner bisa baca, tulis, dan masuk folder
* group `staff` bisa baca, tulis, dan masuk folder
* user lain bisa baca dan masuk folder
* angka `2` di depan mengaktifkan setgid agar file baru mengikuti group `staff`

Di Samba, `member` tetap hanya diberi akses baca, sedangkan `staff` bisa menulis.

---

### 6. Permission folder `sourcecode`

```bash
chown -R root:staff /libraryit/sourcecode
chmod 2750 /libraryit/sourcecode
```

Folder `sourcecode` hanya bisa diakses oleh owner dan group `staff`.

Permission `2750` berarti:

* owner bisa penuh
* group `staff` bisa baca, tulis, dan masuk folder
* user lain tidak punya akses

Ini sesuai aturan bahwa `member` dari group `readonly` tidak boleh mengakses `SourceCode`.

---

### 7. Permission folder `docs`

```bash
chown -R root:staff /libraryit/docs
chmod 0555 /libraryit/docs
```

Folder `docs` dibuat read-only dari sisi host. Permission `0555` berarti semua user hanya bisa membaca dan masuk folder, tetapi tidak bisa menulis langsung.

Namun pada konfigurasi Samba, user `librarian` tetap diizinkan menulis lewat share `docs` dengan bantuan konfigurasi `write list` dan `force user`.

---

### 8. Menyiapkan folder log

```bash
chown -R root:root /logs
chmod 0777 /logs
touch /logs/libraryit.log /logs/samba_audit.log
chmod 0666 /logs/libraryit.log /logs/samba_audit.log
```

Bagian ini membuat file log dapat ditulis oleh service yang membutuhkan. File utama yang diperiksa adalah:

```text
/logs/libraryit.log
```

---

### 9. Menjalankan Samba

```bash
rsyslogd || true

exec smbd -F --no-process-group
```

`rsyslogd` dijalankan untuk membantu pencatatan log Samba. Setelah itu, `smbd` dijalankan dalam foreground agar container tetap hidup.

---

## D. Penjelasan `smb.conf`

File `smb.conf` mengatur konfigurasi Samba dan share yang tersedia.

### 1. Bagian global

```conf
[global]
   workgroup = WORKGROUP
   server string = LibraryIT Server
   server role = standalone server
   security = user
   map to guest = never
```

Bagian ini mengatur Samba sebagai server standalone dengan sistem autentikasi user. `map to guest = never` membuat akses tanpa identitas tidak diperbolehkan.

---

### 2. Konfigurasi audit log

```conf
vfs objects = full_audit
full_audit:prefix = USER=%u|SHARE=%S|IP=%I
full_audit:success = connect disconnect opendir open mkdir rmdir rename unlink pwrite write
full_audit:failure = connect opendir open mkdir rmdir rename unlink pwrite write
```

Bagian ini mengaktifkan audit log Samba. Aktivitas seperti connect, open, mkdir, unlink, dan write dapat dicatat.

Log mentah dari Samba kemudian diproses oleh service logger agar formatnya menjadi lebih mudah dibaca.

---

### 3. Share `ebooks`

```conf
[ebooks]
   path = /libraryit/ebooks
   browsable = yes
   valid users = @readonly @staff
   read only = yes
   write list = @staff
```

`ebooks` bisa diakses oleh `readonly` dan `staff`. Namun secara default read-only. Group `staff` masuk ke `write list`, sehingga contributor dan librarian bisa menulis.

---

### 4. Share `papers`

```conf
[papers]
   path = /libraryit/papers
   browsable = yes
   valid users = @readonly @staff
   read only = yes
   write list = @staff
```

Konsepnya sama seperti `ebooks`.

---

### 5. Share `SourceCode`

```conf
[SourceCode]
   path = /libraryit/sourcecode
   browsable = yes
   valid users = @staff
   read only = no
   write list = @staff
```

Share `SourceCode` hanya bisa diakses oleh group `staff`. Karena `member` hanya berada di group `readonly`, maka `member` tidak boleh membuka share ini.

---

### 6. Share `docs`

```conf
[docs]
   path = /libraryit/docs
   browsable = yes
   valid users = @readonly @staff
   read only = yes
   write list = librarian
   force user = root
   force group = staff
```

Share `docs` bisa dibaca oleh user dari group `readonly` dan `staff`, tetapi hanya user `librarian` yang boleh menulis.

`force user = root` membantu agar penulisan lewat Samba dapat tetap dilakukan meskipun folder di host dibuat read-only untuk akses langsung.

---

## E. Penjelasan `logger.sh`

`logger.sh` bertugas membaca log mentah Samba dan mengubahnya menjadi format log LibraryIT.

### 1. File log yang digunakan

```bash
RAW_LOG="/logs/samba_audit.log"
OUT_LOG="/logs/libraryit.log"
```

`RAW_LOG` adalah log mentah dari Samba. `OUT_LOG` adalah log akhir yang dibaca sebagai hasil soal.

---

### 2. Memantau log secara real-time

```bash
tail -n 0 -F "$RAW_LOG" | while IFS= read -r line; do
```

Command ini memantau perubahan file log secara terus-menerus. Setiap ada aktivitas baru dari Samba, logger akan membaca baris tersebut.

---

### 3. Mengambil username dan share

```bash
if [[ "$payload" =~ USER=([^\|]+) ]]; then
    username="${BASH_REMATCH[1]}"
fi
if [[ "$payload" =~ SHARE=([^\|]+) ]]; then
    share="${BASH_REMATCH[1]}"
fi
```

Bagian ini mengambil username dan nama share dari log mentah.

---

### 4. Menentukan level dan aksi

Jika aktivitas gagal atau mengandung `NT_STATUS_ACCESS_DENIED`, logger memberi level:

```text
WARNING
```

dan aksi:

```text
DENIED
```

Jika aktivitas berhasil, logger mengubah operasi Samba menjadi aksi yang lebih mudah dibaca:

```text
connect    -> CONNECT
open       -> READ
pwrite     -> WRITE
mkdir      -> WRITE
unlink     -> WRITE
```

---

### 5. Format output log

```bash
printf '[%s] [%s] [%s] [%s] [%s]\n' "$timestamp" "$level" "$username" "$action" "$target"
```

Format log akhir menjadi:

```text
[YYYY-MM-DD HH:MM:SS] [LEVEL] [USERNAME] [AKSI] [FILE/SHARE]
```

Contoh:

```text
[2026-05-16 02:19:06] [INFO] [filesystem] [CREATE] [SourceCode/hello_final.py]
```

---

## Cara Menjalankan

### 1. Masuk ke folder Soal 3

```bash
cd soal_3
```

Atau sesuaikan dengan lokasi folder masing-masing.

---

### 2. Beri permission pada script

```bash
chmod +x entrypoint.sh
chmod +x logger.sh
```

---

### 3. Jalankan Docker Compose

Jika menggunakan Docker Compose versi lama:

```bash
sudo docker-compose down
sudo docker-compose up -d --build
```

Jika menggunakan Docker Compose versi baru:

```bash
sudo docker compose down
sudo docker compose up -d --build
```

---

### 4. Cek container

```bash
sudo docker ps
```

Container yang diharapkan muncul:

```text
libraryit-server
libraryit-logger
```

---

## Pengujian

## 1. Cek user Samba

```bash
sudo docker exec -it libraryit-server pdbedit -L
```

Output yang diharapkan berisi:

```text
member
contributor
librarian
```

---

## 2. Cek group

```bash
sudo docker exec -it libraryit-server getent group staff readonly
```

Output yang diharapkan menunjukkan:

```text
staff
readonly
```

---

## 3. Cek folder koleksi

```bash
sudo docker exec -it libraryit-server ls /libraryit
```

Output yang diharapkan:

```text
docs ebooks papers sourcecode
```

---

## 4. Test akses `member` ke `SourceCode`

```bash
smbclient //127.0.0.1/SourceCode -p 1445 -U 'member%member123'
```

Output yang benar:

```text
NT_STATUS_ACCESS_DENIED
```

Ini bukan error pengerjaan, tetapi bukti bahwa `member` memang tidak boleh mengakses `SourceCode`.

---

## 5. Test contributor menulis ke `SourceCode`

```bash
smbclient //127.0.0.1/SourceCode -p 1445 -U 'contributor%contrib456'
```

Di prompt Samba:

```text
put /etc/hostname hello_final.py
ls
exit
```

Jika berhasil, berarti user `contributor` dari group `staff` dapat menulis ke `SourceCode`.

---

## 6. Test host tidak boleh menulis langsung ke `docs`

Dari folder Soal 3 di host, jalankan:

```bash
touch ./data/docs/test_dari_host.txt
```

Output yang benar:

```text
Permission denied
```

Ini sesuai dengan aturan soal karena folder `docs` tidak boleh ditulis langsung dari host.

---

## 7. Test `librarian` menulis ke `docs` lewat Samba

```bash
smbclient //127.0.0.1/docs -p 1445 -U 'librarian%lib789'
```

Di prompt Samba:

```text
put /etc/hostname test.txt
ls
exit
```

Jika berhasil, berarti `librarian` bisa menulis ke `docs` melalui Samba.

---

## 8. Cek log

```bash
cat logs/libraryit.log
```

Contoh output yang diharapkan:

```text
[LOGGER] libraryit-logger started
[2026-05-16 02:19:06] [INFO] [filesystem] [CREATE] [SourceCode/hello_final.py]
[2026-05-16 02:19:06] [INFO] [filesystem] [WRITE] [SourceCode/hello_final.py]
[2026-05-16 02:20:19] [INFO] [filesystem] [CREATE] [docs/test.txt]
[2026-05-16 02:20:19] [INFO] [filesystem] [WRITE] [docs/test.txt]
```

Format log dapat berbeda sedikit tergantung implementasi logger, tetapi intinya aktivitas akses dan penulisan file harus tercatat di `libraryit.log`.

---

## Kendala yang Dihadapi

Kendala pertama adalah masalah **permission denied** pada folder `docs`. Saat mencoba membuat file langsung dari host dengan command:

```bash
touch ./data/docs/test_dari_host.txt
```

muncul error:

```text
Permission denied
```

Awalnya ini terlihat seperti error, tetapi setelah dicek kembali, hal ini memang sesuai dengan ketentuan soal. Folder `docs` harus read-only dari host dan hanya boleh ditulis melalui Samba oleh user `librarian`.

Solusinya adalah tidak menulis langsung dari host, tetapi masuk lewat Samba:

```bash
smbclient //127.0.0.1/docs -p 1445 -U 'librarian%lib789'
```

lalu upload file menggunakan:

```text
put /etc/hostname test.txt
```

---

Kendala kedua adalah `member` tidak bisa mengakses `SourceCode`.

Saat menjalankan:

```bash
smbclient //127.0.0.1/SourceCode -p 1445 -U 'member%member123'
```

muncul output:

```text
NT_STATUS_ACCESS_DENIED
```

Awalnya ini bisa terlihat seperti error koneksi, tetapi sebenarnya ini adalah output yang benar karena `SourceCode` hanya boleh diakses oleh group `staff`. User `member` berada di group `readonly`, sehingga aksesnya harus ditolak.

---

Kendala ketiga adalah contributor tidak boleh menulis ke `docs`.

Saat mencoba upload file ke `docs` menggunakan user `contributor`, operasi `put` ditolak. Hal ini terjadi karena pada konfigurasi Samba, share `docs` hanya memberi hak tulis kepada user `librarian`.

Bagian konfigurasi yang mengatur hal tersebut adalah:

```conf
[docs]
   read only = yes
   write list = librarian
```

Solusinya adalah menggunakan user `librarian` untuk menulis ke `docs`.

---

Kendala keempat adalah permission folder berubah setelah container dijalankan.

Karena folder `data/` di-bind ke container, ownership dan permission pada host bisa berubah mengikuti konfigurasi di dalam container. Akibatnya, saat ingin membersihkan file testing dari host, terkadang perlu menggunakan `sudo`.

Solusi yang digunakan:

```bash
sudo docker-compose down
sudo chown -R $USER:$USER data logs
sudo chmod 755 data/docs
```

Setelah permission dikembalikan, file testing dapat dihapus dengan lebih mudah.

---

Kendala kelima adalah logger sempat tidak sesuai harapan ketika menggunakan audit Samba secara langsung. Format log mentah dari Samba tidak langsung rapi untuk dibaca sebagai hasil akhir, sehingga perlu service logger terpisah untuk memproses log tersebut menjadi format:

```text
[YYYY-MM-DD HH:MM:SS] [LEVEL] [USERNAME] [AKSI] [FILE/SHARE]
```

Solusinya adalah membuat service `libraryit-logger` yang membaca log mentah dan menuliskannya ulang ke `logs/libraryit.log`.

---

Kendala keenam adalah folder kosong tidak ikut masuk GitHub.

Folder berikut wajib ada sesuai struktur soal:

```text
data/ebooks/
data/papers/
data/sourcecode/
data/docs/
```

Namun GitHub tidak menyimpan folder kosong. Solusinya adalah menambahkan `.gitkeep` pada setiap folder kosong:

```bash
touch data/ebooks/.gitkeep
touch data/papers/.gitkeep
touch data/sourcecode/.gitkeep
touch data/docs/.gitkeep
```

---

## Cleanup Sebelum Upload

Matikan container terlebih dahulu:

```bash
sudo docker-compose down
```

Atau jika memakai Docker Compose versi baru:

```bash
sudo docker compose down
```

Hapus file hasil testing:

```bash
sudo rm -f data/sourcecode/hello_final.py
sudo rm -f data/docs/test.txt
sudo rm -f data/docs/test_dari_host.txt
```

Kosongkan log jika diperlukan:

```bash
sudo sh -c ': > logs/libraryit.log'
```

Pastikan struktur akhir:

```bash
tree -a .
```

Target struktur akhir:

```text
.
├── Dockerfile
├── docker-compose.yml
├── smb.conf
├── entrypoint.sh
├── data
│   ├── docs
│   │   └── .gitkeep
│   ├── ebooks
│   │   └── .gitkeep
│   ├── papers
│   │   └── .gitkeep
│   └── sourcecode
│       └── .gitkeep
└── logs
    └── libraryit.log
```

---

## Kesimpulan

Pada soal ini, saya belajar membangun file sharing server menggunakan Samba di dalam Docker. Sistem LibraryIT memiliki beberapa user dengan hak akses berbeda, yaitu `member`, `contributor`, dan `librarian`.

Hak akses diatur melalui kombinasi user, group, permission folder Linux, dan konfigurasi Samba. Selain itu, aktivitas akses file juga dicatat melalui logger sehingga sistem dapat memantau siapa yang mengakses atau mencoba mengakses koleksi tertentu.

Hasil akhir menunjukkan bahwa:

* container `libraryit-server` dan `libraryit-logger` dapat berjalan
* user `member`, `contributor`, dan `librarian` berhasil dibuat
* `member` tidak bisa mengakses `SourceCode`
* `contributor` bisa menulis ke `SourceCode`
* host tidak bisa menulis langsung ke `docs`
* `librarian` bisa menulis ke `docs` melalui Samba
* aktivitas file tercatat di `logs/libraryit.log`
