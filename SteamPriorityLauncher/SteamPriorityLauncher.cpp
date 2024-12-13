// SteamPriorityLauncher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <cstdio>

#define _WIN32_WINNT 0x501 // Windows XP
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <atlstr.h>
#include <shellapi.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <inttypes.h>

#define SPL_VERSION "1.4"

constexpr int32_t PRI_COUNT = 6;
constexpr int32_t PRI_DEFAULT = 3; // A (Above Normal)
const char* PRI_NAMES[] = { "L", "B", "N", "A", "H", "R" };
const char* PRI_DETAILS[] = { "Low/Idle*", "Below Normal*", "Normal", "Above Normal", "High*", "Realtime* (requires admin privileges, may cause system instability!)" };
const DWORD PRI_VALUES[] = { IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS, REALTIME_PRIORITY_CLASS };

HWND hwndCon;

enum ParseState {
	Default, NextIsPriValue, NextIsGameID, NextIsGameExe, NextIsAffinity
};

void pauseBeforeExit() {
	printf("press any key to exit . . . ");
	(void)getchar();
}

void notifyError() {
	MessageBeep(MB_ICONERROR);
	FLASHWINFO fwi;
	ZeroMemory(&fwi, sizeof(fwi));
	fwi.cbSize = sizeof(fwi);
	fwi.hwnd = hwndCon;
	fwi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
	FlashWindowEx(&fwi);
}

void printError(const char* errSrc, DWORD err) {
	notifyError();
	char* errMsg = NULL;
	DWORD fmtMsgErr = ERROR_SUCCESS;
	if (!FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)& errMsg,
		0,
		NULL))
		fmtMsgErr = GetLastError();
	if (fmtMsgErr == ERROR_SUCCESS)
		printf("ERROR: %s, error code 0x%X: %s", errSrc, err, errMsg);
	else {
		printf("ERROR: %s, error code 0x%X\n", errSrc, err);
		printf("additionally, FormatMessageA failed, error code 0x%X\n", fmtMsgErr);
	}
	LocalFree(errMsg);
	pauseBeforeExit();
}

