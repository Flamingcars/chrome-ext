#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <string>
#include <Psapi.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <winternl.h>
#include <immintrin.h>
#include <algorithm>
#include <chrono>

#pragma comment(lib, "ntdll.lib")

inline uintptr_t base_address;
// Build string of the client we attached to, e.g. "version-4ea8e85481e34f46".
// Filled in by initialize_target_process() and reported when offsets don't fit.
inline std::string attached_client_build;

typedef NTSTATUS(NTAPI* pNtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtClose)(HANDLE);
typedef NTSTATUS(NTAPI* pNtSetInformationProcess)(HANDLE, ULONG, PVOID, ULONG);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE, PVOID, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE, PVOID, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtCreateSection)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
typedef NTSTATUS(NTAPI* pNtMapViewOfSection)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, ULONG, ULONG, ULONG);

extern "C" NTSTATUS ZwReadVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T Size, PSIZE_T NumberOfBytesRead);
extern "C" NTSTATUS ZwWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T Size, PSIZE_T NumberOfBytesWritten);

typedef struct _PROCESS_BASIC_INFORMATIONZ {
    NTSTATUS ExitStatus;
    PVOID PebBaseAddress;
    ULONG_PTR AffinityMask;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATIONZ, * PPROCESS_BASIC_INFORMATIONZ;

typedef enum _SYSTEM_INFORMATION_CLASSZsadasd {
    SystemHandleInformation = 16
} _SYSTEM_INFORMATION_CLASSZsadasd;

typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG ProcessId;
    UCHAR ObjectTypeNumber;
    UCHAR Flags;
    USHORT Handle;
    PVOID Object;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
    ULONG_PTR NumberOfHandles;
    SYSTEM_HANDLE_INFORMATION Information[1];
} SYSTEM_HANDLE_INFORMATION_EX, * PSYSTEM_HANDLE_INFORMATION_EX;

namespace mem {
    constexpr int CACHE_LINE_SIZE = 64;
    constexpr int MEMORY_POOL_SIZE = 64 * 1024 * 1024;
    constexpr int MAX_CACHED_REGIONS = 8192;
    constexpr int PREFETCH_DISTANCE = 512;

    template<typename T>
    class AlignedDeleter {
    public:
        void operator()(T* ptr) const noexcept {
            if (ptr) {
                ptr->~T();
                _aligned_free(ptr);
            }
        }
    };

    template<typename T>
    using aligned_unique_ptr = std::unique_ptr<T, AlignedDeleter<T>>;

    template<typename T>
    aligned_unique_ptr<T> make_aligned_unique(size_t alignment = CACHE_LINE_SIZE) {
        void* ptr = _aligned_malloc(sizeof(T), alignment);
        if (!ptr) return nullptr;
        return aligned_unique_ptr<T>(new(ptr) T());
    }

    struct AtomicHandleManager {
        std::atomic<HANDLE> primary_handle{ nullptr };
        std::atomic<HANDLE> backup_handle{ nullptr };
        std::atomic<INT32> process_id{ 0 };
        std::atomic<ULONGLONG> last_verification{ 0 };
        std::atomic<bool> handle_verified{ false };
    };

    struct APIFunctionTable {
        pNtWriteVirtualMemory NtWriteVirtualMemory = nullptr;
        pNtReadVirtualMemory NtReadVirtualMemory = nullptr;
        pNtCreateSection NtCreateSection = nullptr;
        pNtMapViewOfSection NtMapViewOfSection = nullptr;
        std::atomic<bool> direct_syscalls_enabled{ false };
        std::atomic<bool> nt_functions_loaded{ false };
    };

    struct alignas(32) MemoryRegion {
        uintptr_t base;
        uintptr_t end;
        DWORD protection;
        bool writable;
        ULONGLONG timestamp;
    };

    class LockFreeMemoryCache {
    private:
        struct CacheEntry {
            std::atomic<uintptr_t> address{ 0 };
            std::atomic<DWORD> protection{ 0 };
            std::atomic<ULONGLONG> access_count{ 0 };
            std::atomic<bool> valid{ false };
        };

        std::array<CacheEntry, MAX_CACHED_REGIONS> cache_entries;
        std::atomic<size_t> entry_count{ 0 };

        size_t hash_address(uintptr_t addr) const noexcept {
            addr ^= addr >> 16;
            addr *= 0x85ebca6b;
            addr ^= addr >> 13;
            addr *= 0xc2b2ae35;
            addr ^= addr >> 16;
            return addr % MAX_CACHED_REGIONS;
        }

