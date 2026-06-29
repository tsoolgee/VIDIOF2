#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <strsafe.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")

#define XOR_KEY    0x42
#define XOR_SIZE   (64 * 1024)
#define BUF_SIZE   (256 * 1024)

// Control IDs
#define ID_VIDEO_PANEL   100
#define ID_SEEK_BAR      101
#define ID_VOL_BAR       102
#define ID_BTN_PLAY      103
#define ID_BTN_STOP      104
#define ID_BTN_PREV      105
#define ID_BTN_NEXT      106
#define ID_BTN_NEXT_DIR  107
#define ID_BTN_PREV_DIR  108
#define ID_BTN_FULL      109
#define ID_BTN_MUTE      110
#define ID_BTN_SHUFFLE   111
#define ID_BTN_REPEAT    112
#define ID_PLAYLIST      113
#define ID_BTN_OPEN      114
#define ID_BTN_ADD       115
#define ID_TIME_LABEL    116
#define ID_SPEED_LABEL   117
#define ID_STATUS_BAR    118
#define ID_BTN_SUB_INC   119
#define ID_BTN_SUB_DEC   120
#define ID_BTN_AUDIO_DL  121
#define ID_BTN_AUDIO_IL  122
#define ID_BTN_SNAP      123
#define ID_BTN_ABLOOP    124
#define ID_BTN_ZOOM      125

// Menu IDs
#define IDM_FILE_OPEN     200
#define IDM_FILE_OPEN_URL 201
#define IDM_FILE_ADD      202
#define IDM_FILE_SNAP     203
#define IDM_FILE_PROPS    204
#define IDM_PLAY_PLAY     210
#define IDM_PLAY_STOP     211
#define IDM_PLAY_PREV     212
#define IDM_PLAY_NEXT     213
#define IDM_PLAY_SPEED_25 214
#define IDM_PLAY_SPEED_50 215
#define IDM_PLAY_SPEED_75 216
#define IDM_PLAY_SPEED_100 217
#define IDM_PLAY_SPEED_125 218
#define IDM_PLAY_SPEED_150 219
#define IDM_PLAY_SPEED_200 220
#define IDM_PLAY_SPEED_400 221
#define IDM_PLAY_ABLOOP   222
#define IDM_PLAY_FRAME_F  223
#define IDM_PLAY_FRAME_B  224
#define IDM_PLAY_REPEAT_NONE 225
#define IDM_PLAY_REPEAT_ONE  226
#define IDM_PLAY_REPEAT_ALL  227
#define IDM_PLAY_SHUFFLE     228
#define IDM_VIDEO_ASPECT_FREE 230
#define IDM_VIDEO_ASPECT_43   231
#define IDM_VIDEO_ASPECT_169  232
#define IDM_VIDEO_ASPECT_235  233
#define IDM_VIDEO_ZOOM_50     234
#define IDM_VIDEO_ZOOM_100    235
#define IDM_VIDEO_ZOOM_200    236
#define IDM_VIDEO_ROT_0       237
#define IDM_VIDEO_ROT_90      238
#define IDM_VIDEO_ROT_180     239
#define IDM_VIDEO_ROT_270     240
#define IDM_VIDEO_FLIP_H      241
#define IDM_VIDEO_FLIP_V      242
#define IDM_VIDEO_FULLSCREEN  243
#define IDM_VIDEO_ONTOP       244
#define IDM_AUDIO_TRACK_BASE  250
#define IDM_AUDIO_DELAY_P     260
#define IDM_AUDIO_DELAY_M     261
#define IDM_AUDIO_MUTE        262
#define IDM_AUDIO_VOL_P       263
#define IDM_AUDIO_VOL_M       264
#define IDM_SUB_LOAD          270
#define IDM_SUB_TRACK_BASE    271
#define IDM_SUB_SIZE_P        280
#define IDM_SUB_SIZE_M        281
#define IDM_SUB_DELAY_P       282
#define IDM_SUB_DELAY_M       283
#define IDM_SUB_HIDE          284
#define IDM_INFO_FILE         290
#define IDM_HELP_KEYS         291
#define IDM_DIR_NEXT          300
#define IDM_DIR_PREV          301

// Timer IDs
#define TIMER_SEEK   1
#define TIMER_HIDE   2
#define TIMER_OSD    3

static HINSTANCE g_hInst;
static HWND g_hMain, g_hVideo, g_hPlaylist, g_hStatus;
static HWND g_hSeek, g_hVol;
static HWND g_hBtnPlay, g_hBtnStop, g_hBtnPrev, g_hBtnNext;
static HWND g_hBtnFull, g_hBtnMute, g_hBtnShuffle, g_hBtnRepeat;
static HWND g_hBtnOpen, g_hBtnAdd, g_hBtnNextDir, g_hBtnPrevDir;
static HWND g_hBtnSnap, g_hBtnABLoop;
static HWND g_hTimeLabel, g_hSpeedLabel;
static HWND g_hToolbar;
static HWND g_hOverlay;
static HANDLE g_hPipe = INVALID_HANDLE_VALUE;
static HANDLE g_hMpvProc = NULL;
static HANDLE g_hDecodeThread = NULL;
static HANDLE g_hPipeRead = INVALID_HANDLE_VALUE;
static HANDLE g_hPipeWrite = INVALID_HANDLE_VALUE;

static std::wstring g_exeDir;
static std::wstring g_mpvPath;
static std::vector<std::wstring> g_playlist;
static int g_plIndex = -1;
static bool g_playing = false;
static bool g_muted = false;
static bool g_shuffle = false;
static int  g_repeat = 0; // 0=none,1=one,2=all
static bool g_fullscreen = false;
static bool g_ontop = false;
static bool g_plVisible = true;
static double g_duration = 0;
static double g_pos = 0;
static int g_volume = 100;
static double g_speed = 1.0;
static bool g_seeking = false;
static bool g_abloop_a = false;
static bool g_abloop_b = false;
static RECT g_savedWndRect;
static std::wstring g_ipcPipeName;
static bool g_mpvReady = false;
static int g_subTrackCount = 0;
static int g_audioTrackCount = 0;
static std::wstring g_snapDir;

// Decode thread state
struct DecodeState {
    std::wstring filePath;
    HANDLE hWrite;
    bool* pStop;
};
static bool g_stopDecode = false;

// Forward declarations
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
void SendMpvCmd(const std::string& cmd);
void OpenFile(const std::wstring& path);
void PlayCurrent();
void PlayNext(bool wrap=true);
void PlayPrev();
void PlayNextInDir();
void PlayPrevInDir();
void TogglePlayPause();
void StopPlayback();
void SeekTo(double pos);
void SetVolume(int vol);
void SetSpeed(double spd);
void ToggleMute();
void ToggleFullscreen();
void TakeSnapshot();
void UpdateTimeLabel();
void UpdatePlaylistUI();
void AddToPlaylist(const std::wstring& path);
void AddFolderToPlaylist(const std::wstring& folder);
std::vector<std::wstring> GetDirVideos(const std::wstring& dir);
void RegisterAssoc();
HMENU CreateMainMenu();
void ShowFileProps();
void ShowKeyHelp();

