/* ZeeHID Probe file layer: path allowlist and save by temporary file.
 * Requirements: IM-009, IM-010, LQ-005, TR-007, CON-1, HZ-1. Tests: CT-007, CT-008.
 * Portable C99. The real file calls live behind zhp_fs_ops (BREW glue implements them). */
#ifndef ZHP_FS_H
#define ZHP_FS_H

#include <stddef.h>
#include <stdint.h>

#define ZHP_FS_PATH_MAX 255
#define ZHP_FS_ENOENT (-2) /* remove(): the file did not exist, which is fine */

typedef struct {
    const char *data_dir; /* the app data folder, no trailing slash */
    const char *sd_dir;   /* SD log folder, no trailing slash, may be NULL */
    uint32_t refused;
} zhp_fs;

void zhp_fs_init(zhp_fs *fs, const char *data_dir, const char *sd_dir);

/* 1 if the path may be written: under data_dir or sd_dir, no "..", no "." component, no empty
 * component, no backslash, at most ZHP_FS_PATH_MAX characters. Otherwise 0 and refused++. */
int zhp_fs_allow(zhp_fs *fs, const char *path);

typedef struct {
    void *ctx;
    int (*write)(void *ctx, const char *path, const uint8_t *data, size_t len);
    int (*remove)(void *ctx, const char *path);
    int (*rename)(void *ctx, const char *from, const char *to);
} zhp_fs_ops;

typedef enum {
    ZHP_SAVE_OK = 0,
    ZHP_SAVE_WRITE_FAILED = 1,
    ZHP_SAVE_REMOVE_FAILED = 2,
    ZHP_SAVE_RENAME_FAILED = 3,
    ZHP_SAVE_DENIED = 4
} zhp_save_step;

typedef struct {
    zhp_save_step step; /* the step that failed, or ZHP_SAVE_OK */
    int err;            /* the code returned by that step */
} zhp_save_result;

/* Write path.tmp, remove path (a missing file is fine), rename path.tmp to path.
 * BREW's rename fails when the destination exists, hence the remove. On failure the temporary file
 * is left in place as the recovery copy. Both paths must pass zhp_fs_allow. */
zhp_save_result zhp_fs_save(zhp_fs *fs, const zhp_fs_ops *ops, const char *path, const uint8_t *data, size_t len);

#endif