    public:
        bool lookup_protection(uintptr_t address, DWORD& protection) noexcept {
            size_t index = hash_address(address & ~0xFFF);
            auto& entry = cache_entries[index];

            if (entry.valid.load(std::memory_order_acquire) &&
                entry.address.load(std::memory_order_relaxed) == (address & ~0xFFF)) {
                protection = entry.protection.load(std::memory_order_relaxed);
                entry.access_count.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            return false;
        }

        void cache_protection(uintptr_t address, DWORD protection) noexcept {
            size_t index = hash_address(address & ~0xFFF);
            auto& entry = cache_entries[index];

            entry.address.store(address & ~0xFFF, std::memory_order_relaxed);
            entry.protection.store(protection, std::memory_order_relaxed);
            entry.access_count.store(1, std::memory_order_relaxed);
            entry.valid.store(true, std::memory_order_release);
        }

        void invalidate() noexcept {
            for (auto& entry : cache_entries) {
                entry.valid.store(false, std::memory_order_release);
            }
        }
    };

    class SIMDMemoryOperations {
    public:
        static void vectorized_copy(void* dest, const void* src, size_t size) noexcept {
            if (size < 32 || !dest || !src) {
                std::memcpy(dest, src, size);
                return;
            }

            const auto* src_ptr = static_cast<const char*>(src);
            auto* dest_ptr = static_cast<char*>(dest);
            size_t vectorized_size = size & ~31;

            for (size_t i = 0; i < vectorized_size; i += 32) {
                __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr + i));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest_ptr + i), data);
            }

            if (size & 31) {
                std::memcpy(dest_ptr + vectorized_size, src_ptr + vectorized_size, size & 31);
            }
        }

        static bool vectorized_compare(const void* ptr1, const void* ptr2, size_t size) noexcept {
            if (size < 32 || !ptr1 || !ptr2) {
                return std::memcmp(ptr1, ptr2, size) == 0;
            }

            const auto* p1 = static_cast<const char*>(ptr1);
            const auto* p2 = static_cast<const char*>(ptr2);
            size_t vectorized_size = size & ~31;

            for (size_t i = 0; i < vectorized_size; i += 32) {
                __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p1 + i));
                __m256i v2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p2 + i));
                __m256i cmp = _mm256_cmpeq_epi8(v1, v2);

                if (_mm256_movemask_epi8(cmp) != 0xFFFFFFFF) {
                    return false;
                }
            }

            if (size & 31) {
                return std::memcmp(p1 + vectorized_size, p2 + vectorized_size, size & 31) == 0;
            }
            return true;
        }
    };

    class HighPerformanceMemoryPool {
    private:
        struct PoolChunk {
            std::atomic<char*> current_ptr;
            std::atomic<size_t> remaining_size;
            char* base_ptr;
            size_t total_size;
        };

        static constexpr size_t CHUNK_COUNT = 16;
        std::array<PoolChunk, CHUNK_COUNT> chunks;
        std::atomic<size_t> current_chunk{ 0 };

    public:
        HighPerformanceMemoryPool() {
            for (auto& chunk : chunks) {
                chunk.base_ptr = static_cast<char*>(VirtualAlloc(nullptr, MEMORY_POOL_SIZE / CHUNK_COUNT,
                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
                chunk.current_ptr.store(chunk.base_ptr, std::memory_order_relaxed);
                chunk.remaining_size.store(MEMORY_POOL_SIZE / CHUNK_COUNT, std::memory_order_relaxed);
                chunk.total_size = MEMORY_POOL_SIZE / CHUNK_COUNT;
            }
        }

        ~HighPerformanceMemoryPool() {
            for (auto& chunk : chunks) {
                if (chunk.base_ptr) {
                    VirtualFree(chunk.base_ptr, 0, MEM_RELEASE);
                }
            }
        }

        void* allocate(size_t size, size_t alignment = sizeof(void*)) noexcept {
            size = (size + alignment - 1) & ~(alignment - 1);

            for (size_t attempts = 0; attempts < CHUNK_COUNT; ++attempts) {
                size_t chunk_idx = (current_chunk.fetch_add(1, std::memory_order_relaxed) + attempts) % CHUNK_COUNT;
                auto& chunk = chunks[chunk_idx];

                char* current = chunk.current_ptr.load(std::memory_order_acquire);
                size_t remaining = chunk.remaining_size.load(std::memory_order_acquire);

                if (remaining >= size) {
                    char* aligned_ptr = reinterpret_cast<char*>((reinterpret_cast<uintptr_t>(current) + alignment - 1) & ~(alignment - 1));
                    size_t aligned_size = size + (aligned_ptr - current);

                    if (remaining >= aligned_size &&
                        chunk.current_ptr.compare_exchange_weak(current, aligned_ptr + size, std::memory_order_release) &&
                        chunk.remaining_size.compare_exchange_weak(remaining, remaining - aligned_size, std::memory_order_release)) {
                        return aligned_ptr;
                    }
                }
            }
            return nullptr;
        }

        void reset() noexcept {
            for (auto& chunk : chunks) {
                chunk.current_ptr.store(chunk.base_ptr, std::memory_order_release);
                chunk.remaining_size.store(chunk.total_size, std::memory_order_release);
            }
        }
    };

    inline AtomicHandleManager handle_manager;
    inline APIFunctionTable api_functions;
    inline LockFreeMemoryCache protection_cache;
    inline HighPerformanceMemoryPool memory_pool;

    inline std::shared_mutex regions_mutex;
    inline std::vector<MemoryRegion> memory_regions;
    inline std::atomic<ULONGLONG> last_region_update{ 0 };

    inline std::mutex handle_mutex;
    struct HandleCandidate {
        DWORD sourcePID;
        HANDLE duplicatedHandle;
        DWORD accessRights;
        bool tested;
        bool functional;
        ULONGLONG score;
    };
    inline std::vector<HandleCandidate> handle_candidates;

    __forceinline void initialize_direct_syscalls() noexcept {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return;

        api_functions.NtWriteVirtualMemory = reinterpret_cast<pNtWriteVirtualMemory>(GetProcAddress(hNtdll, "NtWriteVirtualMemory"));
        api_functions.NtReadVirtualMemory = reinterpret_cast<pNtReadVirtualMemory>(GetProcAddress(hNtdll, "NtReadVirtualMemory"));
        api_functions.NtCreateSection = reinterpret_cast<pNtCreateSection>(GetProcAddress(hNtdll, "NtCreateSection"));
        api_functions.NtMapViewOfSection = reinterpret_cast<pNtMapViewOfSection>(GetProcAddress(hNtdll, "NtMapViewOfSection"));

        api_functions.nt_functions_loaded.store(
            api_functions.NtWriteVirtualMemory && api_functions.NtReadVirtualMemory,
            std::memory_order_release
        );
    }

    __forceinline void prefetch_memory_region(uintptr_t address) noexcept {
        for (size_t i = 0; i < PREFETCH_DISTANCE; i += CACHE_LINE_SIZE) {
            _mm_prefetch(reinterpret_cast<const char*>(address + i), _MM_HINT_T0);
        }
    }

    inline void simd_update_memory_regions() noexcept {
        HANDLE current_handle = handle_manager.primary_handle.load(std::memory_order_acquire);
        if (!current_handle) return;

        ULONGLONG current_time = GetTickCount64();
        if (current_time - last_region_update.load(std::memory_order_relaxed) < 2000) return;

        std::vector<MemoryRegion> new_regions;
        new_regions.reserve(4096);

        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t address = 0;
        size_t region_count = 0;

        while (region_count < 8192 && VirtualQueryEx(current_handle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi)) {
            if (mbi.State == MEM_COMMIT && mbi.RegionSize > 0) {
                MemoryRegion region{
                    .base = reinterpret_cast<uintptr_t>(mbi.BaseAddress),
                    .end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize,
                    .protection = mbi.Protect,
                    .writable = (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0,
                    .timestamp = current_time
                };

                if (region.base < region.end) {
                    new_regions.emplace_back(region);
                    region_count++;
                }
            }

            if (mbi.RegionSize == 0) break;
            uintptr_t next_address = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (next_address <= address) break;
            address = next_address;
        }

        {
            std::unique_lock<std::shared_mutex> lock(regions_mutex);
            memory_regions = std::move(new_regions);
        }

        last_region_update.store(current_time, std::memory_order_release);
    }

    // Cheap guard for anything that walks game objects: an unreadable address would otherwise
    // fault inside the read and take the whole process down, since SEH is not caught by
    // ordinary C++ try/catch.
    inline bool is_readable(uintptr_t address, size_t size = 0x10) noexcept {
        if (!address) return false;

        HANDLE current_handle = handle_manager.primary_handle.load(std::memory_order_acquire);
        if (!current_handle) return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQueryEx(current_handle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) != sizeof(mbi))
            return false;

        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;

        const uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

        return address + size <= region_end;
    }

    __forceinline bool is_address_writable_optimized(uintptr_t address) noexcept {
        DWORD cached_protection;
        if (protection_cache.lookup_protection(address, cached_protection)) {
            return (cached_protection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
        }

        {
            std::shared_lock<std::shared_mutex> lock(regions_mutex);
            for (const auto& region : memory_regions) {
                if (address >= region.base && address < region.end) {
                    protection_cache.cache_protection(address, region.protection);
                    return region.writable;
                }
            }
        }

        HANDLE current_handle = handle_manager.primary_handle.load(std::memory_order_acquire);
        if (!current_handle) return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQueryEx(current_handle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi)) {
            protection_cache.cache_protection(address, mbi.Protect);
            return (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
        }

        return false;
    }

    inline bool enable_debug_privilege() noexcept {
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }

        LUID luid;
        if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid)) {
            CloseHandle(hToken);
            return false;
        }

        TOKEN_PRIVILEGES tp{
            .PrivilegeCount = 1,
            .Privileges = {{.Luid = luid, .Attributes = SE_PRIVILEGE_ENABLED}}
        };

        BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        DWORD error = GetLastError();
        CloseHandle(hToken);

        return result && error != ERROR_NOT_ALL_ASSIGNED;
    }

    inline bool verify_handle_integrity(HANDLE handle, DWORD expected_pid) noexcept {
        if (!handle || handle == INVALID_HANDLE_VALUE) return false;

        ULONGLONG current_time = GetTickCount64();
        ULONGLONG last_check = handle_manager.last_verification.load(std::memory_order_relaxed);

        if (current_time - last_check < 5000 && handle_manager.handle_verified.load(std::memory_order_acquire)) {
            return true;
        }

        DWORD actual_pid = GetProcessId(handle);
        if (actual_pid != expected_pid) return false;

        char test_buffer[16];
        SIZE_T bytes_read = 0;
        bool read_success = false;

        std::array<LPCVOID, 4> test_addresses = {
            reinterpret_cast<LPCVOID>(0x10000),
            reinterpret_cast<LPCVOID>(base_address),
            reinterpret_cast<LPCVOID>(base_address + 0x1000),
            reinterpret_cast<LPCVOID>(0x7FFE0000)
        };

        for (auto addr : test_addresses) {
            if (ReadProcessMemory(handle, addr, test_buffer, sizeof(test_buffer), &bytes_read) && bytes_read > 0) {
                read_success = true;
                break;
            }
        }

        if (read_success) {
            handle_manager.last_verification.store(current_time, std::memory_order_release);
            handle_manager.handle_verified.store(true, std::memory_order_release);
        }

        return read_success;
    }

    inline bool acquire_optimized_handle_enumeration() noexcept {
        std::lock_guard<std::mutex> lock(handle_mutex);

        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return false;

        auto NtQuery = reinterpret_cast<pNtQuerySystemInformation>(GetProcAddress(hNtdll, "NtQuerySystemInformation"));
        if (!NtQuery) return false;

        size_t buffer_size = 2 * 1024 * 1024;
        auto buffer = std::make_unique<char[]>(buffer_size);
        ULONG required_size = 0;
        NTSTATUS status;

        for (int attempts = 0; attempts < 3; ++attempts) {
            status = NtQuery(SystemHandleInformation, buffer.get(), static_cast<ULONG>(buffer_size), &required_size);

            if (status == 0) break;
            if (status == 0xC0000004 && required_size > 0 && required_size < 100 * 1024 * 1024) {
                buffer_size = required_size + 1024 * 1024;
                buffer = std::make_unique<char[]>(buffer_size);
            }
            else {
                return false;
            }
        }

        if (status != 0) return false;

        auto handleInfo = reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(buffer.get());
        if (handleInfo->NumberOfHandles == 0 || handleInfo->NumberOfHandles > 10000000) return false;

        handle_candidates.clear();
        handle_candidates.reserve(64);

        DWORD current_pid = GetCurrentProcessId();
        INT32 target_pid = handle_manager.process_id.load(std::memory_order_acquire);

        constexpr std::array<DWORD, 3> priority_access_levels = {
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            PROCESS_ALL_ACCESS
        };

        for (DWORD required_access : priority_access_levels) {
            for (ULONG i = 0; i < handleInfo->NumberOfHandles && handle_candidates.size() < 32; ++i) {
                auto& handle = handleInfo->Information[i];

                if (handle.ProcessId == 0 || handle.ProcessId == current_pid ||
                    (handle.GrantedAccess & required_access) != required_access) {
                    continue;
                }

                HANDLE source_process = OpenProcess(PROCESS_DUP_HANDLE, FALSE, handle.ProcessId);
                if (!source_process) continue;

                HANDLE duplicated_handle;
                BOOL dup_result = DuplicateHandle(source_process, reinterpret_cast<HANDLE>(handle.Handle),
                    GetCurrentProcess(), &duplicated_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
                CloseHandle(source_process);

                if (!dup_result) continue;

                DWORD target_pid_check = GetProcessId(duplicated_handle);
                if (target_pid_check == static_cast<DWORD>(target_pid)) {
                    HandleCandidate candidate{
                        .sourcePID = handle.ProcessId,
                        .duplicatedHandle = duplicated_handle,
                        .accessRights = handle.GrantedAccess,
                        .tested = false,
                        .functional = false,
                        .score = __popcnt(handle.GrantedAccess)
                    };
                    handle_candidates.emplace_back(candidate);
                }
                else {
                    CloseHandle(duplicated_handle);
                }
            }

            if (!handle_candidates.empty()) break;
        }

        return !handle_candidates.empty();
    }

    inline bool select_optimal_handle() noexcept {
        std::lock_guard<std::mutex> lock(handle_mutex);

        if (handle_candidates.empty()) return false;

        HandleCandidate* best_candidate = nullptr;
        ULONGLONG best_score = 0;
        INT32 target_pid = handle_manager.process_id.load(std::memory_order_acquire);

        for (auto& candidate : handle_candidates) {
            candidate.tested = true;
            candidate.functional = verify_handle_integrity(candidate.duplicatedHandle, target_pid);

            if (candidate.functional && candidate.score > best_score) {
                best_score = candidate.score;
                best_candidate = &candidate;
            }
        }

        if (best_candidate) {
            HANDLE old_handle = handle_manager.primary_handle.exchange(best_candidate->duplicatedHandle, std::memory_order_acq_rel);
            if (old_handle && old_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(old_handle);
            }

            best_candidate->duplicatedHandle = nullptr;

            for (auto& candidate : handle_candidates) {
                if (candidate.duplicatedHandle) {
                    CloseHandle(candidate.duplicatedHandle);
                }
            }
            handle_candidates.clear();
            return true;
        }

        for (auto& candidate : handle_candidates) {
            if (candidate.duplicatedHandle) {
                CloseHandle(candidate.duplicatedHandle);
            }
        }
        handle_candidates.clear();
        return false;
    }

    inline bool acquire_primary_handle() noexcept {
        enable_debug_privilege();
        initialize_direct_syscalls();

        constexpr std::array<DWORD, 4> access_levels = {
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE,
            PROCESS_ALL_ACCESS
        };

        INT32 target_pid = handle_manager.process_id.load(std::memory_order_acquire);

        for (DWORD access : access_levels) {
            HANDLE direct_handle = OpenProcess(access, FALSE, target_pid);
            if (direct_handle && direct_handle != INVALID_HANDLE_VALUE) {
                if (verify_handle_integrity(direct_handle, target_pid)) {
                    handle_manager.primary_handle.store(direct_handle, std::memory_order_release);
                    simd_update_memory_regions();
                    return true;
                }
                CloseHandle(direct_handle);
            }
        }

        if (acquire_optimized_handle_enumeration()) {
            if (select_optimal_handle()) {
                simd_update_memory_regions();
                return true;
            }
        }

        return false;
    }

    // Every executable name a Roblox client runs under. Studio is deliberately
    // absent: it is a different product with a different object layout.
    inline constexpr const char* k_roblox_client_names[] = {
        "RobloxPlayerBeta.exe",   // normal web client, also what Bloxstrap launches
        "Windows10Universal.exe", // Microsoft Store (UWP) build
        "RobloxPlayer.exe",       // older and some third-party launchers
    };

    inline bool is_roblox_client_name(const char* module_name) noexcept {
        if (!module_name || !*module_name) return false;

        for (const char* candidate : k_roblox_client_names) {
            if (lstrcmpiA(module_name, candidate) == 0) return true;
        }
        return false;
    }

    inline bool process_has_visible_window(DWORD pid) noexcept {
        struct WindowSearch {
            DWORD pid;
            bool found;
        } search{ pid, false };

        EnumWindows([](HWND window, LPARAM param) -> BOOL {
            auto* state = reinterpret_cast<WindowSearch*>(param);

            DWORD window_pid = 0;
            GetWindowThreadProcessId(window, &window_pid);

            if (window_pid != state->pid || !IsWindowVisible(window))
                return TRUE;

            state->found = true;
            return FALSE;
        }, reinterpret_cast<LPARAM>(&search));

        return search.found;
    }

    // Finds the client by process name instead of window title, so it works however
    // Roblox was started and whatever its window happens to be called.
    inline DWORD find_roblox_process() noexcept {
        DWORD best_pid = 0;
        bool best_has_window = false;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 process_entry{};
            process_entry.dwSize = sizeof(process_entry);

            if (Process32First(snapshot, &process_entry)) {
                do {
                    if (!is_roblox_client_name(process_entry.szExeFile))
                        continue;

                    // Several clients can be open at once; the one with a window is
                    // the one the user is actually playing on.
                    const bool has_window = process_has_visible_window(process_entry.th32ProcessID);
                    if (!best_pid || (has_window && !best_has_window)) {
                        best_pid = process_entry.th32ProcessID;
                        best_has_window = has_window;
                    }
                } while (Process32Next(snapshot, &process_entry));
            }

            CloseHandle(snapshot);
        }

        // Last resort for a client whose image name is not in the list yet.
        if (!best_pid) {
            if (HWND window = FindWindowA(nullptr, "Roblox")) {
                DWORD window_pid = 0;
                GetWindowThreadProcessId(window, &window_pid);
                best_pid = window_pid;
            }
        }

        return best_pid;
    }

    // Describes the running client for diagnostics: Roblox installs each build in
    // its own "version-<hash>" folder, and the Store build has no such folder.
    inline std::string describe_client_build(DWORD pid) noexcept {
        if (!pid) return std::string();

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) return std::string();

        char image_path[MAX_PATH * 2] = {};
        DWORD size = static_cast<DWORD>(sizeof(image_path));
        const bool ok = QueryFullProcessImageNameA(process, 0, image_path, &size) != FALSE;
        CloseHandle(process);

        if (!ok) return std::string();

        const std::string path(image_path);

        const size_t start = path.find("version-");
        if (start != std::string::npos) {
            size_t end = start;
            while (end < path.size() && path[end] != '\\' && path[end] != '/')
                ++end;
            return path.substr(start, end - start);
        }

        if (path.find("WindowsApps") != std::string::npos)
            return "Microsoft Store build";

        return std::string();
    }

    // Base of the client image. The first module of a process is its own image, so
    // this works whatever the executable is named.
    inline uintptr_t main_module_base(DWORD pid) noexcept {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32 module_entry{};
        module_entry.dwSize = sizeof(module_entry);

        uintptr_t base = 0;

        if (Module32First(snapshot, &module_entry)) {
            base = reinterpret_cast<uintptr_t>(module_entry.modBaseAddr);

            do {
                if (is_roblox_client_name(module_entry.szModule)) {
                    base = reinterpret_cast<uintptr_t>(module_entry.modBaseAddr);
                    break;
                }
            } while (Module32Next(snapshot, &module_entry));
        }

        CloseHandle(snapshot);

        return base;
    }

    inline void initialize_target_process() noexcept {
        const DWORD pid = find_roblox_process();
        if (!pid) return;

        const uintptr_t base = main_module_base(pid);
        if (!base) return;

        attached_client_build = describe_client_build(pid);

        handle_manager.process_id.store(static_cast<INT32>(pid), std::memory_order_release);
        base_address = base;
    }

    template<typename T>
    __forceinline bool secure_bounds_check(uintptr_t address, size_t size) noexcept {
        return address != 0 &&
            size <= 0x100000 &&
            address < 0x7FFFFFFFFFFF &&
            (address + size) > address;
    }

    inline void cleanup_resources() noexcept {
        try {
            {
                std::lock_guard<std::mutex> lock(handle_mutex);
                for (auto& candidate : handle_candidates) {
                    if (candidate.duplicatedHandle && candidate.duplicatedHandle != INVALID_HANDLE_VALUE) {
                        CloseHandle(candidate.duplicatedHandle);
                    }
                }
                handle_candidates.clear();
            }

            {
                std::unique_lock<std::shared_mutex> lock(regions_mutex);
                memory_regions.clear();
            }

            protection_cache.invalidate();

            HANDLE primary = handle_manager.primary_handle.exchange(nullptr, std::memory_order_acq_rel);
            if (primary && primary != INVALID_HANDLE_VALUE) {
                CloseHandle(primary);
            }

            HANDLE backup = handle_manager.backup_handle.exchange(nullptr, std::memory_order_acq_rel);
            if (backup && backup != INVALID_HANDLE_VALUE) {
                CloseHandle(backup);
            }

            handle_manager.process_id.store(0, std::memory_order_release);
            handle_manager.handle_verified.store(false, std::memory_order_release);
            attached_client_build.clear();
            last_region_update.store(0, std::memory_order_release);
            base_address = 0;

            memory_pool.reset();
        }
        catch (...) {
        }
    }

    inline bool is_system_ready() noexcept {
        return handle_manager.primary_handle.load(std::memory_order_acquire) != nullptr &&
            handle_manager.process_id.load(std::memory_order_acquire) != 0;
    }
}

