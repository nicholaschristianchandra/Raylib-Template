#pragma region Boilerplates

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define NOGDI
    #define NOUSER
    #include <windows.h>
    #include <dbghelp.h>
    #include <signal.h>
    #include <stdlib.h>
    #pragma comment(lib, "user32.lib")
    extern "C" __declspec(dllimport) int __stdcall MessageBoxA(HWND window, const char *text, const char *caption, unsigned int type);
#endif

#define FRAMEWORK_HOST
#define FRAMEWORK_UNIT_DECLARATIONS
#include <Framework/Framework.hpp>
#include <stdlib.h>
#include <new>
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif
#ifndef IS_DEBUG
    #if __has_include(<BakedAssets.hpp>)
        #include <BakedAssets.hpp>
    #else
        extern const unsigned char BakedPack[] = { 0 };
        extern const unsigned int BakedPackSize = 0;
        extern const char BakedPackFile[] = "";
        extern const unsigned int BakedPackHash = 0;
        extern const unsigned long long BakedPackKey = 0;
    #endif

    namespace Program::Lifecycle
    {
        extern "C" bool Startup(Program::State *state);
        extern "C" bool Initialize(Program::State *state);
        extern "C" bool Update(Program::State *state);
        extern "C" void Deinitialize(Program::State *state);
        extern "C" void Shutdown(Program::State *state);
    }
#endif

#if defined(_WIN32) && !defined(_GAMING_XBOX) && !defined(FRAMEWORK_NO_CRASH_REPORTS)

static char CrashFolder[MAX_PATH];
static DWORD CrashThread;
static EXCEPTION_POINTERS *CrashException;
static HANDLE CrashEvent;
static HANDLE CrashWriter;

static DWORD WINAPI WriteCrashReportThread(void *)
{
    typedef BOOL(WINAPI * WriteDumpFunction)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
    HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
    WriteDumpFunction writeDump = dbghelp ? (WriteDumpFunction)(void *)GetProcAddress(dbghelp, "MiniDumpWriteDump") : nullptr;

    WaitForSingleObject(CrashEvent, INFINITE);
    EXCEPTION_POINTERS *exception = CrashException;

    SYSTEMTIME time;
    GetLocalTime(&time);
    char path[MAX_PATH + 64];
    snprintf(path, sizeof(path), "%s\\%s_%02u-%02u-%04u_%02u-%02u-%02u.dmp", CrashFolder, PROGRAM_VERSION, time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);

    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    BOOL written = FALSE;

    if (writeDump && file != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION information = {CrashThread, exception, FALSE};
        written = writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory), &information, NULL, NULL);
    }

    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }

    char message[MAX_PATH + 256];
    snprintf(message, sizeof(message), written ? "%s crashed. A crash report was saved to:\n%s" : "%s crashed.", PROGRAM_NAME, path);
    MessageBoxA(NULL, message, PROGRAM_NAME, 0x50010);

    return 0;
}

static LONG WINAPI WriteCrashReport(EXCEPTION_POINTERS *exception)
{
    CrashThread = GetCurrentThreadId();
    CrashException = exception;
    SetEvent(CrashEvent);
    WaitForSingleObject(CrashWriter, INFINITE);

    return EXCEPTION_EXECUTE_HANDLER;
}

