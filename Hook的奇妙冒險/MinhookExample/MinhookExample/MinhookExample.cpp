#include <Windows.h>
#include "MinHook.h"

#if defined _M_X64
#pragma comment(lib, "libMinHook.x64.lib")
#elif defined _M_IX86
#pragma comment(lib, "libMinHook.x86.lib")
#endif

// 1. 定義要 Hook 的函數，因為等等要把它改成自己的函數，所以最好參數可以對應原本的函數。
typedef int (WINAPI* MESSAGEBOXW)(HWND, LPCWSTR, LPCWSTR, UINT);
MESSAGEBOXW fpMessageBoxW = NULL;

int WINAPI DetourMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
	return fpMessageBoxW(hWnd, L"Hooked\n", lpCaption, uType);
}

int main() {
	// 2. 將目標函數的資訊記下來，做好 Hook 的前置作業。
	if (MH_Initialize() != MH_OK) {
		return 1;
	}
	if (MH_CreateHook(&MessageBoxW, &DetourMessageBoxW, reinterpret_cast<LPVOID*>(&fpMessageBoxW)) != MH_OK) {
		return 1;
	}

	// 3. 啟用 Hook，將目標函數前幾 Byte 改掉，jmp 到自己定義的函數。
	if (MH_EnableHook(&MessageBoxW) != MH_OK)
	{
		return 1;
	}

	// 測試目標函數被改掉後有沒有變成自己定義的函數，這邊應該要跳出 Hooked
	MessageBoxW(NULL, L"Not hooked\n", L"MinHook Example", MB_OK);

	// 4. 停用 Hook，把目標函數改回來
	if (MH_DisableHook(&MessageBoxW) != MH_OK) {
		return 1;
	}

	// 測試目標函數有沒有被改回來，這邊應該要跳出 Not hooked
	MessageBoxW(NULL, L"Not hooked\n", L"MinHook Example", MB_OK);

	if (MH_Uninitialize() != MH_OK) {
		return 1;
	}
	return 0;
}