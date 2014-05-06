#include <iostream>
#include <sstream>

#include "pin.H"
#include "libminestrone.hpp"


// Define minestrone reporting command line options
#include "minestrone_opts.hpp"



/**
 * Issue notification in pintool log file, and std err if we have been
 * configured this way.
 *
 * @param str	Reference to string that will be logged
 */
static inline void notify(const string &str)
{
	if (notify_stderr.Value()) {
		cerr << str;
		LOG(str);
	} else {
		LOG(str);
	}
}

/**
 * Return minestrone message for execution status.
 *
 * @param status	Exit status
 * @param code		Exit code
 *
 * @return string containing message
 */
string minestrone_status_message(exis_status_t status, INT32 code)
{
	stringstream ss;

	// Execute status
	switch (status) {
	case ES_SUCCESS:
		ss << "<status>SUCCESS</status>" << endl;
		break;

	case ES_TIMEOUT:
		ss << "<status>TIMEOUT</status>" << endl;
		break;

	case ES_SKIP:
		ss << "<status>SKIP</status>" << endl;
		break;

	case ES_OTHER:
		ss << "<status>OTHER</status>" << endl;
		break;
	}
	ss << "<return_code>" << code << "</return_code>" << endl;

	return ss.str();
}

/**
 * Log execution status in pintool log file, and std err if we have been
 * configured this way.
 *
 * @param status	Exit status
 * @param code		Exit code
 */
void minestrone_log_status(exis_status_t status, INT32 code)
{
	notify(minestrone_status_message(status, code));
}

/**
 * Return minestrone message for a particular vulnerability/event.
 *
 * @param cwe		CWE number of detected vulnerability.
 * 			0 if not CWE was detected
 * @param behavior	String describing the beahvior of the event
 * @param impact	String describing the impact of the event
 *
 * @return string containing message
 */
string minestrone_message(UINT32 cwe, const char *behavior, const char *impact)
{
	stringstream ss;

	// XML version
	//ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;

	ss << "<action>" << endl;
	ss << "<behavior>" << behavior << "</behavior>" << endl;

	// Report CWE, if specified
        if (cwe > 0) {
		ss << "<weakness>" << endl <<
			"<cwe>" << cwe << "</cwe>" << endl <<
			"</weakness>" << endl;
        }

	ss << "<impact>" << endl <<
		"<effect>" << impact << "</effect>" << endl <<
		"</impact>" << endl;

	ss << "</action>" << endl;

	return ss.str();
}

/**
 * Log minestrone message in pintool log file, and std err if we have been
 * configured this way.
 *
 * @param cwe		CWE number of detected vulnerability.
 * 			0 if not CWE was detected
 * @param behavior	String describing the beahvior of the event
 * @param impact	String describing the impact of the event
 */
void minestrone_notify(UINT32 cwe, const char *behavior, const char *impact)
{
	// Finally log everything
	notify(minestrone_message(cwe, behavior, impact));
}

/**
 * Log a successful exit status message on process exit
 *
 * @param code	Exit code
 * @param v	Opaque pointer for use with Pin's API (unused)	
 */
void minestrone_fini_success(INT32 code, VOID *v)
{
	minestrone_log_status(ES_SUCCESS, code);
}