static void __cdecl RaiseCrash(int)
{
    RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

static void __cdecl RaiseCrashOnInvalidParameter(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t)
{
    RaiseCrash(0);
}

static void InstallCrashReports(void)
{
    char appData[MAX_PATH];

    if (GetEnvironmentVariableA("LOCALAPPDATA", appData, sizeof(appData)))
    {
        snprintf(CrashFolder, sizeof(CrashFolder), "%s\\%s", appData, PROGRAM_NAME);
        CreateDirectoryA(CrashFolder, NULL);
        strncat(CrashFolder, "\\Crashes", sizeof(CrashFolder) - strlen(CrashFolder) - 1);
        CreateDirectoryA(CrashFolder, NULL);
        CrashEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        CrashWriter = CreateThread(NULL, 0, WriteCrashReportThread, NULL, 0, NULL);
        SetUnhandledExceptionFilter(WriteCrashReport);
        signal(SIGABRT, RaiseCrash);
        _set_invalid_parameter_handler(RaiseCrashOnInvalidParameter);
        ULONG guarantee = 32 * 1024;
        SetThreadStackGuarantee(&guarantee);
    }
}

#else

static void InstallCrashReports(void)
{
}

#endif

#ifdef IS_DEBUG

typedef bool (*StateFunction)(Program::State *state);
typedef void (*StateProcedure)(Program::State *state);

static StateFunction StartupFunction;
static StateFunction InitializeFunction;
static StateFunction UpdateFunction;
static StateProcedure DeinitializeFunction;
static StateProcedure ShutdownFunction;

static String(1024) DllPath;
static String(1024) CopyPaths[2];
static int LoadedSlot = 1;
static HMODULE Dll;
static FILETIME DllTime;

static bool IsStateCompatible(HMODULE dll)
{
    const unsigned int *size = (const unsigned int *)(void *)GetProcAddress(dll, "ProgramStateSize");
    const unsigned int *signature = (const unsigned int *)(void *)GetProcAddress(dll, "ProgramStateSignature");

    return size && *size == sizeof(Program::State) && signature && *signature == FRAMEWORK_STATE_SIGNATURE;
}

static bool LoadProgram(Program::State *state)
{
    WIN32_FILE_ATTRIBUTE_DATA info;

    if (!GetFileAttributesExA(DllPath, GetFileExInfoStandard, &info) || CompareFileTime(&info.ftLastWriteTime, &DllTime) == 0)
    {
        if (!Dll)
        {
            TraceLog(LOG_ERROR, "Could not find %s. Is it built?", DllPath);
        }

        return Dll != nullptr;
    }

    DllTime = info.ftLastWriteTime;
    const int slot = 1 - LoadedSlot;
    HMODULE next = CopyFileA(DllPath, CopyPaths[slot], FALSE) ? LoadLibraryA(CopyPaths[slot]) : nullptr;

    if (!next || !IsStateCompatible(next))
    {
        if (next)
        {
            TraceLog(LOG_ERROR, "Program.dll was not loaded: a Types file or Utilities.hpp changed since the game started, so the running game's state no longer fits the new code. Close the game and start it again.");
            FreeLibrary(next);
        }
        else
        {
            TraceLog(LOG_ERROR, "Could not load %s (error %lu)", DllPath, GetLastError());
        }

        DeleteFileA(CopyPaths[slot]);

        return Dll != nullptr;
    }

    /* Checked before unloading the old one, which keeps running if this fails */
    const char *const exports[] = {"Startup", "Initialize", "Update", "Deinitialize", "Shutdown"};

    for (const char *name : exports)
    {
        if (!GetProcAddress(next, name))
        {
            TraceLog(LOG_ERROR, "Program.dll was not loaded: it is missing %s.", name);
            FreeLibrary(next);
            DeleteFileA(CopyPaths[slot]);

            return Dll != nullptr;
        }
    }

    const bool reloading = Dll != nullptr;

    if (reloading)
    {
        printf("INFO: Program.dll changed. Reloading...\n");
        DeinitializeFunction(state);
        FreeLibrary(Dll);
        DeleteFileA(CopyPaths[LoadedSlot]);
    }

    Dll = next;
    LoadedSlot = slot;
    StartupFunction = (StateFunction)(void *)GetProcAddress(Dll, "Startup");
    InitializeFunction = (StateFunction)(void *)GetProcAddress(Dll, "Initialize");
    UpdateFunction = (StateFunction)(void *)GetProcAddress(Dll, "Update");
    DeinitializeFunction = (StateProcedure)(void *)GetProcAddress(Dll, "Deinitialize");
    ShutdownFunction = (StateProcedure)(void *)GetProcAddress(Dll, "Shutdown");

    return !reloading || InitializeFunction(state);
}

#else

#define StartupFunction Program::Lifecycle::Startup
#define InitializeFunction Program::Lifecycle::Initialize
#define UpdateFunction Program::Lifecycle::Update
#define DeinitializeFunction Program::Lifecycle::Deinitialize
#define ShutdownFunction Program::Lifecycle::Shutdown

#endif

#ifdef __EMSCRIPTEN__

static Program::State *WebState;

static void WebFrame(void)
{
    if (!UpdateFunction(WebState))
    {
        emscripten_cancel_main_loop();
        DeinitializeFunction(WebState);
        ShutdownFunction(WebState);
    }
}

#endif

#pragma endregion

int main(void)
{
    InstallCrashReports();
    Assets::Initialize();

    Program::State *state = (Program::State *)calloc(1, sizeof(Program::State));

    if (!state)
    {
        return 1;
    }

    new (state) Program::State();

    bool running = true;

#ifdef IS_DEBUG
    const char *base = GetApplicationDirectory();
    snprintf(DllPath, sizeof(DllPath), "%sProgram.dll", base);
    snprintf(CopyPaths[0], sizeof(CopyPaths[0]), "%sProgram_Loaded0.dll", base);
    snprintf(CopyPaths[1], sizeof(CopyPaths[1]), "%sProgram_Loaded1.dll", base);
    running = LoadProgram(state);
#endif

    running = running && StartupFunction(state) && InitializeFunction(state);
    const bool started = running;

#ifdef __EMSCRIPTEN__
    // Browsers draw on their own schedule: the page calls WebFrame once per frame, and main never returns
    WebState = state;
    if (running)
    {
        emscripten_set_main_loop(WebFrame, 0, true);
    }
#endif

    while (running && !WindowShouldClose())
    {
#ifdef IS_DEBUG
        running = LoadProgram(state);
#endif
        running = running && UpdateFunction(state);
    }

#ifdef IS_DEBUG
    if (DeinitializeFunction)
    {
        DeinitializeFunction(state);
        ShutdownFunction(state);
    }

    if (Dll)
    {
        FreeLibrary(Dll);
        DeleteFileA(CopyPaths[LoadedSlot]);
    }
#else
    DeinitializeFunction(state);
    ShutdownFunction(state);
#endif

    state->~State();
    free(state);

    return started ? 0 : 1;
}