// ------- IPC via named pipe -------
static DWORD WINAPI IpcListenerThread(LPVOID) {
    HANDLE hPipe = CreateNamedPipeW(
        g_ipcPipeName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, 4096, 4096, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) return 0;
    ConnectNamedPipe(hPipe, NULL);
    g_hPipe = hPipe;
    g_mpvReady = true;
    char buf[4096];
    DWORD nr;
    std::string leftover;
    while (ReadFile(hPipe, buf, sizeof(buf)-1, &nr, NULL) && nr > 0) {
        buf[nr] = 0;
        leftover += buf;
        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string line = leftover.substr(0, pos);
            leftover = leftover.substr(pos+1);
            if (line.find("\"playback-restart\"") != std::string::npos) {
                g_playing = true;
                PostMessage(g_hMain, WM_USER+10, 0, 0);
            } else if (line.find("\"end-file\"") != std::string::npos) {
                g_playing = false;
                PostMessage(g_hMain, WM_USER+11, 0, 0);
            }
            auto dp = line.find("\"duration\":");
            if (dp != std::string::npos) {
                double v = atof(line.c_str()+dp+11);
                if (v > 0) { g_duration = v; PostMessage(g_hMain, WM_USER+12, 0, 0); }
            }
            auto tp = line.find("\"time-pos\":");
            if (tp != std::string::npos && !g_seeking) {
                g_pos = atof(line.c_str()+tp+11);
                PostMessage(g_hMain, WM_USER+13, 0, 0);
            }
        }
    }
    CloseHandle(hPipe);
    g_hPipe = INVALID_HANDLE_VALUE;
    return 0;
}

void SendMpvCmd(const std::string& cmd) {
    if (g_hPipe == INVALID_HANDLE_VALUE || !g_mpvReady) return;
    std::string s = cmd + "\n";
    DWORD nw;
    WriteFile(g_hPipe, s.c_str(), (DWORD)s.size(), &nw, NULL);
}

// ------- Decode thread: XOR header then stream -------
static DWORD WINAPI DecodeThread(LPVOID param) {
    DecodeState* st = (DecodeState*)param;
    HANDLE hFile = CreateFileW(st->filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) { delete st; return 1; }
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, BUF_SIZE);
    DWORD nr, nw;
    DWORD xorLeft = XOR_SIZE;
    while (!(*st->pStop) && ReadFile(hFile, buf, BUF_SIZE, &nr, NULL) && nr > 0) {
        if (xorLeft > 0) {
            DWORD toXor = min(nr, xorLeft);
            for (DWORD i = 0; i < toXor; i++) buf[i] ^= XOR_KEY;
            xorLeft -= toXor;
        }
        if (!WriteFile(st->hWrite, buf, nr, &nw, NULL)) break;
    }
    HeapFree(GetProcessHeap(), 0, buf);
    CloseHandle(hFile);
    CloseHandle(st->hWrite);
    delete st;
    return 0;
}

// ------- Launch mpv with embedded window -------
void LaunchMpv(const std::wstring& filePath) {
    g_stopDecode = true;
    if (g_hDecodeThread) { WaitForSingleObject(g_hDecodeThread, 2000); CloseHandle(g_hDecodeThread); g_hDecodeThread = NULL; }
    if (g_hMpvProc) { TerminateProcess(g_hMpvProc, 0); CloseHandle(g_hMpvProc); g_hMpvProc = NULL; }
    if (g_hPipeWrite != INVALID_HANDLE_VALUE) { CloseHandle(g_hPipeWrite); g_hPipeWrite = INVALID_HANDLE_VALUE; }
    if (g_hPipeRead  != INVALID_HANDLE_VALUE) { CloseHandle(g_hPipeRead);  g_hPipeRead  = INVALID_HANDLE_VALUE; }

    g_mpvReady = false;
    g_duration = 0; g_pos = 0; g_playing = false;
    g_stopDecode = false;

    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    CreatePipe(&g_hPipeRead, &g_hPipeWrite, &sa, 0);
    SetHandleInformation(g_hPipeWrite, HANDLE_FLAG_INHERIT, 0);

    WCHAR pipeName[64];
    StringCchPrintfW(pipeName, 64, L"\\\\.\\pipe\\vidiof_%u", GetCurrentProcessId());
    g_ipcPipeName = pipeName;

    WCHAR wid[32];
    StringCchPrintfW(wid, 32, L"%llu", (unsigned long long)(UINT_PTR)g_hVideo);

    WCHAR confDir[MAX_PATH];
    StringCchCopyW(confDir, MAX_PATH, g_exeDir.c_str());
    size_t clen = wcslen(confDir);
    if (clen > 0 && confDir[clen-1] == L'\\') confDir[clen-1] = L'\0';

    WCHAR snapDirW[MAX_PATH];
    StringCchPrintfW(snapDirW, MAX_PATH, L"%ssnaps", g_exeDir.c_str());
    CreateDirectoryW(snapDirW, NULL);
    g_snapDir = snapDirW;

    WCHAR cmdLine[1024];
    StringCchPrintfW(cmdLine, 1024,
        L"\"%s\""
        L" --no-terminal"
        L" --wid=%s"
        L" --input-ipc-server=\"%s\""
        L" --config-dir=\"%s\""
        L" --screenshot-directory=\"%s\""
        L" --keep-open=yes"
        L" --volume=%d"
        L" --speed=%.3f"
        L" -",
        g_mpvPath.c_str(), wid, g_ipcPipeName.c_str(),
        confDir, snapDirW,
        g_volume, g_speed);

    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = g_hPipeRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        MessageBoxW(g_hMain, L"שגיאה בהפעלת mpv.exe", L"שגיאה", MB_ICONERROR);
        return;
    }
    CloseHandle(g_hPipeRead); g_hPipeRead = INVALID_HANDLE_VALUE;
    g_hMpvProc = pi.hProcess;
    CloseHandle(pi.hThread);

    CreateThread(NULL, 0, IpcListenerThread, NULL, 0, NULL);

    Sleep(300);
    SendMpvCmd("{\"command\":[\"observe_property\",1,\"time-pos\"]}");
    SendMpvCmd("{\"command\":[\"observe_property\",2,\"duration\"]}");
    SendMpvCmd("{\"command\":[\"observe_property\",3,\"pause\"]}");

    DecodeState* ds = new DecodeState;
    ds->filePath = filePath;
    ds->hWrite   = g_hPipeWrite;
    ds->pStop    = &g_stopDecode;
    g_hPipeWrite = INVALID_HANDLE_VALUE;
    g_hDecodeThread = CreateThread(NULL, 0, DecodeThread, ds, 0, NULL);

    g_playing = true;
}

// ------- Helpers -------
std::wstring ToLower(std::wstring s) {
    for (auto& c : s) c = towlower(c);
    return s;
}

bool IsVideoFile(const std::wstring& p) {
    std::wstring ext = ToLower(PathFindExtensionW(p.c_str()));
    return ext == L".vidiof" || ext == L".mp4" || ext == L".mkv" || ext == L".avi"
        || ext == L".mov" || ext == L".wmv" || ext == L".flv" || ext == L".ts"
        || ext == L".m4v" || ext == L".mpeg" || ext == L".mpg" || ext == L".webm";
}

std::vector<std::wstring> GetDirVideos(const std::wstring& dir) {
    std::vector<std::wstring> v;
    WIN32_FIND_DATAW fd;
    std::wstring pat = dir + L"\\*";
    HANDLE hf = FindFirstFileW(pat.c_str(), &fd);
    if (hf == INVALID_HANDLE_VALUE) return v;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring name = fd.cFileName;
            if (IsVideoFile(name)) v.push_back(dir + L"\\" + name);
        }
    } while (FindNextFileW(hf, &fd));
    FindClose(hf);
    std::sort(v.begin(), v.end(), [](const std::wstring& a, const std::wstring& b){
        return CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
            a.c_str(),-1,b.c_str(),-1) == CSTR_LESS_THAN;
    });
    return v;
}

void AddToPlaylist(const std::wstring& path) {
    for (auto& p : g_playlist) if (p == path) return;
    g_playlist.push_back(path);
    UpdatePlaylistUI();
}

