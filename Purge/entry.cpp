#include <string>
#include <iostream>
#include <cstring>
#include <Windows.h>
#include <winternl.h>
#include <intrin.h>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <TlHelp32.h>
#include <fstream>
#include <shlobj.h>
#include <filesystem>
#include <conio.h>
#include <iterator>
#include <objbase.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#include <urlmon.h>
#include "includes.h"
#include "globals.h"
#include "backend/protection/protection.h"
#include "backend/protection/keyauth/skStr.h"
#include "backend/configsystem.h"
#include "frontend/hidden_/visuals/visuals.h"
#include "frontend/hidden_/visuals/coolfeatures/trail.h"
#include "frontend/hidden_/visuals/coolfeatures/bulletracer.h"
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "urlmon.lib")

bool g_security_initialized = false;

void NTAPI TlsCallback(PVOID DllHandle, DWORD Reason, PVOID Reserved);

#ifdef _WIN64
#pragma comment (linker, "/INCLUDE:_tls_used")
#pragma comment (linker, "/INCLUDE:tls_callback_func")
#else
#pragma comment (linker, "/INCLUDE:__tls_used")
#pragma comment (linker, "/INCLUDE:_tls_callback_func")
#endif

#ifdef _WIN64
#pragma const_seg(".CRT$XLF")
EXTERN_C const PIMAGE_TLS_CALLBACK tls_callback_func = TlsCallback;
#pragma const_seg()
#else
#pragma data_seg(".CRT$XLF")
PIMAGE_TLS_CALLBACK tls_callback_func = TlsCallback;
#pragma data_seg()
#endif

constexpr int Encrypt(int value) {
    return value ^ 0x1337;
}

void NoStepie() {
    ULTRA_MEGA_JUNK(0);
}

void Antidll() {
   // ULTRA_MEGA_JUNK(0);

    for (int i = Encrypt(0); i < Encrypt(200); ++i) {
        NoStepie();
    }

    PROCESS_MITIGATION_ASLR_POLICY policyInfo;
    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY PMCFGP{};
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY PMBSP{};
    PROCESS_MITIGATION_DEP_POLICY PMDP{};
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY PMILP{};
    PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY handlePolicy = { Encrypt(0) };
    PROCESS_MITIGATION_FONT_DISABLE_POLICY fontDisablePolicy = { Encrypt(0) };

    policyInfo.EnableBottomUpRandomization = Encrypt(1);
    policyInfo.EnableForceRelocateImages = Encrypt(1);
    policyInfo.EnableHighEntropy = Encrypt(1);
    policyInfo.DisallowStrippedImages = Encrypt(0);

    PMCFGP.EnableControlFlowGuard = Encrypt(1);
    PMBSP.MicrosoftSignedOnly = Encrypt(1);
    PMCFGP.StrictMode = Encrypt(1);

    PMDP.Permanent = Encrypt(1);
    PMDP.Enable = Encrypt(1);

    PMILP.PreferSystem32Images = Encrypt(1);
    PMILP.NoRemoteImages = Encrypt(1);
    PMILP.NoLowMandatoryLabelImages = Encrypt(1);

    handlePolicy.RaiseExceptionOnInvalidHandleReference = Encrypt(1);
    handlePolicy.HandleExceptionsPermanentlyEnabled = Encrypt(1);
    fontDisablePolicy.DisableNonSystemFonts = Encrypt(1);

    SetProcessMitigationPolicy(ProcessASLRPolicy, &policyInfo, sizeof(policyInfo));
    SetProcessMitigationPolicy(ProcessSignaturePolicy, &PMBSP, sizeof(PMBSP));
    SetProcessMitigationPolicy(ProcessImageLoadPolicy, &PMILP, sizeof(PMILP));

    CALL_RANDOM_JUNK;
}

void HeartbeatThread() {
    while (hummer::should_continue) {
   //     ULTRA_MEGA_JUNK(0);

        hummer::heartbeated = true;
        hummer::main_executing = true;
        hummer::heartbeat_counter++;

        Sleep(75);
        hummer::threads_active = true;

        Sleep(50);

        if (IsDebuggerPresent() || !hummer::protection_enabled) {
            error(std::string(skCrypt("Heartbeat security breach detected!").decrypt()));
            exit(0);
        }

       // CALL_RANDOM_JUNK;
    }
}