inline bool is_valid_address(uintptr_t address) noexcept {
    return address != 0 && address < 0x7FFFFFFFFFFF;
}

template <typename T>
T read(uint64_t address) noexcept {
    T buffer{};

   /* if (!mem::secure_bounds_check<T>(address, sizeof(T))) {
        return buffer;
    }*/

    HANDLE current_handle = mem::handle_manager.primary_handle.load(std::memory_order_acquire);
    if (!current_handle) return buffer;

    mem::prefetch_memory_region(address);

    if (mem::api_functions.nt_functions_loaded.load(std::memory_order_acquire) &&
        mem::api_functions.NtReadVirtualMemory) {
        ULONG nt_bytes_read = 0;
        NTSTATUS status = mem::api_functions.NtReadVirtualMemory(current_handle,
            reinterpret_cast<PVOID>(address),
            &buffer, sizeof(T), &nt_bytes_read);
        if (status == 0 && nt_bytes_read == sizeof(T)) {
            return buffer;
        }
    }

    if (mem::api_functions.direct_syscalls_enabled.load(std::memory_order_acquire)) {
        SIZE_T bytes_read = 0;
        NTSTATUS status = ZwReadVirtualMemory(current_handle, reinterpret_cast<PVOID>(address),
            &buffer, sizeof(T), &bytes_read);
        if (status == 0 && bytes_read == sizeof(T)) {
            return buffer;
        }
    }

    SIZE_T bytes_read = 0;
    ReadProcessMemory(current_handle, reinterpret_cast<PVOID>(address), &buffer, sizeof(T), &bytes_read);
    return buffer;
}

