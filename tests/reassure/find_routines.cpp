#include <iostream>
#include <sstream>

#include "pin.H"

extern "C" {
#include <stdlib.h>
}

static unsigned long long routinesno = 0;


static VOID Usage(void)
{
	cerr << "This is a test tool." << endl;
	cerr << KNOB_BASE::StringKnobSummary() << endl;
}

static VOID InstrumentFunctions(RTN rtn, VOID *v)
{
	const string *rname;
	stringstream ss;
	
	RTN_Open(rtn);
	rname = new string(RTN_Name(rtn));
	ss << "Routine: " << *rname << endl;
	LOG(ss.str());
	delete rname;
	routinesno++;
	RTN_Close(rtn);
}

static VOID InstrumentImages(IMG img, VOID *v)
{
	stringstream ss;
	SEC sec;
	RTN rtn;

	if (IMG_IsMainExecutable(img)) {
		ss << "IMG (main) " << IMG_Name(img) << endl;
	} else {
		ss << "IMG " << IMG_Name(img) << endl;
	}
	LOG(ss.str());

	for (sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
		for (rtn = SEC_RtnHead(sec); 
				RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
			InstrumentFunctions(rtn, v);
		}
	}
}

static VOID Fini(INT32 code, VOID *v)
{
	stringstream ss;
	ss << "Total routines: " << routinesno << endl;
	LOG(ss.str());
}


int main(int argc, char **argv)
{
	// This is needed to access functions by name
	PIN_InitSymbols();

	// Initialize pin
	if (PIN_Init(argc, argv)) {
		Usage();
		exit(1);
	}

	//RTN_AddInstrumentFunction(InstrumentFunctions, 0);
	IMG_AddInstrumentFunction(InstrumentImages, 0);
	PIN_AddFiniFunction(Fini, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}