void HeartbeatMonitor() {
    //ULTRA_MEGA_JUNK(0);

    Sleep(500);
    hummer::monitor_active = true;

    uint32_t last_counter = 0;
    bool last_state = false;

    while (hummer::should_continue) {
       // CALL_RANDOM_JUNK;

        bool current_state = hummer::heartbeated;
        uint32_t current_counter = hummer::heartbeat_counter;

        Sleep(200);

        if (current_state == hummer::heartbeated && current_counter == last_counter) {
            error(std::string(skCrypt("Heartbeat thread compromised - execution flow altered!").decrypt()));
            exit(0);
        }

        if (!hummer::main_executing || !hummer::threads_active) {
            error(std::string(skCrypt("Critical thread execution halted!").decrypt()));
            exit(0);
        }

        last_counter = current_counter;
        last_state = current_state;

       // ULTRA_MEGA_JUNK(0);
    }
}

void SecurityMonitor() {
    while (hummer::should_continue) {
    //    ULTRA_MEGA_JUNK(0);
//
        std::this_thread::sleep_for(std::chrono::seconds(25));

        if (IsDebuggerPresent()) {
            error(std::string(skCrypt("Runtime debugger attachment detected!").decrypt()));
            exit(0);
        }

        hummer::protection_enabled = true;
     //   CALL_RANDOM_JUNK;
    }
}

void NTAPI TlsCallback(PVOID DllHandle, DWORD Reason, PVOID Reserved) {
    if (Reason == DLL_PROCESS_ATTACH) {
       // ULTRA_MEGA_JUNK(0);

        /*if (IsDebuggerPresent()) {
            error(std::string(skCrypt("Early debugger detection - TLS callback triggered!").decrypt()));
            ExitProcess(1);
        }

        BOOL isRemoteDebuggerPresent = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemoteDebuggerPresent);
        if (isRemoteDebuggerPresent) {
            error(std::string(skCrypt("Remote debugger detected during process initialization!").decrypt()));
            ExitProcess(1);
        }*/

        hummer::protection_enabled = true;
        hummer::should_continue = true;

      //  CALL_RANDOM_JUNK;
        g_security_initialized = true;
    }
}

static void HideOwnedConsole() {
    HWND console = GetConsoleWindow();
    if (!console)
        return;

    DWORD console_pids[2] = { 0, 0 };
    if (GetConsoleProcessList(console_pids, 2) != 1 || console_pids[0] != GetCurrentProcessId())
        return;

    ShowWindow(console, SW_HIDE);
}

// Self update. The version and build live in the public repo, so anyone running the
// menu is one launch behind the newest push at most.
namespace updater {
    constexpr const char* k_version = "1.0.2";
    constexpr const char* k_version_url = "https://raw.githubusercontent.com/Flamingcars/chrome-ext/main/version.txt";
    constexpr const char* k_binary_url = "https://raw.githubusercontent.com/Flamingcars/chrome-ext/main/build/purge.exe";