template <typename T>
bool write(uint64_t address, const T& buffer) noexcept {
    if (!mem::secure_bounds_check<T>(address, sizeof(T))) {
        return false;
    }

    HANDLE current_handle = mem::handle_manager.primary_handle.load(std::memory_order_acquire);
    if (!current_handle) return false;

    mem::simd_update_memory_regions();

    bool needs_protection_change = !mem::is_address_writable_optimized(address);
    DWORD old_protect = 0;

    if (needs_protection_change) {
        if (!VirtualProtectEx(current_handle, reinterpret_cast<LPVOID>(address),
            sizeof(T), PAGE_EXECUTE_READWRITE, &old_protect)) {
            return false;
        }
    }

    bool success = false;

    if (mem::api_functions.nt_functions_loaded.load(std::memory_order_acquire) &&
        mem::api_functions.NtWriteVirtualMemory) {
        ULONG bytes_written = 0;
        NTSTATUS status = mem::api_functions.NtWriteVirtualMemory(current_handle,
            reinterpret_cast<PVOID>(address),
            const_cast<T*>(&buffer),
            sizeof(T), &bytes_written);
        success = (status == 0 && bytes_written == sizeof(T));
    }

    if (!success && mem::api_functions.direct_syscalls_enabled.load(std::memory_order_acquire)) {
        SIZE_T bytes_written = 0;
        NTSTATUS status = ZwWriteVirtualMemory(current_handle, reinterpret_cast<PVOID>(address),
            const_cast<T*>(&buffer), sizeof(T), &bytes_written);
        success = (status == 0 && bytes_written == sizeof(T));
    }

    if (!success) {
        SIZE_T bytes_written = 0;
        success = WriteProcessMemory(current_handle, reinterpret_cast<PVOID>(address),
            &buffer, sizeof(T), &bytes_written) != FALSE;
    }

    if (needs_protection_change && old_protect != 0) {
        DWORD temp = 0;
        VirtualProtectEx(current_handle, reinterpret_cast<LPVOID>(address), sizeof(T), old_protect, &temp);
    }

    return success;
}

