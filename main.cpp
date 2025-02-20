#include <Windows.h>
#include <map>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <regex>
#include <shared_mutex>
#include <functional>
#include <string>

#include "file.hpp"
#include "fx.hpp"
#include "isolated.hpp"
#include "ini.hpp"

namespace memory {
    fx::NetLibrary** g_netLibrary = nullptr;
    std::vector<fx::ResourceImpl*>* g_allResources = nullptr;

    bool InitMemory() {
        const auto resourceModule = reinterpret_cast<uint64_t>(GetModuleHandleA("citizen-resources-core.dll"));
        if (!resourceModule) {
            MessageBoxA(0, "Failed to locate citizen-resources-core.dll", "Error", MB_ICONERROR);
            return false;
        }

        const auto netFiveModule = reinterpret_cast<uint64_t>(GetModuleHandleA("gta-net-five.dll"));
        if (!netFiveModule) {
            MessageBoxA(0, "Failed to locate gta-net-five.dll", "Error", MB_ICONERROR);
            return false;
        }

        g_allResources = reinterpret_cast<std::vector<fx::ResourceImpl*>*>(resourceModule + 0xAE6C0);
        g_netLibrary = reinterpret_cast<fx::NetLibrary**>(netFiveModule + 0x1F41D8);

        if (!g_allResources || !g_netLibrary) {
            MessageBoxA(0, "Memory initialization failed. Offsets might be outdated.", "Error", MB_ICONERROR);
            return false;
        }
        return true;
    }

    void ForAllResources(const std::function<void(fx::ResourceImpl*)>& cb) {
        if (g_allResources) {
            for (auto& resource : *g_allResources) {
                if (resource) {
                    cb(resource);
                }
            }
        }
    }
}

namespace ch {
    std::string g_cachePath = "C:\\Plugins\\Cache\\";

    std::string SanitizeFileName(const std::string& filename) {
        static const std::unordered_set<char> illegalChars = {'\\', '/', ':', '*', '?', '"', '<', '>', '|'};
        std::string result;
        result.reserve(filename.size());
        
        for (char c : filename) {
            if (illegalChars.find(c) == illegalChars.end()) {
                result.push_back(c);
            }
        }
        
        return std::regex_replace(result, std::regex("http"), "");
    }

    class CachedScript {
    public:
        void SetIndex(int index) { m_index = index; }
        void SetData(const std::string& data) { m_data = data; }
        int GetIndex() const { return m_index; }
        const std::string& GetData() const { return m_data; }
    private:
        int m_index = -1;
        std::string m_data;
    };

    class CachedResource {
    public:
        bool AddCachedScript(int index, const std::string& data, const std::string& directoryPath) {
            std::unique_lock<std::shared_mutex> lock(m_mutex);

            // Avoid duplication
            if (m_cachedScripts.find(index) != m_cachedScripts.end()) {
                return false;
            }

            CachedScript newScript;
            newScript.SetIndex(index);
            newScript.SetData(data);
            m_cachedScripts[index] = newScript;

            win32::File fileHandle(directoryPath + GetName() + "\\script_" + std::to_string(index) + ".lua");
            return fileHandle.Write(data);
        }

        void SetName(const std::string& name) { m_name = name; }
        const std::string& GetName() const { return m_name; }

    private:
        std::string m_name;
        std::unordered_map<int, CachedScript> m_cachedScripts;
        mutable std::shared_mutex m_mutex;
    };

    std::vector<CachedResource> g_cachedResources;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        if (!memory::InitMemory()) {
            return FALSE;
        }
    }
    return TRUE;
}
