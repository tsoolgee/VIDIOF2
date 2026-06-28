#include <windows.h>
#include <strsafe.h>

#define PADDING_SIZE 64
#define PIPE_NAME    L"\\\\.\\pipe\\videopipe"
#define BUFFER_SIZE  (1024 * 256)

static HANDLE g_hPipe = INVALID_HANDLE_VALUE;
static WCHAR  g_filePath[MAX_PATH];

DWORD WINAPI PipeThread(LPVOID) {
    // המתן לחיבור של mpv
    if (!ConnectNamedPipe(g_hPipe, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_PIPE_CONNECTED) return 1;
    }

    // פתח את הקובץ
    HANDLE hFile = CreateFileW(g_filePath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 1;

    // דלג על ה-padding
    LARGE_INTEGER li; li.QuadPart = PADDING_SIZE;
    SetFilePointerEx(hFile, li, NULL, FILE_BEGIN);

    // שלח נתונים ל-mpv
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
    DWORD dwRead, dwWritten;
    while (ReadFile(hFile, buf, BUFFER_SIZE, &dwRead, NULL) && dwRead > 0) {
        if (!WriteFile(g_hPipe, buf, dwRead, &dwWritten, NULL)) break;
    }

    HeapFree(GetProcessHeap(), 0, buf);
    FlushFileBuffers(g_hPipe);
    DisconnectNamedPipe(g_hPipe);
    CloseHandle(hFile);
    return 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    ZeroMemory(g_filePath, sizeof(g_filePath));

    if (argc < 2) {
        OPENFILENAMEW ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = L"VIDEO Files\0*.VIDEO\0All Files\0*.*\0";
        ofn.lpstrFile = g_filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        ofn.lpstrTitle = L"בחר קובץ VIDEO";
        if (!GetOpenFileNameW(&ofn)) { LocalFree(argv); return 0; }
    } else {
        StringCchCopyW(g_filePath, MAX_PATH, argv[1]);
    }
    LocalFree(argv);

    // מצא mpv.exe
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

    // צור את ה-pipe קודם
    g_hPipe = CreateNamedPipeW(PIPE_NAME,
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1, BUFFER_SIZE, BUFFER_SIZE, 0, NULL);

    if (g_hPipe == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"שגיאה ביצירת pipe", L"שגיאה", MB_ICONERROR);
        return 1;
    }

    // הפעל pipe thread שימתין לחיבור
    HANDLE hThread = CreateThread(NULL, 0, PipeThread, NULL, 0, NULL);

    // הפעל mpv עם ה-pipe
    WCHAR cmdLine[MAX_PATH * 2];
    StringCchPrintfW(cmdLine, MAX_PATH*2,
        L"\"%s\" --no-terminal --force-window --demuxer=lavf \"%s\"",
        mpvPath, PIPE_NAME);

    STARTUPINFOW si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        MessageBoxW(NULL, L"שגיאה בהפעלת mpv", L"שגיאה", MB_ICONERROR);
        CloseHandle(g_hPipe);
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    WaitForSingleObject(hThread, 5000);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hThread);
    CloseHandle(g_hPipe);
    return 0;
}