void AddFolderToPlaylist(const std::wstring& folder) {
    auto v = GetDirVideos(folder);
    for (auto& f : v) AddToPlaylist(f);
}

void UpdatePlaylistUI() {
    if (!g_hPlaylist) return;
    SendMessageW(g_hPlaylist, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < (int)g_playlist.size(); i++) {
        WCHAR name[MAX_PATH];
        StringCchCopyW(name, MAX_PATH, PathFindFileNameW(g_playlist[i].c_str()));
        PathRemoveExtensionW(name);
        SendMessageW(g_hPlaylist, LB_ADDSTRING, 0, (LPARAM)name);
    }
    if (g_plIndex >= 0 && g_plIndex < (int)g_playlist.size()) {
        SendMessageW(g_hPlaylist, LB_SETCURSEL, g_plIndex, 0);
    }
}

void PlayCurrent() {
    if (g_plIndex < 0 || g_plIndex >= (int)g_playlist.size()) return;
    LaunchMpv(g_playlist[g_plIndex]);
    UpdatePlaylistUI();
    WCHAR title[MAX_PATH+32];
    WCHAR name[MAX_PATH];
    StringCchCopyW(name, MAX_PATH, PathFindFileNameW(g_playlist[g_plIndex].c_str()));
    PathRemoveExtensionW(name);
    StringCchPrintfW(title, MAX_PATH+32, L"%s — VIDIOF נגן", name);
    SetWindowTextW(g_hMain, title);
    SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)name);
}

void PlayNext(bool wrap) {
    if (g_playlist.empty()) return;
    if (g_shuffle) {
        g_plIndex = rand() % (int)g_playlist.size();
    } else {
        g_plIndex++;
        if (g_plIndex >= (int)g_playlist.size()) {
            if (g_repeat == 2 || wrap) g_plIndex = 0;
            else { g_plIndex = (int)g_playlist.size()-1; return; }
        }
    }
    PlayCurrent();
}

void PlayPrev() {
    if (g_playlist.empty()) return;
    g_plIndex--;
    if (g_plIndex < 0) g_plIndex = (int)g_playlist.size()-1;
    PlayCurrent();
}

void PlayNextInDir() {
    if (g_plIndex < 0 || g_plIndex >= (int)g_playlist.size()) return;
    std::wstring curFile = g_playlist[g_plIndex];
    WCHAR dir[MAX_PATH];
    StringCchCopyW(dir, MAX_PATH, curFile.c_str());
    PathRemoveFileSpecW(dir);
    auto files = GetDirVideos(dir);
    auto it = std::find(files.begin(), files.end(), curFile);
    if (it == files.end() || ++it == files.end()) it = files.begin();
    if (it == files.end()) return;
    AddToPlaylist(*it);
    auto idx = std::find(g_playlist.begin(), g_playlist.end(), *it);
    if (idx != g_playlist.end()) {
        g_plIndex = (int)(idx - g_playlist.begin());
        PlayCurrent();
    }
}

void PlayPrevInDir() {
    if (g_plIndex < 0 || g_plIndex >= (int)g_playlist.size()) return;
    std::wstring curFile = g_playlist[g_plIndex];
    WCHAR dir[MAX_PATH];
    StringCchCopyW(dir, MAX_PATH, curFile.c_str());
    PathRemoveFileSpecW(dir);
    auto files = GetDirVideos(dir);
    auto it = std::find(files.begin(), files.end(), curFile);
    if (it == files.end()) return;
    if (it == files.begin()) it = files.end();
    --it;
    AddToPlaylist(*it);
    auto idx = std::find(g_playlist.begin(), g_playlist.end(), *it);
    if (idx != g_playlist.end()) {
        g_plIndex = (int)(idx - g_playlist.begin());
        PlayCurrent();
    }
}

void OpenFile(const std::wstring& path) {
    AddToPlaylist(path);
    g_plIndex = (int)g_playlist.size()-1;
    PlayCurrent();
}

void TogglePlayPause() {
    SendMpvCmd("{\"command\":[\"cycle\",\"pause\"]}");
    g_playing = !g_playing;
    SetWindowTextW(g_hBtnPlay, g_playing ? L"⏸" : L"▶");
}

void StopPlayback() {
    SendMpvCmd("{\"command\":[\"stop\"]}");
    g_playing = false;
    g_pos = 0;
    SetWindowTextW(g_hBtnPlay, L"▶");
    SetScrollPos(g_hSeek, SB_CTL, 0, TRUE);
    UpdateTimeLabel();
}

void SeekTo(double pos) {
    char cmd[128];
    sprintf_s(cmd, "{\"command\":[\"seek\",%.3f,\"absolute\"]}", pos);
    SendMpvCmd(cmd);
}

void SetVolume(int vol) {
    g_volume = max(0, min(200, vol));
    char cmd[64];
    sprintf_s(cmd, "{\"command\":[\"set_property\",\"volume\",%d]}", g_volume);
    SendMpvCmd(cmd);
    SetScrollPos(g_hVol, SB_CTL, g_volume, TRUE);
    if (g_muted && g_volume > 0) { g_muted = false; SetWindowTextW(g_hBtnMute, L"🔊"); }
}

void SetSpeed(double spd) {
    g_speed = spd;
    char cmd[64];
    sprintf_s(cmd, "{\"command\":[\"set_property\",\"speed\",%.3f]}", spd);
    SendMpvCmd(cmd);
    WCHAR label[32];
    StringCchPrintfW(label, 32, L"%.2fx", spd);
    SetWindowTextW(g_hSpeedLabel, label);
}

void ToggleMute() {
    g_muted = !g_muted;
    SendMpvCmd(g_muted ? "{\"command\":[\"set_property\",\"mute\",true]}"
                       : "{\"command\":[\"set_property\",\"mute\",false]}");
    SetWindowTextW(g_hBtnMute, g_muted ? L"🔇" : L"🔊");
}