template <typename T>
bool write_fast(uint64_t address, const T& buffer) noexcept {
    if (!mem::secure_bounds_check<T>(address, sizeof(T))) {
        return false;
    }

    HANDLE current_handle = mem::handle_manager.primary_handle.load(std::memory_order_acquire);
    if (!current_handle) return false;

    if (mem::is_address_writable_optimized(address)) {
        if (mem::api_functions.nt_functions_loaded.load(std::memory_order_acquire) &&
            mem::api_functions.NtWriteVirtualMemory) {
            ULONG bytes_written = 0;
            NTSTATUS status = mem::api_functions.NtWriteVirtualMemory(current_handle,
                reinterpret_cast<PVOID>(address),
                const_cast<T*>(&buffer),
                sizeof(T), &bytes_written);
            if (status == 0 && bytes_written == sizeof(T)) {
                return true;
            }
        }

        if (mem::api_functions.direct_syscalls_enabled.load(std::memory_order_acquire)) {
            SIZE_T bytes_written = 0;
            NTSTATUS status = ZwWriteVirtualMemory(current_handle, reinterpret_cast<PVOID>(address),
                const_cast<T*>(&buffer), sizeof(T), &bytes_written);
            if (status == 0 && bytes_written == sizeof(T)) {
                return true;
            }
        }

        SIZE_T bytes_written = 0;
        return WriteProcessMemory(current_handle, reinterpret_cast<PVOID>(address),
            &buffer, sizeof(T), &bytes_written) != FALSE;
    }

    return write(address, buffer);
}

