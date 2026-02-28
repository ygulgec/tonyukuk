#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

#ifdef _WIN32
  #include <windows.h>
  #include <process.h>
  #include <io.h>
  #include <direct.h>
  #include <sys/types.h>

  #define popen _popen
  #define pclose _pclose
  #define realpath(N,R) _fullpath((R),(N),MAX_PATH)
  #define WIFEXITED(x) 1
  #define WEXITSTATUS(x) (x)
  #define access _access
  #define F_OK 0

  /* readlink doesn't exist on Windows - provide a stub */
  static inline ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    (void)path;
    /* On Windows, get the executable path using GetModuleFileName */
    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)bufsiz);
    if (len == 0 || len >= bufsiz) return -1;
    return (ssize_t)len;
  }
#else
  #include <unistd.h>
  #include <sys/wait.h>
  #include <limits.h>
#endif

#endif