int main(int argc, char* argv[])
{
	printf("SteamPriorityLauncher v" SPL_VERSION " by ADudeCalledLeo (aka Leo40Git)\n");
	printf("built on " __DATE__ " at " __TIME__ "\n");
	printf("https://github.com/Leo40Git/SteamPriorityWrapper\n");
	printf("\n");

	hwndCon = GetConsoleWindow();

	DWORD priValue = PRI_VALUES[PRI_DEFAULT];
	DWORD_PTR affMask = 0, affMaskSystem = 0;
	char gameID[0x101] = "";
	char gameExe[MAX_PATH] = "";
	char affMaskBuf[0x101] = "";
	bool gameIDSet = false, gameExeSet = false, affMaskSet = false;

	// Get system affinity mask
	{
		DWORD_PTR useless = 0; // we don't need our affinity mask
		if (!GetProcessAffinityMask(GetCurrentProcess(), &useless, &affMaskSystem)) {
			DWORD err = GetLastError();
			printError("GetProcessAffinityMask failed", err);
			return EXIT_FAILURE;
		}
	}
	if (argc == 1) {
		printf("usage: %s -priority <priority> -gameID <steam ID of the game to launch> -gameExe <name of the game's EXE> -affinity <list of cores>\n", argv[0]);
		printf("Only the -gameID and -gameExe options are required.\n");
		printf("\n");
		printf("Valid values for <priority> are:\n");
		for (int i = 0; i < PRI_COUNT; i++)
			printf("  %s - %s\n", PRI_NAMES[i], PRI_DETAILS[i]);
		printf("  (* - not recommended)\n");
		printf("If the priority parameter is not specified, it'll default to %s.\n", PRI_NAMES[PRI_DEFAULT]);
		printf("\n");
		printf("<list of cores> - First core is 0, not 1. Separated by semicolons. Only recognizes decimal format.\n");
		printf("If you have more than 64 cores, each entry defines a group of cores, rather than one.\n");
		printf("(but let's be real, if you have a machine with >64 cores, it's probably not for gaming)\n");
		printf("If you don't know what affinity is, you probably don't need to set it.\n");
		return EXIT_SUCCESS;
	}

	ParseState ps = Default;
	char *ptr = NULL, *ptr_next = NULL;
	for (int i = 1; i < argc; i++) {
		switch (ps) {
		case NextIsPriValue:
			for (int j = 0; j < PRI_COUNT; j++) {
				if (strcmp(argv[i], PRI_NAMES[j]) == 0) {
					priValue = PRI_VALUES[j];
					ps = Default;
					break;
				}
			}
			if (ps == Default)
				break;
			printf("ERROR: unknown priority \"%s\"\n", argv[i]);
			pauseBeforeExit();
			return EXIT_FAILURE;
		case NextIsGameID:
			gameIDSet = true;
			ZeroMemory(gameID, sizeof(gameID));
			strcpy_s(gameID, argv[i]);
			ps = Default;
			break;
		case NextIsGameExe:
			gameExeSet = true;
			ZeroMemory(gameExe, sizeof(gameExe));
			strcpy_s(gameExe, argv[i]);
#ifdef DEBUG
			printf("setting gameExe to \"%s\"\n", gameExe);
#endif
			// append ".exe" to the game EXE name if it isn't already there
			ptr = strrchr(gameExe, '.');
			if (!ptr || strcmp(ptr, ".exe") != 0)
				strcat_s(gameExe, ".exe");
#ifdef DEBUG
			printf("gameExe = \"%s\"\n", gameExe);
#endif
			ps = Default;
			break;
		case NextIsAffinity:
			affMaskSet = true;
			ZeroMemory(affMaskBuf, sizeof(affMaskBuf));
			strcpy_s(affMaskBuf, argv[i]);
			ptr = strtok_s(affMaskBuf, ";", &ptr_next);
			while (ptr) {
#ifdef DEBUG
				printf("got core list entry \"%s\"\n", affMaskBuf);
#endif
				// Get system's maximum number of valid affinity bits (based on the architecture)
				size_t maxBits = sizeof(DWORD_PTR) * 8; // This will be 32 on 32-bit and 64 on 64-bit systems

				DWORD_PTR affBit = strtoul(ptr, NULL, 10);
				affMask |= static_cast<DWORD_PTR>(1) << affBit;
#ifdef DEBUG
				printf("affMask = 0x%" PRIXPTR "\n", affMask);
				
#endif
				ptr = strtok_s(NULL, ";", &ptr_next);
			}
			// AND affinity mask and system affinity mask, since the former has to be a subset of the latter
			affMask &= affMaskSystem;
			ps = Default;
			break;
		case Default:
		default:
			if (strcmp(argv[i], "-priority") == 0) {
				// next "parameter" is actually our priority value
				ps = NextIsPriValue;
				continue;
			}
			if (strcmp(argv[i], "-gameID") == 0) {
				// next "parameter" is actually our game ID
				ps = NextIsGameID;
				continue;
			}
			if (strcmp(argv[i], "-gameExe") == 0) {
				// next "parameter" is actually our game EXE name
				ps = NextIsGameExe;
				continue;
			}
			if (strcmp(argv[i], "-affinity") == 0) {
				// next "parameter" is actually our core affinity list
				ps = NextIsAffinity;
				continue;
			}
			printf("WARNING: unknown option \"%s\"\n", argv[i]);
			break;
		}
	}

	// quit if we don't have a required parameter
	if (!gameIDSet) {
		notifyError();
		printf("ERROR: game ID not set!\n");
		printf("please specify the -gameID option.\n");
		pauseBeforeExit();
		return EXIT_FAILURE;
	}
	if (!gameExeSet) {
		notifyError();
		printf("ERROR: game EXE name not set!\n");
		printf("please specify the -gameExe option.\n");
		pauseBeforeExit();
		return EXIT_FAILURE;
	}
	// also quit if we somehow got an affinity mask that specifies 0 cores
	if (affMaskSet && affMask == 0) {
		notifyError();
		printf("ERROR: -affinity specified with no cores!\n");
		pauseBeforeExit();
		return EXIT_FAILURE;
	}

	// create steam browser protocol command
	char steamCmd[0x100 + 13] = "steam://run/";
	strcat_s(steamCmd, gameID);

	// run the command
	DWORD errcode = 0;
	if ((errcode = reinterpret_cast<DWORD>(ShellExecuteA(NULL, "open", steamCmd, NULL, NULL, SW_SHOW))) <= 32) {
		printError("ShellExecuteA failed", errcode);
		return EXIT_FAILURE;
	}

	printf("SteamPriorityLauncher will now wait for a process of \"%s\" to launch.\n", gameExe);
	printf("If, for some reason, the game fails to launch, you can terminate the launcher by pressing Ctrl + C.\n");

	// TIME TO SEARCH FOR THE GAME EXE, YAY
	HANDLE hGameProc = NULL;
	bool match = false;

	// convert EXE name to TCHAR*
	USES_CONVERSION;
#pragma warning( disable : 6255 )
	TCHAR* gameExeT = A2T(gameExe);
#pragma warning( default : 6255 )

	while (!match) {
#ifdef DEBUG
		printf("== SEARCH LOOP START! ==");
#endif

		// setup to loop through process snapshot
		HINSTANCE hInst = GetModuleHandle(NULL);
		HANDLE hSnap = NULL;
		PROCESSENTRY32 pe32;
		ZeroMemory(&pe32, sizeof(pe32));
		pe32.dwSize = sizeof(pe32);

		// take that snapshot
		hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (!hSnap) {
			DWORD err = GetLastError();
			printError("CreateToolhelp32Snapshot failed; could not get a snapshot of all processes", err);
			return EXIT_FAILURE;
		}

		// loop through and grab the matching EXE
		if (Process32First(hSnap, &pe32)) {
			do {
				hGameProc = OpenProcess(PROCESS_QUERY_INFORMATION, true, pe32.th32ProcessID);
				// if we can't open the process, continue
				if (!hGameProc)
					continue;
				CloseHandle(hGameProc);
#ifdef DEBUG
				_tprintf(_T("checking \"%s\": "), pe32.szExeFile);
#endif
				// if the EXE names don't match, continue
				if (StrCmp(pe32.szExeFile, gameExeT) != 0) {
#ifdef DEBUG
					printf("no match\n");
#endif
					continue;
				}
				// we got a match! open the process and break this loop
#ifdef DEBUG
				printf("it's a match!\n");
#endif
				hGameProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION, true, pe32.th32ProcessID);
				match = true;
				break;
			} while (Process32Next(hSnap, &pe32));
		}
		CloseHandle(hSnap);
	}

	// and finally, set the new priority!
	if (!SetPriorityClass(hGameProc, priValue)) {
		DWORD err = GetLastError();
		printError("SetPriorityClass failed", err);
		CloseHandle(hGameProc);
		return EXIT_FAILURE;
	}

	// oh, and set affinity too I guess
	if (affMaskSet) {
		if (!SetProcessAffinityMask(hGameProc, affMask)) {
			DWORD err = GetLastError();
			printError("SetProcessAffinityMask failed", err);
			CloseHandle(hGameProc);
			return EXIT_FAILURE;
		}
	}

	CloseHandle(hGameProc);

	printf("successfully set process attributes!\n");

#ifdef DEBUG
	pauseBeforeExit();
#endif

	return EXIT_SUCCESS;
}
