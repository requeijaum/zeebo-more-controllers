#include "zhp_fs.h"

#include <string.h>

void zhp_fs_init(zhp_fs *fs, const char *data_dir, const char *sd_dir)
{
    fs->data_dir = data_dir;
    fs->sd_dir = sd_dir;
    fs->refused = 0;
}

static int under(const char *dir, const char *path)
{
    size_t n;
    if (!dir || !dir[0]) {
        return 0;
    }
    n = strlen(dir);
    if (n > 0 && dir[n - 1] == '/') {
        return 0; /* the configured folder must not end with a slash */
    }
    return strncmp(path, dir, n) == 0 && path[n] == '/' && path[n + 1] != '\0';
}

static int clean_components(const char *path)
{
    const char *p = path;
    /* every component between slashes must be non-empty and not "." or ".." */
    while (*p) {
        const char *q;
        size_t n;
        if (*p == '/') {
            p++;
            if (*p == '/' ) {
                return 0; /* empty component */
            }
            if (!*p) {
                return 0; /* trailing slash */
            }
        }
        q = p;
        while (*q && *q != '/') {
            if (*q == '\\') {
                return 0;
            }
            q++;
        }
        n = (size_t)(q - p);
        if (n == 0 || (n == 1 && p[0] == '.') || (n == 2 && p[0] == '.' && p[1] == '.')) {
            return 0;
        }
        p = q;
    }
    return 1;
}

int zhp_fs_allow(zhp_fs *fs, const char *path)
{
    size_t n;
    int ok = 0;
    if (path && path[0]) {
        n = strlen(path);
        if (n <= ZHP_FS_PATH_MAX && clean_components(path) &&
            (under(fs->data_dir, path) || under(fs->sd_dir, path))) {
            ok = 1;
        }
    }
    if (!ok) {
        fs->refused++;
    }
    return ok;
}

zhp_save_result zhp_fs_save(zhp_fs *fs, const zhp_fs_ops *ops, const char *path, const uint8_t *data, size_t len)
{
    zhp_save_result r;
    char tmp[ZHP_FS_PATH_MAX + 5];
    size_t n;
    int rc;
    r.step = ZHP_SAVE_OK;
    r.err = 0;
    if (!path || !zhp_fs_allow(fs, path)) {
        r.step = ZHP_SAVE_DENIED;
        return r;
    }
    n = strlen(path);
    memcpy(tmp, path, n);
    memcpy(tmp + n, ".tmp", 5);
    if (!zhp_fs_allow(fs, tmp)) {
        r.step = ZHP_SAVE_DENIED;
        return r;
    }
    rc = ops->write(ops->ctx, tmp, data, len);
    if (rc != 0) {
        r.step = ZHP_SAVE_WRITE_FAILED;
        r.err = rc;
        return r;
    }
    rc = ops->remove(ops->ctx, path);
    if (rc != 0 && rc != ZHP_FS_ENOENT) {
        r.step = ZHP_SAVE_REMOVE_FAILED;
        r.err = rc;
        return r;
    }
    rc = ops->rename(ops->ctx, tmp, path);
    if (rc != 0) {
        r.step = ZHP_SAVE_RENAME_FAILED;
        r.err = rc;
        return r;
    }
    return r;
}
