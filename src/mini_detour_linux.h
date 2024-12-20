#ifndef MINI_DETOUR_LINUX_H
#define MINI_DETOUR_LINUX_H

#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

namespace MemoryManipulation {
#if defined(MINIDETOUR_ARCH_X64) || defined(MINIDETOUR_ARCH_ARM64)
    const void* max_user_address = reinterpret_cast<void*>(0x7ffefffff000);
#elif defined(MINIDETOUR_ARCH_X86) || defined(MINIDETOUR_ARCH_ARM)
    const void* max_user_address = reinterpret_cast<void*>(0x7ffff000);
#endif

    int memory_protect_rights_to_native(memory_rights rights)
    {
        switch (rights)
        {
            case mem_r  : return PROT_READ;
            case mem_w  : return PROT_WRITE;
            case mem_x  : return PROT_EXEC;
            case mem_rw : return PROT_WRITE | PROT_READ;
            case mem_rx : return PROT_READ  | PROT_EXEC;
            case mem_wx : return PROT_WRITE | PROT_EXEC;
            case mem_rwx: return PROT_WRITE | PROT_READ | PROT_EXEC;

            default: return PROT_NONE;
        }
    }

    size_t PageSize()
    {
        return sysconf(_SC_PAGESIZE);
    }

    region_infos_t GetRegionInfos(void* address)
    {
        region_infos_t res{};

        char* str_it;
        const char* str_end;

        uintptr_t target = (uintptr_t)address;
        uintptr_t start;
        uintptr_t end;
        std::ifstream f("/proc/self/maps");
        std::string s;
        unsigned int rights = mem_unset;

        while (std::getline(f, s))
        {
            if (!s.empty())
            {
                str_it = &s[0];
                str_end = s.data() + s.length();

                start = (uintptr_t)strtoul(str_it, &str_it, 16);
                end = (uintptr_t)strtoul(str_it + 1, &str_it, 16);
                if (start != 0 && end != 0 && start <= target && target < end) {
                    res.start = start;
                    res.end = end;

                    rights = mem_none;

                    ++str_it;
                    if (str_it[0] == 'r')
                        rights |= mem_r;

                    if (str_it[1] == 'w')
                        rights |= mem_w;

                    if (str_it[2] == 'x')
                        rights |= mem_x;

                    for (int i = 0; i < 4; ++i)
                    {
                        while (*str_it != ' ' && str_it < str_end)
                        {
                            ++str_it;
                        }
                        while (*str_it == ' ' && str_it < str_end)
                        {
                            ++str_it;
                        }
                    }

                    res.module_name = str_it;

                    break;
                }
            }
        }

        res.rights = (memory_rights)rights;
        return res;
    }

    std::vector<region_infos_t> GetAllRegions()
    {
        std::vector<region_infos_t> mappings;

        char* str_it;
        const char* str_end;
        uintptr_t start;
        uintptr_t end;
        uintptr_t old_end(0);
        unsigned int rights;

        std::ifstream f("/proc/self/maps");
        std::string s;

        while (std::getline(f, s))
        {
            if (!s.empty())
            {
                str_it = &s[0];
                str_end = s.data() + s.length();
                start = (uintptr_t)strtoul(str_it, &str_it, 16);
                end = (uintptr_t)strtoul(str_it + 1, &str_it, 16);
                if (start != 0 && end != 0)
                {
                    if (old_end != start)
                    {
                        mappings.emplace_back(
                            memory_rights::mem_unset,
                            old_end,
                            start,
                            std::string()
                        );
                    }

                    old_end = end;

                    rights = memory_rights::mem_none;

                    ++str_it;
                    if (str_it[0] == 'r')
                        rights |= mem_r;

                    if (str_it[1] == 'w')
                        rights |= mem_w;

                    if (str_it[2] == 'x')
                        rights |= mem_x;

                    for (int i = 0; i < 4; ++i)
                    {
                        while (*str_it != ' ' && str_it < str_end)
                        {
                            ++str_it;
                        }
                        while (*str_it == ' ' && str_it < str_end)
                        {
                            ++str_it;
                        }
                    }

                    mappings.emplace_back(
                        (memory_rights)rights,
                        start,
                        end,
                        str_it
                    );
                }
            }
        }

        return mappings;
    }

