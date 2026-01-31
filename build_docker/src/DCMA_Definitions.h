// DCMA_Definitions.h.
//
// This file contains configuration settings detected by the build system.

#pragma once

// sys/select.h
#define DCMA_HAS_SYS_SELECT 1

// unistd.h
#define DCMA_HAS_UNISTD_ISATTY 1

#if __has_include(<unistd.h>) \
 && !defined(DCMA_HAS_UNISTD) \
 && defined(DCMA_HAS_UNISTD_ISATTY)      && DCMA_HAS_UNISTD_ISATTY 
    #define DCMA_HAS_UNISTD 1
#endif

// cstdio.h
#define DCMA_HAS_CSTDIO_FILENO 1

// termios.h
/* #undef DCMA_HAS_TERMIOS_TERMIOS */
#define DCMA_HAS_TERMIOS_ICANON 1
#define DCMA_HAS_TERMIOS_ECHO 1
#define DCMA_HAS_TERMIOS_VMIN 1
#define DCMA_HAS_TERMIOS_VTIME 1
#define DCMA_HAS_TERMIOS_TCSANOW 1
#define DCMA_HAS_TERMIOS_TCGETADDR 1

#if __has_include(<termios.h>) \
 && !defined(DCMA_HAS_TERMIOS) \
 && defined(DCMA_HAS_TERMIOS_ICANON)    && DCMA_HAS_TERMIOS_ICANON \
 && defined(DCMA_HAS_TERMIOS_ECHO)      && DCMA_HAS_TERMIOS_ECHO \
 && defined(DCMA_HAS_TERMIOS_VMIN)      && DCMA_HAS_TERMIOS_VMIN \
 && defined(DCMA_HAS_TERMIOS_VTIME)     && DCMA_HAS_TERMIOS_VTIME \
 && defined(DCMA_HAS_TERMIOS_TCSANOW)   && DCMA_HAS_TERMIOS_TCSANOW \
 && defined(DCMA_HAS_TERMIOS_TCGETADDR) && DCMA_HAS_TERMIOS_TCGETADDR 
    #define DCMA_HAS_TERMIOS 1
#endif

// fcntl.h
#define DCMA_HAS_FCNTL_FCNTL 1
#define DCMA_HAS_FCNTL_F_GETFL 1
#define DCMA_HAS_FCNTL_O_NONBLOCK 1

#if __has_include(<fcntl.h>) \
 && !defined(DCMA_HAS_FCNTL) \
 && defined(DCMA_HAS_FCNTL_FCNTL)      && DCMA_HAS_FCNTL_FCNTL \
 && defined(DCMA_HAS_FCNTL_F_GETFL)    && DCMA_HAS_FCNTL_F_GETFL \
 && defined(DCMA_HAS_FCNTL_O_NONBLOCK) && DCMA_HAS_FCNTL_O_NONBLOCK
    #define DCMA_HAS_FCNTL 1
#endif
