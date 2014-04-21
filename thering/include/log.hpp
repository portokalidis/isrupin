#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <sstream>

static inline void LOGSTR(const string &s)
{
#ifdef NOPIN_RUNTIME
	cerr << s << endl;
#else
	LOG(s);
#endif
}

static inline void ERRLOG(const string &s)
{
	LOGSTR("ERROR: " + s);
}

static inline void ERRLOG(stringstream &ss)
{
	ERRLOG(ss.str());
	ss.str("");
}

static inline void WARNLOG(const string &s)
{
	LOGSTR("WARNING: " + s);
}

static inline void OUTLOG(const string &s)
{
	LOGSTR("Message: " + s);
}

static inline void OUTLOG(stringstream &ss)
{
	OUTLOG(ss.str());
	ss.str("");
}

static inline void TIDWLOG(THREADID tid, const string &s)
{
	LOGSTR("WARNING: PIN [" + decstr(tid) + "] " + s);
}

static inline void TIDELOG(THREADID tid, const string &s)
{
	LOGSTR("ERROR: PIN [" + decstr(tid) + "] " + s);
}

static inline void TIDMLOG(THREADID tid, const string &s)
{
	LOGSTR("MESSAGE: PIN [" + decstr(tid) + "] " + s);
}

#ifdef DEBUG
static inline void DBGLOG(const string &s)
{
	LOGSTR("DEBUG: " + s);
}

static inline void TIDDLOG(THREADID tid, const string &s)
{
	LOGSTR("DEBUG: PIN [" + decstr(tid) + "] " + s);
}
#else
#define DBGLOG(s) do { } while (0)
#define TIDDLOG(t, s) do { } while (0)
#endif


#endif
