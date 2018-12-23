#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "Windows.h"
#include <iostream>
//#include "asyncrt_utils.h"

#define LOG(A) std::cout << std::endl; std::cout
/* Difference in us between UNIX Epoch and Win32 Epoch */
#define EPOCH_DELTA_US  116444736000000000ULL
#define RATE_DIFF 10000000ULL /* 1000 nsecs */

/* maps Win32 error to errno */
int errno_from_Win32Error(int win32_error){
  switch (win32_error) {
  case ERROR_ACCESS_DENIED:
    return EACCES;
  case ERROR_OUTOFMEMORY:
    return ENOMEM;
  case ERROR_FILE_EXISTS:
    return EEXIST;
  case ERROR_FILE_NOT_FOUND:
    return ENOENT;
  default:
    return win32_error;
  }
}

#define errno_from_Win32LastError() errno_from_Win32Error(GetLastError())

int file_attr_to_st_mode(wchar_t * path, DWORD attributes){
  int mode = S_IREAD;
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 || is_root_or_empty(path))
    mode |= S_IFDIR | _S_IEXEC;
  else {
    mode |= S_IFREG;
    /* See if file appears to be an executable by checking its extension */
    if (has_executable_extension(path))
      mode |= _S_IEXEC;

  }
  if (!(attributes & FILE_ATTRIBUTE_READONLY))
    mode |= S_IWRITE;

  // propagate owner read/write/execute bits to group/other fields.
  mode |= (mode & 0700) >> 3;
  mode |= (mode & 0700) >> 6;
  return mode;
}

/* Convert a Windows file time into a UNIX time_t */
void file_time_to_unix_time(const LPFILETIME pft, time_t * winTime){
  *winTime = ((long long)pft->dwHighDateTime << 32) + pft->dwLowDateTime;
  *winTime -= EPOCH_DELTA_US;
  *winTime /= RATE_DIFF;       /* Nano to seconds resolution */
}

wchar_t *
utf8_to_utf16(const char *utf8)
{
    int needed = 0;
    wchar_t* utf16 = NULL;
    if ((needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0)) == 0 ||
        (utf16 = malloc(needed * sizeof(wchar_t))) == NULL ||
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16, needed) == 0)
        return NULL;

    return utf16;
}

int fileio_stat(const char *path, struct _stat64 *buf){
  wchar_t* wpath = NULL;
  WIN32_FILE_ATTRIBUTE_DATA attributes = { 0 };
  int ret = -1, len = 0;

  memset(buf, 0, sizeof(struct _stat64));

  /* Detect root dir */
  if (path && strcmp(path, "/") == 0) {
    buf->st_mode = _S_IFDIR | _S_IREAD | 0xFF;
    buf->st_dev = USHRT_MAX;   // rootdir flag
    return 0;
  }

  if ((wpath = utf8_to_utf16(path)) == NULL) {
    errno = ENOMEM;
    LOG(XM_DEBUG) << "utf8_to_utf16 failed for file:%s error:%d", path, GetLastError();
    return -1;
  }

  if (GetFileAttributesExW(wpath, GetFileExInfoStandard, &attributes) == FALSE) {
    errno = errno_from_Win32LastError();
    LOG(XM_DEBUG) << "GetFileAttributesExW with last error " << GetLastError();
    goto cleanup;
  }

  len = (int)wcslen(wpath);

  buf->st_ino = 0; /* Has no meaning in the FAT, HPFS, or NTFS file systems*/
  buf->st_gid = 0; /* UNIX - specific; has no meaning on windows */
  buf->st_uid = 0; /* UNIX - specific; has no meaning on windows */
  buf->st_nlink = 1; /* number of hard links. Always 1 on non - NTFS file systems.*/
  buf->st_mode |= file_attr_to_st_mode(wpath, attributes.dwFileAttributes);
  buf->st_size = attributes.nFileSizeLow | (((off_t)attributes.nFileSizeHigh) << 32);
  if (len > 1 && __ascii_iswalpha(*wpath) && (*(wpath + 1) == ':'))
    buf->st_dev = buf->st_rdev = towupper(*wpath) - L'A'; /* drive num */
  else
    buf->st_dev = buf->st_rdev = _getdrive() - 1;
  file_time_to_unix_time(&(attributes.ftLastAccessTime), &(buf->st_atime));
  file_time_to_unix_time(&(attributes.ftLastWriteTime), &(buf->st_mtime));
  file_time_to_unix_time(&(attributes.ftCreationTime), &(buf->st_ctime));

  if (attributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    WIN32_FIND_DATAW findbuf = { 0 };
    HANDLE handle = FindFirstFileW(wpath, &findbuf);
    if (handle != INVALID_HANDLE_VALUE) {
      if ((findbuf.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
          (findbuf.dwReserved0 == IO_REPARSE_TAG_SYMLINK)) {
        buf->st_mode |= S_IFLNK;
      }
      FindClose(handle);
    }
  }
  ret = 0;
 cleanup:
  if (wpath)
    free(wpath);
  return ret;
}


int w32_stat(const char *path, struct w32_stat *buf){
  return fileio_stat(sanitized_path(path), (struct _stat64*)buf);
}