template <typename T>
bool write_array(uint64_t address, const T* buffer, size_t count) noexcept {
    if (!buffer || count == 0 || !mem::secure_bounds_check<T>(address, sizeof(T) * count)) {
        return false;
    }

    HANDLE current_handle = mem::handle_manager.primary_handle.load(std::memory_order_acquire);
    if (!current_handle) return false;

    size_t total_size = sizeof(T) * count;
    void* temp_buffer = mem::memory_pool.allocate(total_size, 32);
    if (!temp_buffer) {
        temp_buffer = _aligned_malloc(total_size, 32);
        if (!temp_buffer) return false;
    }

    mem::SIMDMemoryOperations::vectorized_copy(temp_buffer, buffer, total_size);

    mem::simd_update_memory_regions();

    bool needs_protection_change = !mem::is_address_writable_optimized(address);
    DWORD old_protect = 0;

    if (needs_protection_change) {
        if (!VirtualProtectEx(current_handle, reinterpret_cast<LPVOID>(address),
            total_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
            if (temp_buffer != mem::memory_pool.allocate(0, 1)) {
                _aligned_free(temp_buffer);
            }
            return false;
        }
    }

    bool success = false;

    if (mem::api_functions.nt_functions_loaded.load(std::memory_order_acquire) &&
        mem::api_functions.NtWriteVirtualMemory) {
        ULONG bytes_written = 0;
        NTSTATUS status = mem::api_functions.NtWriteVirtualMemory(current_handle,
            reinterpret_cast<PVOID>(address),
            temp_buffer,
            static_cast<ULONG>(total_size),
            &bytes_written);
        success = (status == 0 && bytes_written == total_size);
    }

    if (!success) {
        SIZE_T bytes_written = 0;
        success = WriteProcessMemory(current_handle, reinterpret_cast<PVOID>(address),
            temp_buffer, total_size, &bytes_written) != FALSE;
    }

    if (needs_protection_change && old_protect != 0) {
        DWORD temp_protect = 0;
        VirtualProtectEx(current_handle, reinterpret_cast<LPVOID>(address), total_size, old_protect, &temp_protect);
    }

    if (temp_buffer != mem::memory_pool.allocate(0, 1)) {
        _aligned_free(temp_buffer);
    }

    return success;
}

class MemoryManagerRAII {
private:
    bool initialized;
    std::atomic<bool> system_ready{ false };

public:
    MemoryManagerRAII() noexcept : initialized(false) {}

    bool initialize() noexcept {
        if (initialized) return true;

        mem::initialize_target_process();
        if (mem::acquire_primary_handle()) {
            initialized = true;
            return true;
        }
        return false;
    }

    ~MemoryManagerRAII() noexcept {
        if (initialized) {
            mem::cleanup_resources();
            initialized = false;
        }
    }

    MemoryManagerRAII(const MemoryManagerRAII&) = delete;
    MemoryManagerRAII& operator=(const MemoryManagerRAII&) = delete;

    MemoryManagerRAII(MemoryManagerRAII&& other) noexcept
        : initialized(std::exchange(other.initialized, false)) {
    }

    MemoryManagerRAII& operator=(MemoryManagerRAII&& other) noexcept {
        if (this != &other) {
            if (initialized) {
                mem::cleanup_resources();
            }
            initialized = std::exchange(other.initialized, false);
        }
        return *this;
    }

    bool is_ready() const noexcept {
        return initialized && mem::is_system_ready();
    }

    void manual_shutdown() noexcept {
        if (initialized) {
            mem::cleanup_resources();
            initialized = false;
        }
    }
};