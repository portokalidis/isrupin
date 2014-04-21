
//! Rescue points checkpoint type
KNOB<BOOL> fork_checkpoint(KNOB_MODE_WRITEONCE, "pintool", "rp_fork", "0",
		"REASSURE - use fork for checkpointing");

//! Rescue points configuration 
KNOB<string> config_file(KNOB_MODE_WRITEONCE, "pintool", "rp_conf", 
		"", "REASSURE - Configuration file.");

//! Set type of blocking rescue points
KNOB<BOOL> runtime_block(KNOB_MODE_WRITEONCE, "pintool", "rp_rb", 
		"1", "REASSURE - Use runtime blocks for blocking rescue points."
		" Faster for non-rescue point code, but slower when blocking "
		"rescue points occur extremely often.");

