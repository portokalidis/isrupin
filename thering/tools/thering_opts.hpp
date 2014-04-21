
#ifdef USE_DTA
/* Enable DTA (enabled by default) */
static KNOB<BOOL> dta(KNOB_MODE_WRITEONCE, "pintool", "use_dta", "0", 
		"THE RING - Enable DTA component");
#endif

#ifdef USE_ISR
/* Enable ISR (enabled by default only on Linux) */
static KNOB<BOOL> isr(KNOB_MODE_WRITEONCE, "pintool", "use_isr", "0", 
		"THE RING - Enable ISR component");
#endif

#ifdef USE_REASSURE
/* Enable REASSURE (enabled by default) */
static KNOB<BOOL> reassure(KNOB_MODE_WRITEONCE, "pintool", "use_reassure", "0",
		"THE RING - Enable REASSURE component");
#endif

#ifdef USE_DYBOC
/* Enable DYBOC-link (disabled by default) */
static KNOB<BOOL> dyboc(KNOB_MODE_WRITEONCE, "pintool", "use_dyboc", "0",
		"THE RING - Enable dyboc link");
#endif

