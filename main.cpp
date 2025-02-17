#include <Windows.h>
#include <map>
#include <vector>
#include <iostream>
#include <unordered_set>
#include <regex>
#include <mutex>

#include "file.hpp"
#include "fx.hpp"
#include "isolated.hpp"
#include "ini.hpp"

namespace memory
{
    fx::NetLibrary** g_netLibrary = nullptr;
    std::vector<fx::ResourceImpl*>* g_allResources = nullptr;

    bool InitMemory()
    {
        const uint64_t resourceModule = reinterpret_cast<uint64_t>(GetModuleHandleA("citizen-resources-core.dll"));
        if (!resourceModule)
        {
            MessageBoxA(0, "Failed to locate citizen-resources-core.dll", "Error", MB_ICONERROR);
            return false;
        }

        const uint64_t netFiveModule = reinterpret_cast<uint64_t>(GetModuleHandleA("gta-net-five.dll"));
        if (!netFiveModule)
        {
            MessageBoxA(0, "Failed to locate gta-net-five.dll", "Error", MB_ICONERROR);
            return false;
        }

        g_allResources = reinterpret_cast<std::vector<fx::ResourceImpl*>*>(resourceModule + 0xAE6C0);
        g_netLibrary = reinterpret_cast<fx::NetLibrary**>(netFiveModule + 0x1F41D8);

        if (!g_allResources || !g_netLibrary || g_allResources->empty())
        {
            MessageBoxA(0, "Memory initialization failed. Offsets might be outdated.", "Error", MB_ICONERROR);
            return false;
        }
        return true;
    }

    void ForAllResources(const std::function<void(fx::ResourceImpl*)>& cb)
    {
        if (!g_allResources || g_allResources->empty()) return;
        for (fx::ResourceImpl* resource : *g_allResources)
        {
            if (resource)
                cb(resource);
        }
    }
}

namespace ch
{
    std::string g_cachePath = "C:\\Plugins\\Cache\\";

    std::string SanitizeFileName(const std::string& filename)
    {
        static const std::unordered_set<char> illegalSet = {'\\', '/', ':', '*', '?', '"', '<', '>', '|'};
        std::string result;
        std::copy_if(filename.begin(), filename.end(), std::back_inserter(result), 
                     [&](char c) { return illegalSet.find(c) == illegalSet.end(); });
        return std::regex_replace(result, std::regex("http"), "");
    }

    class CachedScript
    {
    public:
        void SetIndex(int index) { m_index = index; }
        void SetData(const std::string& data) { m_data = data; }
        int GetIndex() const { return m_index; }
        const std::string& GetData() const { return m_data; }
    private:
        int m_index;
        std::string m_data;
    };

    class CachedResource
    {
    public:
        bool AddCachedScript(int index, const std::string& data, const std::string& directoryPath)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& script : m_cachedScripts)
            {
                if (script.GetIndex() == index || script.GetData().find(data) != std::string::npos)
                    return false;
            }
            CachedScript newScript;
            newScript.SetIndex(index);
            newScript.SetData(data);
            m_cachedScripts.push_back(newScript);

            win32::File fileHandle(directoryPath + GetName() + "\\script_" + std::to_string(index) + ".lua");
            fileHandle.Write(data);

            return true;
        }

        void SetName(const std::string& name) { m_name = name; }
        const std::string& GetName() const { return m_name; }
    private:
        std::string m_name;
        std::vector<CachedScript> m_cachedScripts;
        std::mutex m_mutex;
    };

    std::vector<CachedResource> g_cachedResources;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        return memory::InitMemory();
    }
    return TRUE;
}
