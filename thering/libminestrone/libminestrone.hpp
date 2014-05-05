#ifndef LIBMINESTRONE_HPP
#define LIBMINESTRONE_HPP

//! CWE-476 NULL Pointer Dereference
#define CWE_NULLPOINTER 476

//! CWE-94 Failure to Control Generation of Code (code injection)
#define CWE_CI 94

//! CWE-691 Insufficient Control Flow Management (alter flow)
#define CWE_CF 691


//! Exit status enum for reporting minestrone style
typedef enum EXIT_STATUS_ENUM { ES_SUCCESS, ES_TIMEOUT, ES_SKIP } exis_status_t;

extern KNOB<unsigned long> exec_timeout;


string minestrone_message(UINT32 cwe, const char *impact);

void minestrone_notify(UINT32 cwe, const char *impact);

void minestrone_fini_success(INT32 code, void *v);

string minestrone_status_message(exis_status_t status, INT32 code);

void minestrone_log_status(exis_status_t status, INT32 code);

void minestrone_watchdog_init(int argc, char **argv, unsigned long timeout);

bool minestrone_watchdog_start(void);

#endif
