
//! Original name
static KNOB<string> original_name(KNOB_MODE_WRITEONCE, "pintool", "name", "", 
	"THE RING - Specify executable's original name. For reporting errors.");

//! Reference Id
static KNOB<string> ref_id(KNOB_MODE_WRITEONCE, "pintool", "refid", "", 
	"THE RING - Specify reference-id. For reporting errors.");

//! Notification messages to stderr
static KNOB<bool> notify_stderr(KNOB_MODE_WRITEONCE, "pintool", "notify", "0",
	"THE RING - Notification messages are also written to stderr.");