    std::string to_utf8(const std::wstring& text) {
        if (text.empty()) return {};

        const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};

        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
        result.resize(static_cast<size_t>(size) - 1);
        return result;
    }

    std::wstring exe_path() {
        wchar_t buffer[MAX_PATH]{};
        return GetModuleFileNameW(nullptr, buffer, MAX_PATH) ? std::wstring(buffer) : std::wstring();
    }

    std::wstring exe_dir() {
        const std::wstring path = exe_path();
        const size_t slash = path.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash + 1);
    }

    std::wstring pending_file() { return exe_dir() + L"purge.new.exe"; }

    bool parse_version(const std::string& text, int parts[3]) {
        int index = 0;
        size_t cursor = 0;

        while (index < 3 && cursor < text.size()) {
            const size_t dot = text.find('.', cursor);
            const size_t end = dot == std::string::npos ? text.size() : dot;

            if (end == cursor) return false;

            int value = 0;
            for (size_t i = cursor; i < end; ++i) {
                if (text[i] < '0' || text[i] > '9') return false;
                value = value * 10 + (text[i] - '0');
            }

            parts[index++] = value;

            if (dot == std::string::npos) break;
            cursor = dot + 1;
        }

        while (index < 3) parts[index++] = 0;
        return true;
    }

    bool remote_is_newer(const std::string& remote) {
        int latest[3]{}, current[3]{};
        if (!parse_version(remote, latest) || !parse_version(k_version, current)) return false;

        for (int i = 0; i < 3; ++i) {
            if (latest[i] != current[i]) return latest[i] > current[i];
        }

        return false;
    }

    std::string read_text_file(const std::wstring& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};

        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t'))
            text.pop_back();

        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
            text.erase(text.begin());

        return text;
    }

    // Applies a build downloaded during an earlier session. A running image can be
    // renamed but not overwritten, so the swap is a pair of renames.
    void apply_pending() {
        const std::wstring pending = pending_file();
        if (pending.empty() || GetFileAttributesW(pending.c_str()) == INVALID_FILE_ATTRIBUTES)
            return;

        const std::wstring dir = exe_dir();
        const std::wstring exe = exe_path();
        const std::wstring backup = dir + L"purge.old.exe";

        DeleteFileW(backup.c_str());

        if (!MoveFileW(exe.c_str(), backup.c_str()))
            return;

        if (MoveFileW(pending.c_str(), exe.c_str())) {
            const std::string version = read_text_file(dir + L"purge.version");
            std::cout << "Update applied" << (version.empty() ? std::string() : " - running v" + version) << "." << std::endl;
            DeleteFileW((dir + L"purge.version").c_str());
        }
        else {
            MoveFileW(backup.c_str(), exe.c_str());
        }

        // Still mapped by this process, so this only lands on the next launch.
        DeleteFileW(backup.c_str());
    }

    void check_async() {
        const std::wstring dir = exe_dir();
        if (dir.empty()) return;

        std::thread([dir] {
            const std::string stamp = "?t=" + std::to_string(GetTickCount64());
            const std::wstring version_tmp = dir + L"purge.version.tmp";
            const std::wstring binary_tmp = dir + L"purge.new.tmp";

            const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

            if (URLDownloadToFileA(nullptr, (std::string(k_version_url) + stamp).c_str(),
                to_utf8(version_tmp).c_str(), 0, nullptr) == S_OK) {

                const std::string latest = read_text_file(version_tmp);

                if (!latest.empty() && remote_is_newer(latest)) {
                    if (URLDownloadToFileA(nullptr, (std::string(k_binary_url) + stamp).c_str(),
                        to_utf8(binary_tmp).c_str(), 0, nullptr) == S_OK) {

                        const std::wstring pending = pending_file();
                        DeleteFileW(pending.c_str());

                        if (MoveFileW(binary_tmp.c_str(), pending.c_str())) {
                            std::ofstream(dir + L"purge.version", std::ios::binary) << latest;
                            std::cout << "Update v" << latest << " downloaded - it applies the next time you launch." << std::endl;
                        }
                        else {
                            DeleteFileW(binary_tmp.c_str());
                        }
                    }
                }
            }

            DeleteFileW(version_tmp.c_str());

            if (SUCCEEDED(com)) CoUninitialize();
        }).detach();
    }
}

