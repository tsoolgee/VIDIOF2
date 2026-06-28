#include <windows.h>
#include <strsafe.h>

#define PADDING_SIZE 64
#define BUFFER_SIZE  (1024 * 256)

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

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

    // Find mpv.exe next to play.exe
    WCHAR exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    WCHAR* slash = wcsrchr(exeDir, L'\\');
    if (slash) *(slash+1) = L'\0';

    WCHAR mpvPath[MAX_PATH];
    StringCchPrintfW(mpvPath, MAX_PATH, L"%smpv.exe", exeDir);

    if (GetFileAttributesW(mpvPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"mpv.exe not found!", L"Error", MB_ICONERROR);
        return 1;
    }

    // Open the padded file
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Cannot open file", L"Error", MB_ICONERROR);
        return 1;
    }

    // Skip padding
    LARGE_INTEGER li; li.QuadPart = PADDING_SIZE;
    SetFilePointerEx(hFile, li, NULL, FILE_BEGIN);

    // Create stdin pipe for mpv
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        CloseHandle(hFile);
        return 1;
    }

    // Make write end non-inheritable
    SetHandleInformation(hWritePipe, HANDLE_FLAG_INHERIT, 0);

    // Launch mpv reading from stdin
    WCHAR cmdLine[MAX_PATH * 2];
    StringCchPrintfW(cmdLine, MAX_PATH*2,
        L"\"%s\" --no-terminal --force-window --keep-open=yes -", mpvPath);

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

    // Close read end on our side
    CloseHandle(hReadPipe);

    // Stream file data to mpv stdin
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
