#include <windows.h>
#include <strsafe.h>
#include <shlwapi.h>
#include <shlobj.h>

#define XOR_KEY      0x42
#define XOR_SIZE     (64 * 1024)   // 64KB
#define BUFFER_SIZE  (1024 * 256)  // 256KB stream buffer

void RegisterAssociation(LPCWSTR exePath) {
    HKEY hKey;
    WCHAR cmd[MAX_PATH * 2];
    StringCchPrintfW(cmd, MAX_PATH*2, L"\"%s\" \"%%1\"", exePath);

    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.VIDIOF", 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)L"VideoPlayer.VIDIOF",
        (DWORD)(wcslen(L"VideoPlayer.VIDIOF")+1)*2);
    RegCloseKey(hKey);

    RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Classes\\VideoPlayer.VIDIOF\\shell\\open\\command",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)cmd, (DWORD)(wcslen(cmd)+1)*2);
    RegCloseKey(hKey);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    WCHAR exeDir[MAX_PATH];
    StringCchCopyW(exeDir, MAX_PATH, exePath);
    WCHAR* sl = wcsrchr(exeDir, L'\\');
    if (sl) *(sl+1) = L'\0';

    RegisterAssociation(exePath);

    WCHAR filePath[MAX_PATH] = {0};
    if (argc < 2) {
        OPENFILENAMEW ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = L"VIDIOF Files\0*.VIDIOF\0All Files\0*.*\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        ofn.lpstrTitle = L"Select VIDIOF file";
        if (!GetOpenFileNameW(&ofn)) { LocalFree(argv); return 0; }
    } else {
        StringCchCopyW(filePath, MAX_PATH, argv[1]);
    }
    LocalFree(argv);

    WCHAR mpvPath[MAX_PATH];
    StringCchPrintfW(mpvPath, MAX_PATH, L"%smpv.exe", exeDir);
    if (GetFileAttributesW(mpvPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"mpv.exe not found!", L"Error", MB_ICONERROR);
        return 1;
    }

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Cannot open file", L"Error", MB_ICONERROR);
        return 1;
    }

    // Create stdin pipe for mpv
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0);

    // Build config dir path (remove trailing backslash)
    WCHAR confDir[MAX_PATH];
    StringCchCopyW(confDir, MAX_PATH, exeDir);
    size_t len = wcslen(confDir);
    if (len > 0 && confDir[len-1] == L'\\') confDir[len-1] = L'\0';

    WCHAR cmdLine[MAX_PATH * 4];
    StringCchPrintfW(cmdLine, MAX_PATH*4,
        L"\"%s\" --no-terminal --force-window --keep-open=yes --config-dir=\"%s\" -",
        mpvPath, confDir);

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        MessageBoxW(NULL, L"Failed to launch mpv", L"Error", MB_ICONERROR);
        CloseHandle(hFile); CloseHandle(hRead); CloseHandle(hWrite);
        return 1;
    }
    CloseHandle(hRead);

    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
    DWORD dwRead, dwWritten;
    BOOL first = TRUE;
    DWORD xorRemain = XOR_SIZE;

    while (ReadFile(hFile, buf, BUFFER_SIZE, &dwRead, NULL) && dwRead > 0) {
        if (xorRemain > 0) {
            DWORD toXor = min(dwRead, xorRemain);
            for (DWORD i = 0; i < toXor; i++)
                buf[i] ^= XOR_KEY;
            xorRemain -= toXor;
        }
        if (!WriteFile(hWrite, buf, dwRead, &dwWritten, NULL)) break;
    }

    HeapFree(GetProcessHeap(), 0, buf);
    CloseHandle(hWrite);
    CloseHandle(hFile);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
