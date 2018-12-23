#ifndef WIN32_PORT_HPP
#define WIN32_PORT_HPP

typedef _dev_t dev_t;

struct w32_stat {
    dev_t     st_dev;     /* ID of device containing file */
    unsigned short     st_ino;     /* inode number */
    unsigned short    st_mode;    /* protection */
    short    st_nlink;   /* number of hard links */
    short     st_uid;     /* user ID of owner */
    short     st_gid;     /* group ID of owner */
    dev_t     st_rdev;    /* device ID (if special file) */
    __int64     st_size;    /* total size, in bytes */
    __int64    st_atime;   /* time of last access */
    __int64    st_mtime;   /* time of last modification */
    __int64    st_ctime;   /* time of last status change */
};


/**
 *
 * macro to make win platform same as linux
 */
int w32_stat(const char *path, struct w32_stat *buf);
#define stat w32_stat
#define lstat w32_stat

#endif