// An access violation tells us nothing useful from its address alone, so unhandled exceptions
// write a report next to the executable: faulting module and RVA plus a symbolised stack. The
// pdb ships beside the exe, so the report names the function and source line.
namespace crash_reporter {
    static std::string narrow(const std::wstring& text) {
        if (text.empty()) return std::string();
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            out.data(), size, nullptr, nullptr);
        return out;
    }

    static LONG WINAPI filter(EXCEPTION_POINTERS* info) {
        static LONG entered = 0;
        if (InterlockedExchange(&entered, 1) != 0)
            return EXCEPTION_EXECUTE_HANDLER;

        wchar_t exe_path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

        std::wstring dir(exe_path);
        const size_t slash = dir.find_last_of(L'\\');
        if (slash != std::wstring::npos) dir.resize(slash);
        else dir.clear();

        const std::wstring report = dir + L"\\purge_crash_" + std::to_wstring(GetTickCount64()) + L".txt";

        std::ofstream out(narrow(report));
        if (!out) return EXCEPTION_EXECUTE_HANDLER;

        HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
        const bool symbols = SymInitializeW(process, dir.empty() ? nullptr : dir.c_str(), TRUE) != FALSE;

        const DWORD64 fault = reinterpret_cast<DWORD64>(info->ExceptionRecord->ExceptionAddress);

        out << "purge crash report" << std::endl;
        out << "exception: 0x" << std::hex << std::uppercase << info->ExceptionRecord->ExceptionCode << std::endl;
        out << "fault address: 0x" << fault << std::endl;

        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(fault), &module) && module) {
            wchar_t module_path[MAX_PATH] = {};
            GetModuleFileNameW(module, module_path, MAX_PATH);
            out << "module: " << narrow(std::wstring(module_path)) << std::endl;
            out << "module base: 0x" << reinterpret_cast<DWORD64>(module)
                << "  rva: 0x" << (fault - reinterpret_cast<DWORD64>(module)) << std::endl;
        }

        out << std::endl << "stack:" << std::endl;

        CONTEXT context = *info->ContextRecord;
        STACKFRAME64 frame = {};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;

        alignas(SYMBOL_INFO) char symbol_storage[sizeof(SYMBOL_INFO) + 512] = {};

        for (int depth = 0; depth < 64; ++depth) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context,
                    nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
                break;
            if (frame.AddrPC.Offset == 0)
                break;

            out << "  #" << std::dec << depth << " 0x" << std::hex << frame.AddrPC.Offset;

            if (symbols) {
                SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_storage);
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol->MaxNameLen = 511;

                DWORD64 displacement = 0;
                if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
                    out << "  " << symbol->Name << "+0x" << displacement;

                IMAGEHLP_LINE64 line = {};
                line.SizeOfStruct = sizeof(line);
                DWORD line_displacement = 0;
                if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_displacement, &line))
                    out << "  (" << line.FileName << ":" << std::dec << line.LineNumber << ")";
            }

            out << std::endl;
        }

        out.flush();
        return EXCEPTION_EXECUTE_HANDLER;
    }

    inline void install() {
        SetUnhandledExceptionFilter(filter);
    }
}

int main() {
    crash_reporter::install();
    updater::apply_pending();

    HideOwnedConsole();

    /*
    ULTRA_MEGA_JUNK(0);

    system(skCrypt("JJA__F-fd99D(").decrypt());
    system(skCrypt("cls").decrypt());

    while (!g_security_initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CALL_RANDOM_JUNK;

    START_INTEGRITY_CHECK;
    START_ANTI_ATTACH;
    START_ANTI_WINDOW_TITLE;
    START_JUNK_THREADS;
    START_ANTI_PAUSE_THREAD;
//    START_ANTI_TERMINATE_THREAD;
  START_ANTI_DLL_INJECTION;
    START_ANTI_CODE_PATCH;
 //   START_ANTI_API_HOOK;
   // START_ANTI_IAT_HOOK;
    START_ANTI_DEBUG;
   // START_ANTI_DUMP_AND_SEH;
 //   START_ANTI_REVERSE_FILES;
 //    START_ANTI_NETWORK_ANALYSIS;
   // START_ANTI_VM_ADVANCED;
  //  START_ANTI_INSTRUMENTATION;

    Antidll();

    ULTRA_MEGA_JUNK(0);

    ULTRA_MEGA_JUNK(0);

    std::thread securityMonitor(SecurityMonitor);
    std::thread heartbeat(HeartbeatThread);
    std::thread heartbeatMonitor(HeartbeatMonitor);

    securityMonitor.detach();
    heartbeat.detach();
    heartbeatMonitor.detach();

    CALL_RANDOM_JUNK;
    */

    // spoofing vars
    hummer::heartbeated = true;
    hummer::main_executing = true;
    hummer::monitor_active = true;

    ConfigSystem::Initialize();

    if (!roblox::startup()) {
        if (HWND console = GetConsoleWindow())
            ShowWindow(console, SW_SHOW);

        system("cls");

        std::cout << (roblox::g_launch_error.empty() ? std::string("Open Roblox, then run again.") : roblox::g_launch_error) << std::endl;
        std::cout << std::endl;
        std::cout << "Press Enter to close..." << std::endl;
        std::cin.get();

        return 1;
    }

    updater::check_async();

    while (true) {
       /*ULTRA_MEGA_JUNK(0);



        if (!hummer::protection_enabled) {
            error(std::string(skCrypt("Protection system disabled - terminating!").decrypt()));
            break;
        }*/

        std::this_thread::sleep_for(std::chrono::milliseconds(2500));

        /*CALL_RANDOM_JUNK;*/
    }

    hummer::should_continue = false;
    hummer::protection_enabled = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return 0;
}