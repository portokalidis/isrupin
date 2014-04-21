#ifndef RESCUEPOINT_H
#define RESCUEPOINT_H

#define CONF_OPTIONS 5


typedef enum { RPBLOCKOTHERS, RPIGNOREOTHERS, } rescuepoint_block_t;
typedef enum { RPBYFUNCNAME, RPBYFUNCADDR, RPBYRELADDR, } rescuepoint_id_t;


class RescuePoint {
public:
	static RescuePoint *createRescuePoint(const string w[CONF_OPTIONS]);

	string & name(void) { return rp_name; }

	ADDRINT address(void) { return faddr; }

	void setAddress(ADDRINT addr) { faddr = addr; }

	ADDRINT endAddress(void) { return faddr_end; }

	void setEndAddress(ADDRINT addr) { faddr_end = addr; }

	bool hasReturnValue(void) { return !noret; }

	rescuepoint_block_t blockType(void) { return block_type; }

	rescuepoint_id_t idType(void) { return id_type; }

	ADDRINT returnValue() { return retval; }

	void setRetAddress(ADDRINT addr) { ret_addr = addr; }

	ADDRINT retAddress(void) { return ret_addr; }

	void setBaseAddress(ADDRINT base_addr);

	friend ostream & operator<<(ostream &out, RescuePoint *rp);

private:
	rescuepoint_block_t block_type;
	rescuepoint_id_t id_type;
	string rp_name;
	ADDRINT faddr, ret_addr, retval, faddr_end;
	bool noret;
};

#endif
