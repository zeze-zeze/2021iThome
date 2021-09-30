#include<Windows.h>

int main(int argc, char* argv[]) {
	if (IsDebuggerPresent()) {
		MessageBoxW(0, L"Detect", L"Debugger", 0);
	}
	else {
		MessageBoxW(0, L"Not Detect", L"Debugger", 0);
	}
}