// Verify game files (verify.h): a full read of every file of the dump.
#include "verify.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "rt.h"
#include "config.h"  // config_language(): the settings menu's language, for the summary it shows

#define ROMFS_1_4_0_FILES 35531  // 1.4.0 merged romfs (the count game_files_problem() checks at start)

typedef struct {
    unsigned long long files, bytes, bad;  // bad: unreadable or short reads
    uint32_t crc;                          // over each file's size (LE) then its bytes, in walk order
    char bad_list[2048];                   // the first few bad files, for the message
} Tally;

static void crc_bytes(uint32_t *acc, const unsigned char *p, size_t n) {
    uint32_t a = *acc;
    for (size_t i = 0; i < n; i++) a = rt_crc32_tab[(a ^ p[i]) & 0xFF] ^ (a >> 8);
    *acc = a;
}

// One file: its full read updates the tally. Returns 0 fine, 1 unreadable, 2 short.
static int one_file(const char *path, Tally *t) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (t->bad < 20 && strlen(t->bad_list) + strlen(path) + 2 < sizeof t->bad_list)
            strcat(t->bad_list, "  missing/unreadable: "), strcat(t->bad_list, path), strcat(t->bad_list, "\n");
        t->bad++;
        return 1;
    }
    uint64_t want = (uint64_t)st.st_size, got = 0;
    uint32_t crc = 0xFFFFFFFFu;
    unsigned char buf[64 * 1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        got += n;
        crc_bytes(&crc, buf, n);
    }
    fclose(f);
    t->files++;
    t->bytes += got;
    t->crc = rt_crc32_tab[(t->crc ^ (uint32_t)got) & 0xFF] ^ (t->crc >> 8);  // fold the size in
    t->crc ^= crc ^ 0xFFFFFFFFu;                                           // then the contents
    if (got != want) {
        if (t->bad < 20 && strlen(t->bad_list) + strlen(path) + 64 < sizeof t->bad_list)
            snprintf(t->bad_list + strlen(t->bad_list), sizeof t->bad_list - strlen(t->bad_list),
                     "  cut short: %s (%llu of %llu bytes)\n", path, (unsigned long long)got, (unsigned long long)want);
        t->bad++;
        return 2;
    }
    return 0;
}

static void walk(const char *dir, int depth, Tally *t) {
    if (depth > 40) return;
    DIR *dd = opendir(dir);
    if (!dd) return;
    struct dirent *e;
    char path[2048];
    while ((e = readdir(dd))) {
        if (e->d_name[0] == '.') continue;
        if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path) continue;
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) walk(path, depth + 1, t);
        else if (S_ISREG(st.st_mode)) one_file(path, t);
    }
    closedir(dd);
}

int verify_game_files(char *msg, size_t size) {
    // the exefs (small) and the romfs (the 12.3 GB): read them all; the romfs is tallied apart for its file-count check
    char romfs[1200];
    snprintf(romfs, sizeof romfs, "%s/romfs", g_game_dir);
    Tally rf;
    memset(&rf, 0, sizeof rf);
    rf.crc = 0xFFFFFFFFu;
    walk(romfs, 0, &rf);
    char exefs[1200];
    snprintf(exefs, sizeof exefs, "%s/exefs", g_game_dir);
    Tally ef;
    memset(&ef, 0, sizeof ef);
    ef.crc = 0xFFFFFFFFu;
    walk(exefs, 0, &ef);

    unsigned long long files = rf.files + ef.files, bytes = rf.bytes + ef.bytes, bad = rf.bad + ef.bad;
    LOG("[verify] exefs: %llu files, %llu bytes, %llu bad; romfs: %llu files, %llu bytes, %llu bad\n",
        (unsigned long long)ef.files, (unsigned long long)ef.bytes, (unsigned long long)ef.bad,
        (unsigned long long)rf.files, (unsigned long long)rf.bytes, (unsigned long long)rf.bad);
    LOG("[verify] romfs crc=%08x exefs crc=%08x\n", rf.crc, ef.crc);
    if (rf.bad_list[0]) LOG("[verify] bad files:\n%s", rf.bad_list);
    if (ef.bad_list[0]) LOG("[verify] bad files (exefs):\n%s", ef.bad_list);

    int lang = config_language();  // 0: English, 1: Spanish, 2: Chinese
    if (bad) {
        if (lang == 2)
            snprintf(msg, size,
                     "有 %llu 个文件缺失、不完整或无法读取（已读取 %llu 个文件）。请重新复制游戏文件，并确认 romfs "
                     "大小约为 12.3 GB。详细列表见日志。",
                     bad, files);
        else if (lang == 1)
            snprintf(msg, size,
                     "%llu archivo(s) faltan, están cortos o no se pueden leer (%llu archivos leídos). Copia de nuevo "
                     "los archivos del juego y comprueba que romfs ocupa unos 12,3 GB. La lista está en el registro.",
                     bad, files);
        else
            snprintf(msg, size,
                     "%llu file(s) are missing, cut short or unreadable (%llu files read). Copy the game files again, and "
                     "check that romfs holds about 12.3 GB. The list is in the log.",
                     bad, files);
        return 0;
    }
    if (rf.files < ROMFS_1_4_0_FILES) {
        if (lang == 2)
            snprintf(msg, size,
                     "游戏文件读取正常，但 romfs 有 %llu 个文件，而 1.4.0 版本应有 %d 个：有部分文件缺失。请在应用 1.4.0 "
                     "更新的情况下重新解压 romfs。",
                     (unsigned long long)rf.files, ROMFS_1_4_0_FILES);
        else if (lang == 1)
            snprintf(msg, size,
                     "Los archivos del juego se leen bien, pero romfs tiene %llu archivos donde la 1.4.0 tiene %d: faltan "
                     "algunos. Extrae de nuevo la romfs con la actualización 1.4.0 aplicada.",
                     (unsigned long long)rf.files, ROMFS_1_4_0_FILES);
        else
            snprintf(msg, size,
                     "The game files read fine, but romfs has %llu files where 1.4.0 has %d: some are missing. Extract the "
                     "romfs again with the 1.4.0 update applied.",
                     (unsigned long long)rf.files, ROMFS_1_4_0_FILES);
        return 0;
    }
    if (lang == 2)
        snprintf(msg, size, "游戏文件正常：已完整读取 %llu 个文件（%llu 字节）。", files, bytes);
    else if (lang == 1)
        snprintf(msg, size, "Los archivos del juego están bien: %llu archivos (%llu bytes) leídos por completo.", files, bytes);
    else
        snprintf(msg, size, "The game files are fine: %llu files (%llu bytes) read in full.", files, bytes);
    return 1;
}
