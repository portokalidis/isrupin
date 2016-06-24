#ifndef ISR_OPTS_HPP
#define ISR_OPTS_HPP

// Image keys DB
static KNOB<string> isr_keydb(KNOB_MODE_WRITEONCE, "pintool", "isr_keydb", 
		"db/image_keys.db", "Key database to use");

static KNOB<bool> isr_cidebug(KNOB_MODE_WRITEONCE, "pintool",
    "isr_cidebug", "0", "Debug code injection attacks by calling pause() "
    "after printing the CI warning message.");

#endif