    std::vector<region_infos_t> GetFreeRegions()
    {
        std::vector<region_infos_t> mappings;

        char* str_it;
        const char* str_end;
        uintptr_t start;
        uintptr_t end;
        uintptr_t old_end(0);

        std::ifstream f("/proc/self/maps");
        std::string s;

        while (std::getline(f, s))
        {
            if (!s.empty())
            {
                str_it = &s[0];
                str_end = s.data() + s.length();
                start = (uintptr_t)strtoul(str_it, &str_it, 16);
                end = (uintptr_t)strtoul(str_it + 1, &str_it, 16);
                if (start != 0 && end != 0)
                {
                    if (old_end != start)
                    {
                        mappings.emplace_back(
                            memory_rights::mem_unset,
                            old_end,
                            start,
                            std::string()
                        );
                    }

                    old_end = end;
                }
            }
        }

        return mappings;
    }

    bool MemoryProtect(void* address, size_t size, memory_rights rights, memory_rights* old_rights)
    {
        region_infos_t infos;
        if (old_rights != nullptr)
            infos = GetRegionInfos(address);

        bool res = mprotect(PageRound(address, PageSize()), page_addr_size(address, size, PageSize()), memory_protect_rights_to_native(rights)) == 0;

        if (old_rights != nullptr)
            *old_rights = infos.rights;

        return res;
    }

    void MemoryFree(void* address, size_t size)
    {
        if (address != nullptr)
            munmap(address, size);
    }

    static inline void* MemoryAllocNear(uintptr_t addressHint, size_t size, int nativeRights, size_t pageSize)
    {
        auto freeRegions = GetFreeRegions();

        std::sort(freeRegions.begin(), freeRegions.end(), [addressHint](MemoryManipulation::region_infos_t const& l, MemoryManipulation::region_infos_t const& r)
        {
            return std::max(addressHint, l.start) - std::min(addressHint, l.start) <
                std::max(addressHint, r.start) - std::min(addressHint, r.start);
        });

        for (auto const& region : freeRegions)
        {
            for (auto allocAddress = region.start; (allocAddress + size) < region.end; allocAddress += pageSize)
            {
                if (allocAddress > (uintptr_t)max_user_address)
                    break;

                void* r = mmap((void*)allocAddress, size, nativeRights, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

                if (r != nullptr)
                    return r;
            }
        }

        // Fallback to hint alloc
        return mmap((void*)addressHint, size, nativeRights, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    void* MemoryAlloc(void* _addressHint, size_t size, memory_rights rights)
    {
        if (_addressHint > max_user_address)
            _addressHint = (void*)max_user_address;

        auto pageSize = PageSize();
        auto addressHint = reinterpret_cast<uintptr_t>(PageRound(_addressHint, pageSize));
        size = page_addr_size((void*)addressHint, size, pageSize);
        const auto nativeRights = memory_protect_rights_to_native(rights);

        if (_addressHint == nullptr)
            return mmap(nullptr, size, nativeRights, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        return MemoryAllocNear(addressHint, size, nativeRights, pageSize);
    }

    bool SafeMemoryRead(void* address, uint8_t* buffer, size_t size)
    {
        struct iovec local;
        struct iovec remote;

        local.iov_base = buffer;
        local.iov_len = size;
        remote.iov_base = (void*)address;
        remote.iov_len = size;

        if (process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != size)
            return false;

        return true;
    }

    bool SafeMemoryWrite(void* address, const uint8_t* buffer, size_t size)
    {
        struct iovec local;
        struct iovec remote;

        local.iov_base = (void*)buffer;
        local.iov_len = size;
        remote.iov_base = (void*)address;
        remote.iov_len = size;

        if (process_vm_writev(getpid(), &local, 1, &remote, 1, 0) != size)
            return false;

        return true;
    }

    int FlushInstructionCache(void* address, size_t size)
    {
        return 1;
    }
}

#if defined(MINIDETOUR_ARCH_X64)
#include "mini_detour_x64.h"

#elif defined(MINIDETOUR_ARCH_X86)
#include "mini_detour_x86.h"

#elif defined(MINIDETOUR_ARCH_ARM64)
#include "mini_detour_arm64.h"

#elif defined(MINIDETOUR_ARCH_ARM)
#include "mini_detour_arm.h"

#endif

#endif//MINI_DETOUR_LINUX_H