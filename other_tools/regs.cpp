#include <iostream>
#include <cassert>

#include "pin.H"

static INT32 Usage()
{
	cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
	return -1;
}

int main(int argc, char *argv[])
{
	PIN_InitSymbols();

	// Initialize pin
	if (PIN_Init(argc, argv))
		return Usage();

	cout << "REG_EDI = " << REG_EDI << endl;
	cout << "REG_ESI = " << REG_ESI << endl;
	cout << "REG_EBP = " << REG_EBP << endl;
	cout << "REG_ESP = " << REG_ESP << endl;
	cout << "REG_EBX = " << REG_EBX << endl;
	cout << "REG_EDX = " << REG_EDX << endl;
	cout << "REG_ECX = " << REG_ECX << endl;
	cout << "REG_EAX = " << REG_EAX << endl;

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}
