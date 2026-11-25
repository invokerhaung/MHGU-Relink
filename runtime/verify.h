// Verify game files: a full read of every file of the player's dump (exefs/ and romfs/), so a copy that lost or cut
// short a file is found before the game crashes or draws black where it reads it. Nothing is changed or downloaded.
// The result goes to the log; a short summary is returned for the settings menu's notice.
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reads every file under g_game_dir (exefs and romfs), fully: a file whose read comes back short of its size, or that
// can't be opened, is a problem. Counts files and bytes and an aggregate CRC32 (over each file's size and CRC, in the
// walk's order), and compares the romfs file count with 1.4.0's. Writes a human summary into msg (and the full list to
// the log) and returns true when the files are fine. Runs on the calling thread (a few minutes with 12 GB; Android
// calls it on a worker thread).
int verify_game_files(char *msg, size_t size);

#ifdef __cplusplus
}
#endif