void ToggleFullscreen() {
    g_fullscreen = !g_fullscreen;
    if (g_fullscreen) {
        GetWindowRect(g_hMain, &g_savedWndRect);
        HMONITOR hMon = MonitorFromWindow(g_hMain, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(hMon, &mi);
        SetWindowLongW(g_hMain, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g_hMain, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_FRAMECHANGED);
    } else {
        SetWindowLongW(g_hMain, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(g_hMain, NULL,
            g_savedWndRect.left, g_savedWndRect.top,
            g_savedWndRect.right - g_savedWndRect.left,
            g_savedWndRect.bottom - g_savedWndRect.top,
            SWP_FRAMECHANGED);
    }
    SetWindowTextW(g_hBtnFull, g_fullscreen ? L"⊡" : L"⛶");
    SendMpvCmd(g_fullscreen
        ? "{\"command\":[\"set_property\",\"fullscreen\",true]}"
        : "{\"command\":[\"set_property\",\"fullscreen\",false]}");
}

void TakeSnapshot() {
    SendMpvCmd("{\"command\":[\"screenshot\",\"video\"]}");
    WCHAR msg[MAX_PATH+64];
    StringCchPrintfW(msg, MAX_PATH+64, L"תמונה נשמרה ב:\n%s", g_snapDir.c_str());
    MessageBoxW(g_hMain, msg, L"תמונת מסך", MB_ICONINFORMATION);
}

static WCHAR g_timeBuf[64];
void UpdateTimeLabel() {
    int cur = (int)g_pos, tot = (int)g_duration;
    StringCchPrintfW(g_timeBuf, 64, L"%02d:%02d:%02d / %02d:%02d:%02d",
        cur/3600, (cur%3600)/60, cur%60,
        tot/3600, (tot%3600)/60, tot%60);
    SetWindowTextW(g_hTimeLabel, g_timeBuf);
    if (g_duration > 0 && !g_seeking) {
        int pos = (int)(g_pos / g_duration * 1000);
        SetScrollPos(g_hSeek, SB_CTL, pos, TRUE);
    }
}

void ShowFileProps() {
    if (g_plIndex < 0 || g_plIndex >= (int)g_playlist.size()) return;
    WCHAR msg[1024];
    WIN32_FILE_ATTRIBUTE_DATA fa;
    const std::wstring& path = g_playlist[g_plIndex];
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa);
    ULARGE_INTEGER sz; sz.HighPart = fa.nFileSizeHigh; sz.LowPart = fa.nFileSizeLow;
    FILETIME ft = fa.ftLastWriteTime;
    SYSTEMTIME st; FileTimeToSystemTime(&ft, &st);
    StringCchPrintfW(msg, 1024,
        L"נתיב: %s\n\nגודל: %llu MB\nתאריך שינוי: %02d/%02d/%04d %02d:%02d\n\nמשך: %02d:%02d:%02d",
        path.c_str(),
        sz.QuadPart / (1024*1024),
        st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute,
        (int)g_duration/3600, ((int)g_duration%3600)/60, (int)g_duration%60);
    MessageBoxW(g_hMain, msg, L"מאפייני קובץ", MB_ICONINFORMATION);
}

void ShowKeyHelp() {
    MessageBoxW(g_hMain,
        L"קיצורי מקלדת:\n\n"
        L"Space        — נגן / עצור\n"
        L"← →          — דלג 5 שניות\n"
        L"Ctrl+← →     — דלג דקה\n"
        L"↑ ↓          — עוצמה קול\n"
        L"M            — מיוט\n"
        L"F            — מסך מלא\n"
        L"[ ]          — מהירות –/+\n"
        L". ,          — פריים קדימה/אחורה\n"
        L"N / P        — הבא / הקודם\n"
        L"Ctrl+N/P     — הבא/קודם בתיקייה\n"
        L"S            — תמונת מסך\n"
        L"L            — חזרה: אחד/הכל/כבוי\n"
        L"R            — ערבוב\n"
        L"Esc          — יציאה ממסך מלא\n"
        L"Q            — סגור",
        L"עזרה — קיצורי מקלדת", MB_ICONINFORMATION);
}

void RegisterAssoc() {
    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    WCHAR cmd[MAX_PATH*2];
    StringCchPrintfW(cmd, MAX_PATH*2, L"\"%s\" \"%%1\"", exePath);
    HKEY hk;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.VIDIOF", 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL);
    RegSetValueExW(hk, NULL, 0, REG_SZ, (BYTE*)L"VIDIOF.Player", 26);
    RegCloseKey(hk);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\VIDIOF.Player\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL);
    RegSetValueExW(hk, NULL, 0, REG_SZ, (BYTE*)cmd, (DWORD)(wcslen(cmd)+1)*2);
    RegCloseKey(hk);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

HMENU CreateMainMenu() {
    HMENU hMenu = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN,  L"פתח קובץ...\tCtrl+O");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN_URL, L"פתח כתובת URL...");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_ADD,   L"הוסף לרשימה...");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SNAP,  L"שמור תמונת מסך\tS");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_PROPS, L"מאפייני קובץ...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"קובץ");

    HMENU hPlay = CreatePopupMenu();
    AppendMenuW(hPlay, MF_STRING, IDM_PLAY_PLAY,    L"נגן / עצור\tSpace");
    AppendMenuW(hPlay, MF_STRING, IDM_PLAY_STOP,    L"עצור");
    AppendMenuW(hPlay, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPlay, MF_STRING, IDM_PLAY_PREV,    L"קודם\tP");
    AppendMenuW(hPlay, MF_STRING, IDM_PLAY_NEXT,    L"הבא\tN");
    AppendMenuW(hPlay, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPlay, MF_STRING, IDM_PLAY_FRAME_F, L"פריים קדימה\t.");
    AppendMenuW(hPlay, MF_STRING, IDM_PLAY_FRAME_B, L"פריים אחורה\t,");
    AppendMenuW(hPlay, MF_SEPARATOR, 0, NULL);
    HMENU hSpeed = CreatePopupMenu();
    AppendMenuW(hSpeed, MF_STRING, IDM_PLAY_SPEED_25,  L"0.25×");
    AppendMenuW(hSpeed, MF_STRING, IDM_PLAY_SPEED_50,  L"0.50×");
    AppendMenuW(hSpeed, MF_STRING, IDM_PLAY_SPEED_75,  L"0.75×");
    AppendMenuW(hSpeed, MF_STRING, IDM_PLAY_SPEED_100, L"1.00× (רגיל)");
    AppendMenuW(hSpeed, MF_STRING, IDM_PLAY_SPEED_125, L"1.25×");
    AppendMenuW(hSpeed, MF_STRING, IDM_PLAY_SPEED_150, L"1.50×");
    AppendMenuW(hSpeed, MF_STRING, IDM_PLAY_SPEED_200, L"2.00×");
    AppendMenuW(hSpeed, MF_STRING, IDM_PLAY_SPEED_400, L"4.00×");
    AppendMenuW(hPlay, MF_POPUP, (UINT_PTR)hSpeed, L"מהירות נגינה");
    AppendMenuW(hPlay, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPlay, MF_STRING, IDM_PLAY_ABLOOP,  L"AB Loop");
    AppendMenuW(hPlay, MF_SEPARATOR, 0, NULL);
    HMENU hRepeat = CreatePopupMenu();
    AppendMenuW(hRepeat, MF_STRING, IDM_PLAY_REPEAT_NONE, L"ללא חזרה");
    AppendMenuW(hRepeat, MF_STRING, IDM_PLAY_REPEAT_ONE,  L"חזור על הקובץ");
    AppendMenuW(hRepeat, MF_STRING, IDM_PLAY_REPEAT_ALL,  L"חזור על הרשימה");
    AppendMenuW(hPlay, MF_POPUP, (UINT_PTR)hRepeat, L"מצב חזרה");
    AppendMenuW(hPlay, MF_STRING, IDM_PLAY_SHUFFLE, L"ערבוב");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPlay, L"נגינה");

    HMENU hVideo = CreatePopupMenu();
    AppendMenuW(hVideo, MF_STRING, IDM_VIDEO_FULLSCREEN, L"מסך מלא\tF");
    AppendMenuW(hVideo, MF_STRING, IDM_VIDEO_ONTOP,      L"תמיד מעל");
    AppendMenuW(hVideo, MF_SEPARATOR, 0, NULL);
    HMENU hAspect = CreatePopupMenu();
    AppendMenuW(hAspect, MF_STRING, IDM_VIDEO_ASPECT_FREE, L"חופשי");
    AppendMenuW(hAspect, MF_STRING, IDM_VIDEO_ASPECT_43,   L"4:3");
    AppendMenuW(hAspect, MF_STRING, IDM_VIDEO_ASPECT_169,  L"16:9");
    AppendMenuW(hAspect, MF_STRING, IDM_VIDEO_ASPECT_235,  L"2.35:1");
    AppendMenuW(hVideo, MF_POPUP, (UINT_PTR)hAspect, L"יחס גובה-רוחב");
    HMENU hZoom = CreatePopupMenu();
    AppendMenuW(hZoom, MF_STRING, IDM_VIDEO_ZOOM_50,  L"50%");
    AppendMenuW(hZoom, MF_STRING, IDM_VIDEO_ZOOM_100, L"100%");
    AppendMenuW(hZoom, MF_STRING, IDM_VIDEO_ZOOM_200, L"200%");
    AppendMenuW(hVideo, MF_POPUP, (UINT_PTR)hZoom, L"זום");
    HMENU hRot = CreatePopupMenu();
    AppendMenuW(hRot, MF_STRING, IDM_VIDEO_ROT_0,   L"0°");
    AppendMenuW(hRot, MF_STRING, IDM_VIDEO_ROT_90,  L"90°");
    AppendMenuW(hRot, MF_STRING, IDM_VIDEO_ROT_180, L"180°");
    AppendMenuW(hRot, MF_STRING, IDM_VIDEO_ROT_270, L"270°");
    AppendMenuW(hVideo, MF_POPUP, (UINT_PTR)hRot, L"סיבוב");
    AppendMenuW(hVideo, MF_STRING, IDM_VIDEO_FLIP_H, L"שיקוף אופקי");
    AppendMenuW(hVideo, MF_STRING, IDM_VIDEO_FLIP_V, L"שיקוף אנכי");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hVideo, L"וידאו");

    HMENU hAudio = CreatePopupMenu();
    AppendMenuW(hAudio, MF_STRING, IDM_AUDIO_MUTE,    L"השתק / בטל השתק\tM");
    AppendMenuW(hAudio, MF_STRING, IDM_AUDIO_VOL_P,   L"הגבר עוצמה\t↑");
    AppendMenuW(hAudio, MF_STRING, IDM_AUDIO_VOL_M,   L"הנמך עוצמה\t↓");
    AppendMenuW(hAudio, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAudio, MF_STRING, IDM_AUDIO_DELAY_P, L"השהיית שמע +100ms");
    AppendMenuW(hAudio, MF_STRING, IDM_AUDIO_DELAY_M, L"השהיית שמע -100ms");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAudio, L"שמע");

    HMENU hSub = CreatePopupMenu();
    AppendMenuW(hSub, MF_STRING, IDM_SUB_LOAD,    L"טען קובץ כתוביות...");
    AppendMenuW(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSub, MF_STRING, IDM_SUB_SIZE_P,  L"הגדל כתוביות");
    AppendMenuW(hSub, MF_STRING, IDM_SUB_SIZE_M,  L"הקטן כתוביות");
    AppendMenuW(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSub, MF_STRING, IDM_SUB_DELAY_P, L"השהיית כתוביות +100ms");
    AppendMenuW(hSub, MF_STRING, IDM_SUB_DELAY_M, L"השהיית כתוביות -100ms");
    AppendMenuW(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSub, MF_STRING, IDM_SUB_HIDE,    L"הסתר / הצג כתוביות");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSub, L"כתוביות");

    HMENU hDir = CreatePopupMenu();
    AppendMenuW(hDir, MF_STRING, IDM_DIR_NEXT, L"הבא בתיקייה\tCtrl+N");
    AppendMenuW(hDir, MF_STRING, IDM_DIR_PREV, L"קודם בתיקייה\tCtrl+P");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hDir, L"תיקייה");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_INFO_FILE,  L"מידע על הקובץ");
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_KEYS,  L"קיצורי מקלדת");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"עזרה");

    return hMenu;
}

