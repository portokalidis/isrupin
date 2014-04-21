#include <sstream>
#include <cassert>

#ifdef TARGET_LINUX
extern "C" {
#include <stdlib.h>
}
#endif // TARGET_LINUX

#include "pin.H"
#include "RescuePoint.hpp"



/**
 * Create and initialize a rescue point object.
 *
 * @param w Array of strings used to initialize the object. 
 * The string corresponds to the type of the RP, 
 * its rp_name/address/relative address,
 * a specifier of whether the RP returns a value ('V' for yes, 'N' for no), 
 * the integer value that should be returned in the case of an error,
 * and the thread behavior of the RP (BlockOthers or IgnoreOthers).
 * @return Pointer to newly allocated RP object, or NULL on error.
 */
RescuePoint *RescuePoint::createRescuePoint(const string w[CONF_OPTIONS])
{
	RescuePoint *rp = new RescuePoint();

	// How is the rescue point identified
	if (w[0].empty())
		goto fail;
	else if (w[0].compare("SYM") == 0)
		rp->id_type = RPBYFUNCNAME;
	else if (w[0].compare("ADDR") == 0)
		rp->id_type = RPBYFUNCADDR;
	else if (w[0].compare("RELA") == 0)
		rp->id_type = RPBYRELADDR;

	// Get rescue point id
	if (w[1].empty())
		goto fail;
	else if (rp->id_type == RPBYFUNCNAME)
		rp->rp_name = w[1];
	else {
		size_t sep_idx;
		string str, addrstr;

		if (rp->id_type == RPBYRELADDR) {
			// img+start_addr:end_addr in hex

			// Find image - address range sep
			sep_idx = w[1].find_first_of('+');
			if (sep_idx == string::npos) // '+' not found
				return NULL;
			// Read image
			rp->rp_name = w[1].substr(0, sep_idx);
			// Address range string
			addrstr = w[1].substr(sep_idx + 1);
		} else if (rp->id_type == RPBYFUNCADDR) {
			// start_addr:end_addr in hex
			addrstr = w[1];
		} else
			abort();

		// find separator ':'
		sep_idx = addrstr.find_first_of(':');
		if (sep_idx == string::npos) // ':' not found
			return NULL;

		// Read the two hex numbers (start and end addresses of the RP)
		str = addrstr.substr(0, sep_idx);
		rp->faddr = strtoul(str.c_str(), NULL, 16);
		str = addrstr.substr(sep_idx + 1);
		rp->faddr_end = strtoul(str.c_str(), NULL, 16);

	} 

	if (w[2] == "N")
		rp->noret = true;
	else if (w[2] == "V")
		rp->noret = false;
	else
		goto fail;

	if (w[3].empty())
		goto fail;
	else 
		rp->retval = atoi(w[3].c_str());

	
	if (!w[4].empty()) {
		if (w[4] == "BlockOthers")
			rp->block_type = RPBLOCKOTHERS;
		else if (w[4] == "IgnoreOthers")
			rp->block_type = RPIGNOREOTHERS;
		else
			goto fail;
	} else {
fail:
		delete rp;
		rp = NULL;
	}

	rp->ret_addr = 0;

	return rp;
}

/**
 * Output operator for rescue point objects. Print a string representation of
 * the RP based on its type.
 *
 * @param out Output stream reference.
 * @param rp Pointer to RP object.
 * @return The output stream.
 */
ostream & operator<<(ostream &out, RescuePoint *rp)
{
	if (rp->id_type != RPBYFUNCADDR)
		out << rp->rp_name;
	if (rp->id_type == RPBYFUNCNAME)
		return out;
	if (rp->id_type == RPBYRELADDR)
		out << '+';
	out << "0x" << hex << rp->faddr << ":0x" << rp->faddr_end;
	return out;
}

/**
 * Set the base address of the image containing a relatively addressed RP (type
 * RPBYRELADDR). This method adds the base address to the start and end address
 * of the RP, transforming it to type RPBYFUNCNAME.
 *
 * @param base_addr Base address of image containing the RP.
 */
void RescuePoint::setBaseAddress(ADDRINT base_addr)
{
	ASSERT(id_type == RPBYRELADDR, 
		"RescuePoint::setBaseAddress() called with invalid RP type\n");

	faddr += base_addr;
	faddr_end += base_addr;
	id_type = RPBYFUNCADDR;
}
