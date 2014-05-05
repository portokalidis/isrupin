//! Timeout option
#define TIMEOUT_OPTION "timeout"

//! Execution timeout for watchdog
KNOB<unsigned long> exec_timeout(KNOB_MODE_WRITEONCE, "pintool", 
		TIMEOUT_OPTION, "0", "THE RING - Enable watchdog and "
		"set maximum execution time in seconds");