// ------- Layout -------
#define CTRL_H    36
#define TOOLBAR_H 44
#define SEEK_H    18
#define STATUSBAR_H 22
#define PLAYLIST_W  220

void LayoutControls() {
    if (!g_hMain) return;
    RECT rc; GetClientRect(g_hMain, &rc);
    int W = rc.right, H = rc.bottom;
    if (g_fullscreen) {
        SetWindowPos(g_hVideo, NULL, 0, 0, W, H, SWP_NOZORDER);
        if (g_hOverlay) ShowWindow(g_hOverlay, SW_HIDE);
        ShowWindow(g_hToolbar, SW_HIDE);
        ShowWindow(g_hSeek, SW_HIDE);
        ShowWindow(g_hVol, SW_HIDE);
        ShowWindow(g_hStatus, SW_HIDE);
        if (g_hPlaylist) ShowWindow(g_hPlaylist, SW_HIDE);
        return;
    }
    ShowWindow(g_hToolbar, SW_SHOW);
    ShowWindow(g_hSeek, SW_SHOW);
    ShowWindow(g_hStatus, SW_SHOW);

    int plW = (g_plVisible && g_hPlaylist) ? PLAYLIST_W : 0;
    int videoW = W - plW;
    int bottomH = SEEK_H + TOOLBAR_H + STATUSBAR_H;
    int videoH = H - bottomH;
    if (videoH < 50) videoH = 50;

    SetWindowPos(g_hVideo, NULL, 0, 0, videoW, videoH, SWP_NOZORDER);
    SetWindowPos(g_hOverlay, HWND_TOP, 0, 0, videoW, videoH, SWP_SHOWWINDOW);

    if (g_hPlaylist && g_plVisible) {
        ShowWindow(g_hPlaylist, SW_SHOW);
        SetWindowPos(g_hPlaylist, NULL, videoW, 0, plW, videoH, SWP_NOZORDER);
    } else if (g_hPlaylist) {
        ShowWindow(g_hPlaylist, SW_HIDE);
    }

    SetWindowPos(g_hSeek, NULL, 0, videoH, W, SEEK_H, SWP_NOZORDER);

    int y = videoH + SEEK_H;
    int x = 4;
    auto btn = [&](HWND h, int w) {
        SetWindowPos(h, NULL, x, y + (TOOLBAR_H-CTRL_H)/2, w, CTRL_H, SWP_NOZORDER);
        x += w + 4;
    };
    btn(g_hBtnOpen,    70);
    btn(g_hBtnAdd,     50);
    x += 6;
    btn(g_hBtnPrevDir, 36);
    btn(g_hBtnPrev,    36);
    btn(g_hBtnPlay,    48);
    btn(g_hBtnStop,    36);
    btn(g_hBtnNext,    36);
    btn(g_hBtnNextDir, 36);
    x += 6;
    btn(g_hBtnMute,    36);
    SetWindowPos(g_hVol, NULL, x, y + (TOOLBAR_H-20)/2, 80, 20, SWP_NOZORDER);
    ShowWindow(g_hVol, SW_SHOW);
    x += 84;
    x += 6;
    btn(g_hBtnShuffle, 36);
    btn(g_hBtnRepeat,  36);
    x += 6;
    btn(g_hBtnSnap,    36);
    btn(g_hBtnFull,    36);
    x += 6;
    SetWindowPos(g_hTimeLabel, NULL, x, y + (TOOLBAR_H-20)/2, 160, 20, SWP_NOZORDER);
    x += 164;
    SetWindowPos(g_hSpeedLabel, NULL, x, y + (TOOLBAR_H-20)/2, 50, 20, SWP_NOZORDER);

    SetWindowPos(g_hStatus, NULL, 0, H-STATUSBAR_H, W, STATUSBAR_H, SWP_NOZORDER);
    SendMessageW(g_hStatus, WM_SIZE, 0, 0);
}

