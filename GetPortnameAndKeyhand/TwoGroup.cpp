#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <Definitions.h>

using namespace std;

const WORD maxStrSize = 100;
char deviceName[] = "EPOS4";
char protocolStackName[] = "MAXON SERIAL V2";
char interfaceName[] = "USB";
char portSel[maxStrSize];
BOOL endOfSelection = FALSE;
DWORD errorCode = 0;

//HANDLE keyHandle1 = 0;
//HANDLE keyHandle2 = 0;

int main()
{
	if (VCS_GetPortNameSelection(deviceName, protocolStackName, interfaceName, TRUE, portSel, maxStrSize, &endOfSelection, &errorCode))
	{
		while (!endOfSelection)
		{
			VCS_GetPortNameSelection(deviceName, protocolStackName, interfaceName, FALSE, portSel, maxStrSize, &endOfSelection, &errorCode);
			cout << portSel << endl;
		}
	}

	return 0;
}