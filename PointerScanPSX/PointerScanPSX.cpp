#define _CRT_SECURE_NO_WARNINGS 1
#include <Windows.h>
#include <tlhelp32.h>
#include <stdio.h>

DWORD baseAddress;
HANDLE handle;

// needed for all hacks
DWORD GetProcId(const wchar_t* processName, DWORD desiredIndex)
{
	// create snapshot of PROCESS
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	// check for valid
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		// process entry
		PROCESSENTRY32 procEntry;
		procEntry.dwSize = sizeof(procEntry);

		// start looping through processes
		if (Process32First(hSnap, &procEntry))
		{
			int processIndex = 0;

			// check if this is the right process
			do
			{
				// if the name of this process is the name we are searching
				if (!_wcsicmp(procEntry.szExeFile, processName))
				{
					if (processIndex == desiredIndex)
					{
						// return this process ID
						return procEntry.th32ProcessID;
						break;
					}

					processIndex++;
				}

				// check the next one
			} while (Process32Next(hSnap, &procEntry));
		}
	}

	// close the snap and return 0
	CloseHandle(hSnap);
	return 0;
}

// Needed for all ePSXe hacks, used for DLLs in other hacks
uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName)
{
	// create snapshot of MODULE and MODULE32
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);

	// check for valid
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		// module entry
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);

		// start looping through modules
		if (Module32First(hSnap, &modEntry))
		{
			// check if this is the right module
			do
			{
				// if the name of this module is the name we are searching
				if (!_wcsicmp(modEntry.szModule, modName))
				{
					// return this module base address
					return (uintptr_t)modEntry.modBaseAddr;
					break;
				}

				// check the next one
			} while (Module32Next(hSnap, &modEntry));
		}
	}

	// close the snap and return 0
	CloseHandle(hSnap);
	return 0;
}

void initialize()
{
	HWND console = GetConsoleWindow();
	RECT r;
	GetWindowRect(console, &r); //stores the console's current dimensions

	// 300 + height of bar (25)
	MoveWindow(console, r.left, r.top, 400, 325, TRUE);

	printf("Step 1: Open ePSXe.exe\n");
	printf("Step 2: Open Crash Team Racing SCUS_94426\n");
	printf("\n");
	printf("Step 3: Enter ProcessID\n");
	printf("For auto-detection, enter 0\n\n");
	printf("Enter: ");

	DWORD procID = 0;
	scanf("%d", &procID);

	// auto detect the procID if the user enters number less than 5
	// This will act as instance index (first instance, second, etc)
	if (procID < 10) procID = GetProcId(L"ePSXe.exe", procID);

	// get the base address, relative to the module
	baseAddress = GetModuleBaseAddress(procID, L"ePSXe.exe");

	// if the procID is not found
	if (!procID)
	{
		printf("Failed to find ePSXe.exe\n");
		printf("open ePSXe.exe\n");
		printf("and try again\n");

		system("pause");
		exit(0);
	}

	// open the process with procID, and store it in the 'handle'
	handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procID);

	// if handle fails to open
	if (!handle)
	{
		printf("Failed to open process\n");
		system("pause");
		exit(0);
	}
}

// This test passes on 1P Arcade Crash Cove
void Test()
{
	// We are using a pointer here
	int expectedNavAddr;

	printf("PSX %p\n", 0xB0F6A8-0xA82020);
	printf("ePSXe %p\n", 0xB0F6A8);

	// Get value of pointer, which is address of nav data
	ReadProcessMemory(handle, (PBYTE*)(baseAddress + 0xB0F6A8), &expectedNavAddr, sizeof(int), NULL);

	// This should only be 3 bytes large
	expectedNavAddr = expectedNavAddr & 0xFFFFFF;
	printf("PSX %p\n", expectedNavAddr);

	// convert PSX address to ePSXe address
	expectedNavAddr += baseAddress + 0xA82020;
	printf("ePSXe %p\n", expectedNavAddr);

	// Scan memory to see what values there are
	unsigned short dataShort[3];
	ReadProcessMemory(handle, (PBYTE*)(expectedNavAddr + 0x0), &dataShort[0], sizeof(short), 0);
	ReadProcessMemory(handle, (PBYTE*)(expectedNavAddr + 0x2), &dataShort[1], sizeof(short), 0);
	ReadProcessMemory(handle, (PBYTE*)(expectedNavAddr + 0x4), &dataShort[2], sizeof(short), 0);

	unsigned short test[3] = { 62531, 677, 60843 };
	printf("%hu %hu %hu\n", dataShort[0], dataShort[1], dataShort[2]);

	if (dataShort[0] == test[0])
		if (dataShort[1] == test[1])
			if (dataShort[2] == test[2])
			{
				// If there is a match, then we found
				// the navigation address
				printf("Works!\n");
			}
}

#include <vector>
std::vector<int> pointers;

int main()
{
	initialize();

	// On Crash Cove 1P Arcade
	// I want to enter 001DD0D0 and get 0008D688

	// 001DD0D0 is the address of NavAddr1x
	// 0008D688 holds the value 001DD0D0
	// At Address 001DD0D0 is value 62531

	printf("Enter Address to search: ");

	int startAddress;
	scanf("%p", &startAddress);

	//Test();
	//system("pause");


	for (int i = 0; i < 2000000; i += 4)
	{
		int possiblePointer;
		ReadProcessMemory(handle, (PBYTE*)(baseAddress + 0xA82020 + i), &possiblePointer, sizeof(int), NULL);
		possiblePointer = possiblePointer & 0xFFFFFF;
		//printf("%p\n", possiblePointer);

		if (possiblePointer == startAddress)
		{
			printf("Found pointer at PSX Address: %p\n", i);
			
			// add pointer to list
			pointers.push_back(i);
		}
	}
	
	while (true)
	{
		printf("Enter value to search: ");
		
		unsigned short searchVal;
		scanf("%hu", &searchVal);

		for (int i = 0; i < pointers.size(); i++)
		{
			// get address from pointer
			unsigned int address;
			ReadProcessMemory(handle, (PBYTE*)(baseAddress + 0xA82020 + pointers[i]), &address, sizeof(address), NULL);
			address = address & 0xFFFFFF;

			// get value from address
			unsigned short scanVal;
			ReadProcessMemory(handle, (PBYTE*)(baseAddress + 0xA82020 + address), &scanVal, sizeof(scanVal), NULL);

			printf("%hu\n", scanVal);

			if (scanVal != searchVal)
			{
				printf("Pointer %p deleted\n", pointers[i]);
				pointers.erase(pointers.begin() + i);
				i--;
			}
		}

		for (int i = 0; i < pointers.size(); i++)
			printf("Pointer %p remains\n", pointers[i]);
	}
}