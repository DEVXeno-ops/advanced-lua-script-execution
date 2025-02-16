#include <Windows.h>
#include <map>
#include <vector>
#include <iostream>

#include "file.hpp"
#include "fx.hpp"
#include "isolated.hpp"
#include "ini.hpp"

namespace memory
{
    fx::NetLibrary** g_netLibrary;
    std::vector<fx::ResourceImpl*>* g_allResources;

    bool InitMemory()
    {
        const uint64_t resourceModule = (uint64_t)GetModuleHandleA("citizen-resources-core.dll");

        if (!resourceModule)
        {
            MessageBoxA(0, "Failed to locate citizen-resources-core.dll", "Error", MB_ICONERROR);
            return false;
        }

        const uint64_t netFiveModule = (uint64_t)GetModuleHandleA("gta-net-five.dll");

        if (!netFiveModule)
        {
            MessageBoxA(0, "Failed to locate gta-net-five.dll", "Error", MB_ICONERROR);
            return false;
        }

        g_allResources = reinterpret_cast<std::vector<fx::ResourceImpl*>*>(resourceModule + 0xAE6C0);
        g_netLibrary = reinterpret_cast<fx::NetLibrary**>(netFiveModule + 0x1F41D8);

        if (!g_allResources || !g_netLibrary)
        {
            MessageBoxA(0, "Memory initialization failed. Offsets might be outdated.", "Error", MB_ICONERROR);
            return false;
        }

        return true;
    }

    void ForAllResources(const std::function<void(fx::ResourceImpl*)>& cb)
    {
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
        std::string result = filename;
        std::string illegalChars = "\\/:*?\"<>|";
        std::string illegalWord = "http";

        for (char c : illegalChars)
        {
            result.erase(std::remove(result.begin(), result.end(), c), result.end());
        }

        size_t pos;
        while ((pos = result.find(illegalWord)) != std::string::npos)
        {
            result.erase(pos, illegalWord.length());
        }

        return result;
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
        const std::vector<CachedScript>& GetCachedScripts() const { return m_cachedScripts; }

    private:
        std::string m_name;
        std::vector<CachedScript> m_cachedScripts;
    };

    std::vector<CachedResource> g_cachedResources;

    CachedResource& AddCachedResource(const std::string& path, const std::string& name)
    {
        auto it = std::find_if(g_cachedResources.begin(), g_cachedResources.end(),
            [&name](CachedResource& cr) { return cr.GetName() == name; });

        if (it != g_cachedResources.end()) return *it;

        CachedResource newResource;
        newResource.SetName(name);

        if (!win32::DirectoryExists(path))
            win32::CreateNewDirectory(path, true);

        win32::CreateNewDirectory(path + name, true);

        g_cachedResources.push_back(newResource);
        return g_cachedResources.back();
    }
}

namespace lua
{
    std::string g_filePath = "C:\\Plugins\\Script.lua";

    std::string LoadScriptFile(const std::string& scriptFile)
    {
        win32::File fileHandle(scriptFile);
        std::string fileData;
        fileHandle.Read(fileData);
        return fileData;
    }
}

namespace script
{
    std::string g_globalPath = "C:\\Plugins\\";

    bool g_enableCacheSaving = true;
    bool g_enableScriptExecution = true;
    bool g_enableIsolatedExecution = false;

    bool g_hasScriptBeenExecuted = false;
    std::vector<std::string> g_resourceBlacklist;

    int g_targetIndex = 0;
    bool g_replaceTarget = false;
    std::string g_scriptExecutionTarget = "spawnmanager";

    std::map<std::string, int> g_resourceCounter;

    bool AddScriptHandlers()
    {
        if (memory::g_allResources->empty())
            return false;

        memory::ForAllResources([](fx::ResourceImpl* resource)
        {
            g_resourceCounter[resource->m_name] = 0;

            fx::Connect(resource->OnBeforeLoadScript, [resource](std::vector<char>* fileData)
            {
                int resolvedCounter = g_resourceCounter[resource->m_name] - 4;

                if (resolvedCounter >= 0)
                {
                    if (g_enableCacheSaving)
                    {
                        std::string cachePath = ch::g_cachePath + ch::SanitizeFileName((*memory::g_netLibrary)->m_currentServerUrl) + "\\";
                        ch::CachedResource& cachedResource = ch::AddCachedResource(cachePath, resource->m_name);
                        cachedResource.AddCachedScript(resolvedCounter, std::string(fileData->begin(), fileData->end()), cachePath);
                    }

                    if (std::find(g_resourceBlacklist.begin(), g_resourceBlacklist.end(), resource->m_name) != g_resourceBlacklist.end())
                    {
                        fileData->clear();
                    }

                    if (g_enableScriptExecution && !g_hasScriptBeenExecuted &&
                        resource->m_name.find(g_scriptExecutionTarget) != std::string::npos &&
                        resolvedCounter == g_targetIndex)
                    {
                        std::string buffer = lua::LoadScriptFile(lua::g_filePath);
                        if (g_replaceTarget)
                            fileData->clear();
                        
                        std::string resolvedBuffer = (g_enableIsolatedExecution ? isolated::getInput(buffer) : buffer) + ";";
                        fileData->insert(fileData->begin(), resolvedBuffer.begin(), resolvedBuffer.end());

                        g_hasScriptBeenExecuted = true;
                    }
                }

                g_resourceCounter[resource->m_name]++;
            });
        });

        return true;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        return memory::InitMemory() && script::AddScriptHandlers();
    }
    return TRUE;
}