// ------- WndProc -------
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        DragAcceptFiles(hWnd, TRUE);
        SetTimer(hWnd, TIMER_SEEK, 500, NULL);
        return 0;
    }
    case WM_SIZE:
        LayoutControls();
        return 0;
    case WM_TIMER:
        if (wParam == TIMER_SEEK) {
            UpdateTimeLabel();
        }
        return 0;
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < n; i++) {
            WCHAR path[MAX_PATH];
            DragQueryFileW(hDrop, i, path, MAX_PATH);
            DWORD attr = GetFileAttributesW(path);
            if (attr & FILE_ATTRIBUTE_DIRECTORY) AddFolderToPlaylist(path);
            else if (IsVideoFile(path)) {
                AddToPlaylist(path);
                if (g_plIndex < 0) { g_plIndex = (int)g_playlist.size()-1; PlayCurrent(); }
            }
        }
        DragFinish(hDrop);
        return 0;
    }
    case WM_KEYDOWN: {
        bool ctrl = GetKeyState(VK_CONTROL) & 0x8000;
        switch (wParam) {
        case VK_SPACE: TogglePlayPause(); break;
        case VK_LEFT:  SeekTo(ctrl ? g_pos-60 : g_pos-5); break;
        case VK_RIGHT: SeekTo(ctrl ? g_pos+60 : g_pos+5); break;
        case VK_UP:    SetVolume(g_volume+5); break;
        case VK_DOWN:  SetVolume(g_volume-5); break;
        case 'M':      ToggleMute(); break;
        case 'F':      ToggleFullscreen(); break;
        case 'S':      TakeSnapshot(); break;
        case 'N':      ctrl ? PlayNextInDir() : PlayNext(); break;
        case 'P':      ctrl ? PlayPrevInDir() : PlayPrev(); break;
        case 'Q':      SendMessageW(hWnd, WM_CLOSE, 0, 0); break;
        case 'L': g_repeat = (g_repeat+1)%3;
            SetWindowTextW(g_hBtnRepeat, g_repeat==1?L"🔂":g_repeat==2?L"🔁":L"↩");
            break;
        case 'R': g_shuffle = !g_shuffle;
            SetWindowTextW(g_hBtnShuffle, g_shuffle ? L"🔀" : L"⇄");
            break;
        case VK_OEM_PERIOD:
            SendMpvCmd("{\"command\":[\"frame-step\"]}"); break;
        case VK_OEM_COMMA:
            SendMpvCmd("{\"command\":[\"frame-back-step\"]}"); break;
        case VK_OEM_4:
            SetSpeed(max(0.25, g_speed-0.25)); break;
        case VK_OEM_6:
            SetSpeed(min(4.0, g_speed+0.25)); break;
        case VK_ESCAPE:
            if (g_fullscreen) ToggleFullscreen(); break;
        }
        return 0;
    }
    case WM_USER+10:
        SetWindowTextW(g_hBtnPlay, L"⏸");
        break;
    case WM_USER+11: {
        SetWindowTextW(g_hBtnPlay, L"▶");
        if (g_repeat == 1) { PlayCurrent(); }
        else if (g_repeat == 2 || g_plIndex < (int)g_playlist.size()-1) PlayNext(g_repeat==2);
        break;
    }
    case WM_USER+12:
    case WM_USER+13:
        UpdateTimeLabel();
        break;
    case WM_HSCROLL: {
        HWND hCtrl = (HWND)lParam;
        if (hCtrl == g_hSeek) {
            int code = LOWORD(wParam);
            if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION) {
                g_seeking = true;
                int pos = HIWORD(wParam);
                if (g_duration > 0) {
                    g_pos = g_duration * pos / 1000.0;
                    UpdateTimeLabel();
                }
                if (code == SB_THUMBPOSITION) {
                    SeekTo(g_pos);
                    g_seeking = false;
                }
            } else if (code == SB_ENDSCROLL) {
                g_seeking = false;
            }
        } else if (hCtrl == g_hVol) {
            int pos = GetScrollPos(g_hVol, SB_CTL);
            switch (LOWORD(wParam)) {
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: pos = HIWORD(wParam); break;
            case SB_LINELEFT:  pos = max(0,   pos-5); break;
            case SB_LINERIGHT: pos = min(200, pos+5); break;
            case SB_PAGELEFT:  pos = max(0,   pos-20); break;
            case SB_PAGERIGHT: pos = min(200, pos+20); break;
            }
            SetVolume(pos);
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case ID_BTN_PLAY: TogglePlayPause(); break;
        case ID_BTN_STOP: StopPlayback(); break;
        case ID_BTN_PREV: PlayPrev(); break;
        case ID_BTN_NEXT: PlayNext(); break;
        case ID_BTN_PREV_DIR: PlayPrevInDir(); break;
        case ID_BTN_NEXT_DIR: PlayNextInDir(); break;
        case ID_BTN_FULL: ToggleFullscreen(); break;
        case ID_BTN_MUTE: ToggleMute(); break;
        case ID_BTN_SNAP: TakeSnapshot(); break;
        case ID_BTN_SHUFFLE:
            g_shuffle = !g_shuffle;
            SetWindowTextW(g_hBtnShuffle, g_shuffle ? L"🔀" : L"⇄");
            break;
        case ID_BTN_REPEAT:
            g_repeat = (g_repeat+1)%3;
            SetWindowTextW(g_hBtnRepeat, g_repeat==1?L"🔂":g_repeat==2?L"🔁":L"↩");
            break;
        case ID_BTN_OPEN:
        case IDM_FILE_OPEN: {
            WCHAR path[MAX_PATH] = {0};
            OPENFILENAMEW ofn = {sizeof(ofn)};
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"VIDIOF Files\0*.VIDIOF\0All Video\0*.mp4;*.mkv;*.avi;*.mov\0All Files\0*.*\0";
            ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            ofn.lpstrTitle = L"פתח קובץ";
            if (GetOpenFileNameW(&ofn)) OpenFile(path);
            break;
        }
        case IDM_FILE_OPEN_URL:
            MessageBoxW(hWnd, L"גרור קובץ מ-URL לחלון, או השתמש בפתח קובץ.", L"פתח כתובת URL", MB_ICONINFORMATION);
            break;
        case ID_BTN_ADD:
        case IDM_FILE_ADD: {
            WCHAR path[MAX_PATH] = {0};
            OPENFILENAMEW ofn = {sizeof(ofn)};
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"VIDIOF Files\0*.VIDIOF\0All Video\0*.mp4;*.mkv;*.avi\0All Files\0*.*\0";
            ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
            ofn.lpstrTitle = L"הוסף קבצים לרשימה";
            if (GetOpenFileNameW(&ofn)) {
                WCHAR* p = path;
                if (*(p + wcslen(p) + 1) == 0) { AddToPlaylist(p); }
                else {
                    std::wstring dir = p; p += wcslen(p)+1;
                    while (*p) { AddToPlaylist(dir + L"\\" + p); p += wcslen(p)+1; }
                }
            }
            break;
        }
        case IDM_FILE_SNAP:  TakeSnapshot(); break;
        case IDM_FILE_PROPS: ShowFileProps(); break;
        case IDM_PLAY_PLAY:  TogglePlayPause(); break;
        case IDM_PLAY_STOP:  StopPlayback(); break;
        case IDM_PLAY_PREV:  PlayPrev(); break;
        case IDM_PLAY_NEXT:  PlayNext(); break;
        case IDM_PLAY_FRAME_F: SendMpvCmd("{\"command\":[\"frame-step\"]}"); break;
        case IDM_PLAY_FRAME_B: SendMpvCmd("{\"command\":[\"frame-back-step\"]}"); break;
        case IDM_PLAY_SPEED_25:  SetSpeed(0.25); break;
        case IDM_PLAY_SPEED_50:  SetSpeed(0.50); break;
        case IDM_PLAY_SPEED_75:  SetSpeed(0.75); break;
        case IDM_PLAY_SPEED_100: SetSpeed(1.00); break;
        case IDM_PLAY_SPEED_125: SetSpeed(1.25); break;
        case IDM_PLAY_SPEED_150: SetSpeed(1.50); break;
        case IDM_PLAY_SPEED_200: SetSpeed(2.00); break;
        case IDM_PLAY_SPEED_400: SetSpeed(4.00); break;
        case IDM_PLAY_ABLOOP:
            if (!g_abloop_a) {
                SendMpvCmd("{\"command\":[\"ab-loop\"]}");
                g_abloop_a = true;
                SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)L"AB Loop: נקודה A סומנה");
            } else if (!g_abloop_b) {
                SendMpvCmd("{\"command\":[\"ab-loop\"]}");
                g_abloop_b = true;
                SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)L"AB Loop: פעיל");
            } else {
                SendMpvCmd("{\"command\":[\"ab-loop\"]}");
                g_abloop_a = g_abloop_b = false;
                SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)L"AB Loop: כבוי");
            }
            break;
        case IDM_PLAY_REPEAT_NONE: g_repeat=0; SetWindowTextW(g_hBtnRepeat,L"↩"); break;
        case IDM_PLAY_REPEAT_ONE:  g_repeat=1; SetWindowTextW(g_hBtnRepeat,L"🔂"); break;
        case IDM_PLAY_REPEAT_ALL:  g_repeat=2; SetWindowTextW(g_hBtnRepeat,L"🔁"); break;
        case IDM_PLAY_SHUFFLE:
            g_shuffle=!g_shuffle;
            SetWindowTextW(g_hBtnShuffle, g_shuffle?L"🔀":L"⇄");
            break;
        case IDM_VIDEO_FULLSCREEN: ToggleFullscreen(); break;
        case IDM_VIDEO_ONTOP:
            g_ontop = !g_ontop;
            SetWindowPos(hWnd, g_ontop?HWND_TOPMOST:HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
            break;
        case IDM_VIDEO_ASPECT_FREE: SendMpvCmd("{\"command\":[\"set_property\",\"video-aspect-override\",\"-1\"]}"); break;
        case IDM_VIDEO_ASPECT_43:   SendMpvCmd("{\"command\":[\"set_property\",\"video-aspect-override\",\"4:3\"]}"); break;
        case IDM_VIDEO_ASPECT_169:  SendMpvCmd("{\"command\":[\"set_property\",\"video-aspect-override\",\"16:9\"]}"); break;
        case IDM_VIDEO_ASPECT_235:  SendMpvCmd("{\"command\":[\"set_property\",\"video-aspect-override\",\"2.35:1\"]}"); break;
        case IDM_VIDEO_ZOOM_50:   SendMpvCmd("{\"command\":[\"set_property\",\"window-scale\",0.5]}"); break;
        case IDM_VIDEO_ZOOM_100:  SendMpvCmd("{\"command\":[\"set_property\",\"window-scale\",1.0]}"); break;
        case IDM_VIDEO_ZOOM_200:  SendMpvCmd("{\"command\":[\"set_property\",\"window-scale\",2.0]}"); break;
        case IDM_VIDEO_ROT_0:   SendMpvCmd("{\"command\":[\"set_property\",\"video-rotate\",0]}"); break;
        case IDM_VIDEO_ROT_90:  SendMpvCmd("{\"command\":[\"set_property\",\"video-rotate\",90]}"); break;
        case IDM_VIDEO_ROT_180: SendMpvCmd("{\"command\":[\"set_property\",\"video-rotate\",180]}"); break;
        case IDM_VIDEO_ROT_270: SendMpvCmd("{\"command\":[\"set_property\",\"video-rotate\",270]}"); break;
        case IDM_VIDEO_FLIP_H: SendMpvCmd("{\"command\":[\"vf\",\"toggle\",\"hflip\"]}"); break;
        case IDM_VIDEO_FLIP_V: SendMpvCmd("{\"command\":[\"vf\",\"toggle\",\"vflip\"]}"); break;
        case IDM_AUDIO_MUTE:    ToggleMute(); break;
        case IDM_AUDIO_VOL_P:   SetVolume(g_volume+5); break;
        case IDM_AUDIO_VOL_M:   SetVolume(g_volume-5); break;
        case IDM_AUDIO_DELAY_P: SendMpvCmd("{\"command\":[\"add\",\"audio-delay\",0.1]}"); break;
        case IDM_AUDIO_DELAY_M: SendMpvCmd("{\"command\":[\"add\",\"audio-delay\",-0.1]}"); break;
        case IDM_SUB_LOAD: {
            WCHAR path[MAX_PATH]={0};
            OPENFILENAMEW ofn={sizeof(ofn)};
            ofn.hwndOwner=hWnd;
            ofn.lpstrFilter=L"כתוביות\0*.srt;*.ass;*.ssa;*.sub;*.idx;*.vtt\0All\0*.*\0";
            ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH;
            ofn.Flags=OFN_FILEMUSTEXIST;
            ofn.lpstrTitle=L"טען כתוביות";
            if (GetOpenFileNameW(&ofn)) {
                std::string p(path, path+wcslen(path));
                std::string cmd = "{\"command\":[\"sub-add\",\""+p+"\"]}";
                SendMpvCmd(cmd);
            }
            break;
        }
        case IDM_SUB_SIZE_P:  SendMpvCmd("{\"command\":[\"add\",\"sub-scale\",0.1]}"); break;
        case IDM_SUB_SIZE_M:  SendMpvCmd("{\"command\":[\"add\",\"sub-scale\",-0.1]}"); break;
        case IDM_SUB_DELAY_P: SendMpvCmd("{\"command\":[\"add\",\"sub-delay\",0.1]}"); break;
        case IDM_SUB_DELAY_M: SendMpvCmd("{\"command\":[\"add\",\"sub-delay\",-0.1]}"); break;
        case IDM_SUB_HIDE:    SendMpvCmd("{\"command\":[\"cycle\",\"sub-visibility\"]}"); break;
        case IDM_DIR_NEXT: PlayNextInDir(); break;
        case IDM_DIR_PREV: PlayPrevInDir(); break;
        case IDM_INFO_FILE: ShowFileProps(); break;
        case IDM_HELP_KEYS: ShowKeyHelp(); break;
        case ID_PLAYLIST:
            if (HIWORD(wParam) == LBN_DBLCLK) {
                int sel = (int)SendMessageW(g_hPlaylist, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)g_playlist.size()) {
                    g_plIndex = sel;
                    PlayCurrent();
                }
            }
            break;
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {
        POINT pt; GetCursorPos(&pt);
        HMENU hCtx = CreatePopupMenu();
        AppendMenuW(hCtx, MF_STRING, IDM_PLAY_PLAY,        L"נגן / עצור");
        AppendMenuW(hCtx, MF_STRING, IDM_VIDEO_FULLSCREEN, L"מסך מלא");
        AppendMenuW(hCtx, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hCtx, MF_STRING, IDM_PLAY_NEXT,        L"הבא");
        AppendMenuW(hCtx, MF_STRING, IDM_PLAY_PREV,        L"קודם");
        AppendMenuW(hCtx, MF_STRING, IDM_DIR_NEXT,         L"הבא בתיקייה");
        AppendMenuW(hCtx, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hCtx, MF_STRING, IDM_FILE_SNAP,        L"תמונת מסך");
        AppendMenuW(hCtx, MF_STRING, IDM_FILE_PROPS,       L"מאפייני קובץ");
        TrackPopupMenu(hCtx, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(hCtx);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        // Check if click is on overlay (video area)
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT vrc; GetWindowRect(g_hVideo, &vrc);
        POINT spt = pt; ClientToScreen(hWnd, &spt);
        if (spt.x >= vrc.left && spt.x < vrc.right &&
            spt.y >= vrc.top  && spt.y < vrc.bottom) {
            TogglePlayPause();
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK:
        ToggleFullscreen();
        return 0;
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        SetVolume(g_volume + (delta>0?5:-5));
        return 0;
    }
    case WM_CLOSE:
        g_stopDecode = true;
        if (g_hMpvProc) { TerminateProcess(g_hMpvProc, 0); }
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ------- Button helper -------
HWND MakeBtn(HWND parent, const WCHAR* text, int id, const WCHAR* tip=NULL) {
    HWND h = CreateWindowW(L"BUTTON", text,
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_CENTER|BS_VCENTER,
        0,0,0,0, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (tip && tip[0]) {
        HWND hTip = CreateWindowW(TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_ALWAYSTIP,
            CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
            h, NULL, g_hInst, NULL);
        TOOLINFOW ti={sizeof(ti),TTF_IDISHWND|TTF_SUBCLASS};
        ti.hwnd=h; ti.uId=(UINT_PTR)h; ti.lpszText=(WCHAR*)tip;
        SendMessageW(hTip,TTM_ADDTOOLW,0,(LPARAM)&ti);
    }
    return h;
}

HWND MakeScrollbar(HWND parent, int id, int max) {
    HWND h = CreateWindowW(L"SCROLLBAR", NULL,
        WS_CHILD|WS_VISIBLE|SBS_HORZ,
        0,0,0,0, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    SCROLLINFO si={sizeof(si), SIF_RANGE|SIF_PAGE};
    si.nMin=0; si.nMax=max; si.nPage=1;
    SetScrollInfo(h, SB_CTL, &si, FALSE);
    return h;
}

// ------- WinMain -------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    g_hInst = hInst;
    srand(GetTickCount());

    INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_BAR_CLASSES|ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&ic);

    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    WCHAR exeDir[MAX_PATH];
    StringCchCopyW(exeDir, MAX_PATH, exePath);
    WCHAR* sl = wcsrchr(exeDir, L'\\');
    if (sl) *(sl+1) = L'\0';
    g_exeDir = exeDir;

    g_mpvPath = g_exeDir + L"mpv.exe";
    if (GetFileAttributesW(g_mpvPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"mpv.exe לא נמצא!\nשים mpv.exe באותה תיקייה כמו play.exe", L"שגיאה", MB_ICONERROR);
        return 1;
    }

    RegisterAssoc();

    WNDCLASSEXW wc={sizeof(wc)};
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"VIDIOFPlayer";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.style         = CS_DBLCLKS;
    RegisterClassExW(&wc);

    WNDCLASSEXW vc={sizeof(vc)};
    vc.lpfnWndProc   = DefWindowProcW;
    vc.hInstance     = hInst;
    vc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    vc.lpszClassName = L"VIDIOFVideoWnd";
    RegisterClassExW(&vc);

    g_hMain = CreateWindowExW(WS_EX_ACCEPTFILES, L"VIDIOFPlayer",
        L"נגן VIDIOF",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 680,
        NULL, CreateMainMenu(), hInst, NULL);

    // Register overlay class
    WNDCLASSEXW oc={sizeof(oc)};
    oc.lpfnWndProc   = DefWindowProcW;
    oc.hInstance     = hInst;
    oc.hbrBackground = NULL;
    oc.lpszClassName = L"VIDIOFOverlay";
    oc.style         = CS_DBLCLKS;
    RegisterClassExW(&oc);

    g_hVideo = CreateWindowExW(0, L"VIDIOFVideoWnd", NULL,
        WS_CHILD|WS_VISIBLE, 0,0,0,0, g_hMain, (HMENU)ID_VIDEO_PANEL, hInst, NULL);

    // Transparent overlay on top of video to capture mouse clicks
    g_hOverlay = CreateWindowExW(WS_EX_TRANSPARENT, L"VIDIOFOverlay", NULL,
        WS_CHILD|WS_VISIBLE, 0,0,0,0, g_hMain, NULL, hInst, NULL);

    g_hSeek = MakeScrollbar(g_hMain, ID_SEEK_BAR, 1000);
    g_hVol  = MakeScrollbar(g_hMain, ID_VOL_BAR, 200);
    SetScrollPos(g_hVol, SB_CTL, 100, FALSE);

    g_hBtnOpen    = MakeBtn(g_hMain, L"📂 פתח",   ID_BTN_OPEN,    L"פתח קובץ");
    g_hBtnAdd     = MakeBtn(g_hMain, L"+ הוסף",   ID_BTN_ADD,     L"הוסף קבצים לרשימה");
    g_hBtnPrevDir = MakeBtn(g_hMain, L"⏮̈",        ID_BTN_PREV_DIR,L"קודם בתיקייה (Ctrl+P)");
    g_hBtnPrev    = MakeBtn(g_hMain, L"⏮",        ID_BTN_PREV,    L"קודם ברשימה (P)");
    g_hBtnPlay    = MakeBtn(g_hMain, L"▶",         ID_BTN_PLAY,    L"נגן / עצור (Space)");
    g_hBtnStop    = MakeBtn(g_hMain, L"⏹",        ID_BTN_STOP,    L"עצור");
    g_hBtnNext    = MakeBtn(g_hMain, L"⏭",        ID_BTN_NEXT,    L"הבא ברשימה (N)");
    g_hBtnNextDir = MakeBtn(g_hMain, L"⏭̈",        ID_BTN_NEXT_DIR,L"הבא בתיקייה (Ctrl+N)");
    g_hBtnMute    = MakeBtn(g_hMain, L"🔊",        ID_BTN_MUTE,    L"השתק (M)");
    g_hBtnShuffle = MakeBtn(g_hMain, L"⇄",         ID_BTN_SHUFFLE, L"ערבוב (R)");
    g_hBtnRepeat  = MakeBtn(g_hMain, L"↩",         ID_BTN_REPEAT,  L"חזרה (L): כבוי/קובץ/הכל");
    g_hBtnSnap    = MakeBtn(g_hMain, L"📷",        ID_BTN_SNAP,    L"תמונת מסך (S)");
    g_hBtnFull    = MakeBtn(g_hMain, L"⛶",         ID_BTN_FULL,    L"מסך מלא (F)");

    g_hTimeLabel = CreateWindowW(L"STATIC", L"00:00:00 / 00:00:00",
        WS_CHILD|WS_VISIBLE|SS_LEFT, 0,0,0,0, g_hMain, (HMENU)ID_TIME_LABEL, hInst, NULL);
    g_hSpeedLabel = CreateWindowW(L"STATIC", L"1.00x",
        WS_CHILD|WS_VISIBLE|SS_LEFT, 0,0,0,0, g_hMain, (HMENU)ID_SPEED_LABEL, hInst, NULL);

    g_hPlaylist = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,
        0,0,0,0, g_hMain, (HMENU)ID_PLAYLIST, hInst, NULL);

    g_hStatus = CreateWindowW(STATUSCLASSNAMEW, NULL,
        WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP,
        0,0,0,0, g_hMain, (HMENU)ID_STATUS_BAR, hInst, NULL);
    int parts[] = {400, -1};
    SendMessageW(g_hStatus, SB_SETPARTS, 2, (LPARAM)parts);
    SendMessageW(g_hStatus, SB_SETTEXTW, 1, (LPARAM)L"נגן VIDIOF — גרור קובץ .VIDIOF לכאן");

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; i++) {
        DWORD attr = GetFileAttributesW(argv[i]);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            if (attr & FILE_ATTRIBUTE_DIRECTORY) AddFolderToPlaylist(argv[i]);
            else AddToPlaylist(argv[i]);
        }
    }
    LocalFree(argv);
    if (!g_playlist.empty()) { g_plIndex = 0; PlayCurrent(); }

    ShowWindow(g_hMain, SW_SHOWNORMAL);
    UpdateWindow(g_hMain);
    LayoutControls();

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(g_hMain, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
