#include <windows.h>
#include <strsafe.h>
#include <shlwapi.h>

#define PADDING_SIZE 64
#define BUFFER_SIZE  (1024 * 256)

// Register .VIDEO file association so double-click works
void RegisterFileAssociation(LPCWSTR exePath) {
    HKEY hKey;
    WCHAR cmd[MAX_PATH * 2];
    StringCchPrintfW(cmd, MAX_PATH*2, L"\"%s\" \"%%1\"", exePath);

    // HKCU - no admin needed
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.VIDEO", 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)L"VideoPlayer.VIDEO",
        (DWORD)(wcslen(L"VideoPlayer.VIDEO")+1)*2);
    RegCloseKey(hKey);

    RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Classes\\VideoPlayer.VIDEO\\shell\\open\\command",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)cmd,
        (DWORD)(wcslen(cmd)+1)*2);
    RegCloseKey(hKey);

    // Icon
    RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Classes\\VideoPlayer.VIDEO\\DefaultIcon",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    WCHAR iconStr[MAX_PATH * 2];
    StringCchPrintfW(iconStr, MAX_PATH*2, L"%s,0", exePath);
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)iconStr,
        (DWORD)(wcslen(iconStr)+1)*2);
    RegCloseKey(hKey);

    // Notify shell
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // Get exe directory
    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    WCHAR exeDir[MAX_PATH];
    StringCchCopyW(exeDir, MAX_PATH, exePath);
    WCHAR* slash = wcsrchr(exeDir, L'\\');
    if (slash) *(slash+1) = L'\0';

    // Register file association on first run
    RegisterFileAssociation(exePath);

    WCHAR filePath[MAX_PATH] = {0};

    if (argc < 2) {
        OPENFILENAMEW ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = L"VIDEO Files\0*.VIDEO\0All Files\0*.*\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        ofn.lpstrTitle = L"Select VIDEO file";
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

    // Open file
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Cannot open file", L"Error", MB_ICONERROR);
        return 1;
    }

    // Skip padding
    LARGE_INTEGER li; li.QuadPart = PADDING_SIZE;
    SetFilePointerEx(hFile, li, NULL, FILE_BEGIN);

    // Create stdin pipe
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        CloseHandle(hFile);
        return 1;
    }
    SetHandleInformation(hWritePipe, HANDLE_FLAG_INHERIT, 0);

    // Build mpv command - use config from exe directory
    WCHAR confDir[MAX_PATH];
    StringCchCopyW(confDir, MAX_PATH, exeDir);
    // Remove trailing backslash for mpv
    size_t len = wcslen(confDir);
    if (len > 0 && confDir[len-1] == L'\\') confDir[len-1] = L'\0';

    // Get file title for OSD
    WCHAR* fileTitle = wcsrchr(filePath, L'\\');
    if (!fileTitle) fileTitle = filePath; else fileTitle++;

    WCHAR cmdLine[MAX_PATH * 4];
    StringCchPrintfW(cmdLine, MAX_PATH*4,
        L"\"%s\" --no-terminal --force-window --keep-open=yes "
        L"--config-dir=\"%s\" "
        L"--title=\"%%F - VideoPlayer\" "
        L"-",
        mpvPath, confDir);

    STARTUPINFOW si2 = {0};
    si2.cb = sizeof(si2);
    si2.dwFlags = STARTF_USESTDHANDLES;
    si2.hStdInput  = hReadPipe;
    si2.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si2.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si2, &pi)) {
        MessageBoxW(NULL, L"Failed to launch mpv", L"Error", MB_ICONERROR);
        CloseHandle(hFile); CloseHandle(hReadPipe); CloseHandle(hWritePipe);
        return 1;
    }

    CloseHandle(hReadPipe);

    // Stream data
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
    DWORD dwRead, dwWritten;
    while (ReadFile(hFile, buf, BUFFER_SIZE, &dwRead, NULL) && dwRead > 0) {
        if (!WriteFile(hWritePipe, buf, dwRead, &dwWritten, NULL)) break;
    }

    HeapFree(GetProcessHeap(), 0, buf);
    CloseHandle(hWritePipe);
    CloseHandle(hFile);

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
