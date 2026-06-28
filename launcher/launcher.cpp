#include <windows.h>
#include <strsafe.h>

#define PADDING_SIZE 64
#define PIPE_NAME    L"\\\\.\\pipe\\videopipe"
#define BUFFER_SIZE  (1024 * 256)

struct PipeArgs { WCHAR filePath[MAX_PATH]; };

DWORD WINAPI PipeThread(LPVOID lpParam) {
    PipeArgs* args = (PipeArgs*)lpParam;

    HANDLE hFile = CreateFileW(args->filePath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 1;

    LARGE_INTEGER li; li.QuadPart = PADDING_SIZE;
    SetFilePointerEx(hFile, li, NULL, FILE_BEGIN);

    HANDLE hPipe = CreateNamedPipeW(PIPE_NAME,
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1, BUFFER_SIZE, BUFFER_SIZE, 10000, NULL);

    if (hPipe == INVALID_HANDLE_VALUE) { CloseHandle(hFile); return 1; }

    ConnectNamedPipe(hPipe, NULL);

    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
    DWORD dwRead, dwWritten;
    while (ReadFile(hFile, buf, BUFFER_SIZE, &dwRead, NULL) && dwRead > 0)
        WriteFile(hPipe, buf, dwRead, &dwWritten, NULL);

    HeapFree(GetProcessHeap(), 0, buf);
    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    CloseHandle(hFile);
    return 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    static PipeArgs args;
    ZeroMemory(&args, sizeof(args));

    if (argc < 2) {
        OPENFILENAMEW ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = L"VIDEO Files\0*.VIDEO\0All Files\0*.*\0";
        ofn.lpstrFile = args.filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        ofn.lpstrTitle = L"בחר קובץ VIDEO";
        if (!GetOpenFileNameW(&ofn)) { LocalFree(argv); return 0; }
    } else {
        StringCchCopyW(args.filePath, MAX_PATH, argv[1]);
    }
    LocalFree(argv);

    // מצא mpv.exe ליד ה-launcher
    WCHAR exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    WCHAR* slash = wcsrchr(exeDir, L'\\');
    if (slash) *(slash+1) = L'\0';

    WCHAR mpvPath[MAX_PATH];
    StringCchPrintfW(mpvPath, MAX_PATH, L"%smpv.exe", exeDir);

    if (GetFileAttributesW(mpvPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"לא נמצא mpv.exe באותה תיקייה!", L"שגיאה", MB_ICONERROR);
        return 1;
    }

    // הפעל pipe thread
    HANDLE hThread = CreateThread(NULL, 0, PipeThread, &args, 0, NULL);
    if (!hThread) return 1;

    // הפעל mpv
    WCHAR cmdLine[MAX_PATH * 2];
    StringCchPrintfW(cmdLine, MAX_PATH*2,
        L"\"%s\" --no-terminal --force-window \"%s\"",
        mpvPath, PIPE_NAME);

    STARTUPINFOW si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    WaitForSingleObject(pi.hProcess, INFINITE);
    WaitForSingleObject(hThread, 5000);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hThread);
    return 0;
}
