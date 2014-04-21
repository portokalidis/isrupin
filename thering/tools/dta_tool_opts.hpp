
/*
 * flag variables
 *
 * 0	: feature disabled
 * >= 1	: feature enabled
 */ 

/* track stdin (enabled by default) */
static KNOB<size_t> sin_source(KNOB_MODE_WRITEONCE, "pintool", "tainted_sin", 
		"1", "DTA - Data coming from stdin are considered tainted.");

/* track fs (enabled by default) */
static KNOB<size_t> fs_source(KNOB_MODE_WRITEONCE, "pintool", "tainted_fs", "1",
                "DTA - Data coming from files are considered tainted.");

/* track net (enabled by default) */
static KNOB<size_t> net_source(KNOB_MODE_WRITEONCE, "pintool", "tainted_net", 
		"1", "DTA - Data coming from network sockets are "
                "considered tainted.");

/* use a list of files to track */
static KNOB<string> fs_list(KNOB_MODE_WRITEONCE, "pintool", "tainted_files", 
		"", "DTA - Define tainted-file input "
		"configuration file. Only data coming from files specified in "
		"the configuration will be tainted.");

/* Alert type. The same as the alert mode passed as a command line. */
typedef enum ALERT_TYPE_ENUM 
	{ ALERT_ALTER_FLOW = 0, ALERT_WRITTEN = 1 } dta_alert_type_t;

/* use a list of files to track */
static KNOB<size_t> alert_mode(KNOB_MODE_WRITEONCE, "pintool", 
		"tainted_alert", 
		"0", "DTA - Set when alerts will be generated\n"
		"\t0 - Tainted value control program flow\n"
		"\t1 - Tainted value is being used with the write(2)\n");
