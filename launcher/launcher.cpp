/*
 * VIDIOF Player Launcher
 * XOR-decodes .VIDIOF files on-the-fly via stdin pipe to mpv.
 * Build: g++ -O2 -o play.exe launcher.cpp -lshell32 -lshlwapi
 *        (or via GitHub Actions / MSVC)
 */

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <cstdio>
#include <thread>
#include <fstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

static const BYTE XOR_KEY = 0x42;

// Register .VIDIOF -> this exe (HKCU, no admin needed)
static void RegisterExtension(const std::wstring& exePath)
{
    HKEY hk;
    // .VIDIOF -> VIDIOFfile
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.VIDIOF", 0,
        nullptr, 0, KEY_SET_VALUE, nullptr, &hk, nullptr);
    const wchar_t* progId = L"VIDIOFfile";
    RegSetValueExW(hk, nullptr, 0, REG_SZ,
        (const BYTE*)progId, (DWORD)((wcslen(progId)+1)*sizeof(wchar_t)));
    RegCloseKey(hk);

    // ProgID shell open command
    std::wstring cmd = L"\"" + exePath + L"\" \"%1\"";
    std::wstring keyPath = L"Software\\Classes\\VIDIOFfile\\shell\\open\\command";
    RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0,
        nullptr, 0, KEY_SET_VALUE, nullptr, &hk, nullptr);
    RegSetValueExW(hk, nullptr, 0, REG_SZ,
        (const BYTE*)cmd.c_str(), (DWORD)((cmd.size()+1)*sizeof(wchar_t)));
    RegCloseKey(hk);

    // Notify shell
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

// Open file dialog for .VIDIOF
static std::wstring PickFile()
{
    wchar_t buf[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFilter  = L"VIDIOF Files\0*.VIDIOF\0All Files\0*.*\0";
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = L"פתח קובץ VIDIOF";
    if (GetOpenFileNameW(&ofn))
        return buf;
    return {};
}

// Find mpv.exe beside this exe
static std::wstring FindMpv()
{
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    PathRemoveFileSpecW(self);
    std::wstring dir(self);
    return dir + L"\\mpv.exe";
}

// XOR-decode thread: reads from file, XORs, writes to pipe write-end
struct PipeArgs {
    std::wstring filePath;
    HANDLE       hWrite;
};

static DWORD WINAPI XorPipeThread(LPVOID param)
{
    PipeArgs* args = (PipeArgs*)param;
    std::ifstream fin(args->filePath, std::ios::binary);
    if (!fin) { CloseHandle(args->hWrite); delete args; return 1; }

    const size_t CHUNK = 256 * 1024; // 256 KB
    std::vector<BYTE> buf(CHUNK);

    while (fin)
    {
        fin.read((char*)buf.data(), CHUNK);
        std::streamsize got = fin.gcount();
        if (got <= 0) break;

        for (std::streamsize i = 0; i < got; ++i)
            buf[i] ^= XOR_KEY;

        DWORD written = 0;
        if (!WriteFile(args->hWrite, buf.data(), (DWORD)got, &written, nullptr))
            break;
    }

    CloseHandle(args->hWrite);
    delete args;
    return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // Register extension on first run
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
    RegisterExtension(selfPath);

    // Get file from args or dialog
    std::wstring filePath;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc >= 2)
        filePath = argv[1];
    LocalFree(argv);

    if (filePath.empty())
        filePath = PickFile();
    if (filePath.empty())
        return 0;

    // Find mpv
    std::wstring mpvPath = FindMpv();
    if (!PathFileExistsW(mpvPath.c_str()))
    {
        MessageBoxW(nullptr, L"mpv.exe לא נמצא בתיקייה של play.exe",
                    L"שגיאה", MB_ICONERROR);
        return 1;
    }

    // Create anonymous pipe (read → mpv stdin, write → our thread)
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
    {
        MessageBoxW(nullptr, L"שגיאה ביצירת pipe", L"שגיאה", MB_ICONERROR);
        return 1;
    }

    // Make sure write end is NOT inherited by child
    SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0);

    // Build mpv command line: read from stdin pipe
    std::wstring cmdLine = L"\"" + mpvPath + L"\" --keep-open=yes "
        L"--cache=yes --demuxer-max-bytes=50MiB "
        L"--sub-auto=fuzzy "
        L"--save-position-on-quit "
        L"--volume-max=200 "
        L"--script-opts=osc-scalewindowed=1.2 "
        L"--";          // next arg is filename, but we use stdin
    // We actually pipe via stdin; tell mpv to read from stdin
    cmdLine += L" -";   // '-' means stdin

    STARTUPINFOW si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdInput   = hRead;
    si.hStdOutput  = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError   = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(0);

    if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr,
                        TRUE, 0, nullptr, nullptr, &si, &pi))
    {
        DWORD err = GetLastError();
        wchar_t msg[256];
        swprintf(msg, 256, L"לא ניתן להפעיל mpv.exe (שגיאה %lu)", err);
        MessageBoxW(nullptr, msg, L"שגיאה", MB_ICONERROR);
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return 1;
    }

    // Close read end in parent (child owns it now)
    CloseHandle(hRead);
    CloseHandle(pi.hThread);

    // Spawn XOR pipe thread
    PipeArgs* args = new PipeArgs{ filePath, hWrite };
    HANDLE hThread = CreateThread(nullptr, 0, XorPipeThread, args, 0, nullptr);
    if (!hThread) { CloseHandle(hWrite); delete args; }
    else          CloseHandle(hThread);

    // Wait for mpv to exit
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    return 0;
}
