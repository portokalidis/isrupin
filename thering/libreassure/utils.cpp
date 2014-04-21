#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>


#ifdef TARGET_LINUX
extern "C" {
#include <errno.h>
#include <string.h>
#include <stdlib.h>
}
#endif

#include "pin.H"
#include "RescuePoint.hpp"
#include "utils.hpp"
#include "log.hpp"



/*
 * Parse configuration file
 */
int ParseConf(const char *str, map<string, RescuePoint *> &rpbyname,
		map<ADDRINT, RescuePoint *> &rpbyaddr,
		multimap<string, RescuePoint *> &rpbyimg, bool *has_blocking)
{
	ifstream infile;
	string line, word[CONF_OPTIONS];
	UINT32 lineno, w;
	RescuePoint *rp;
	stringstream ss;
	bool duplicate;

	infile.open(str);
	if (!infile.is_open()) {
		ss << "Could not open configuration file " << str << ": "
			<< strerror(errno) << endl;
		ERRLOG(ss);
		return -1;
	}

	if (has_blocking)
		*has_blocking = false;
	lineno = 0;
	while (true) {
		line = ReadLine(infile, &lineno);
		if (line.empty())
			break;
		w = Tokenize(line, word, CONF_OPTIONS);
		if (w != CONF_OPTIONS)
			goto conf_error;
		rp = RescuePoint::createRescuePoint(word);
		if (!rp) {
conf_error:
			ss << "Invalid rescue point definition at lineno " <<
				lineno << endl << ">> " << line << endl;
			ERRLOG(ss);
			continue;
		}
		duplicate = false;
		if (rp->idType() == RPBYFUNCNAME) {
			pair<map<string, RescuePoint *>::iterator, bool> r;

			r = rpbyname.insert(pair<string, RescuePoint *>
					(rp->name(), rp));
			duplicate = !r.second;
		} else if (rp->idType() == RPBYRELADDR) {
			rpbyimg.insert(pair<string, RescuePoint *>
					(rp->name(), rp));
		} else {
			pair<map<ADDRINT, RescuePoint *>::iterator, bool> r;

			r = rpbyaddr.insert(pair<ADDRINT, RescuePoint *>
					(rp->endAddress(), rp));
			duplicate = !r.second;
		}

		if (duplicate) {
			ss << "Duplicate rescue point for " << rp << endl;
			ERRLOG(ss);
			delete rp;
			continue;
		}

		if (has_blocking && rp->blockType() == RPBLOCKOTHERS)
			*has_blocking = true;
	}

	infile.close();

	return 0;
}
