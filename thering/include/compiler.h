#ifndef COMPILER_H
#define COMPILER_H

#ifdef TARGET_LINUX
# define LINUX_PACKED __attribute__ ((__packed__))
# define likely(x)       __builtin_expect((x),1)
# define unlikely(x)     __builtin_expect((x),0)
#else
# define LINUX_PACKED
# define likely(x)       (x)
# define unlikely(x)     (x)
#endif

#endif
