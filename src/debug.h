#ifndef DEBUG_H
#define DEBUG_H

#ifdef PSSHDEBUG
#define pssh_printf     printf
#else
#define pssh_printf
#endif

#endif /*DEBUG_H*/
