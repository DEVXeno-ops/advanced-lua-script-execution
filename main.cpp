#pragma once
#include <Windows.h>
#include <map>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <shared_mutex>
#include <functional>
#include <string>
#include <filesystem>
#include <mutex>
#include <optional>

#include "file.hpp"
#include "fx.hpp"
#include "isolated.hpp"
#include "ini.hpp"

namespace memory {
    fx::NetLibrary** g_netLibrary = nullptr;
    std::vector<fx::ResourceImpl*>* g_allResources = nullptr;

    // Logs an error message to a file
    inline void LogError(const std::wstring& message) {
        win32::File file(L"C:\\Plugins\\Logs\\error.log");
        std::string error;
        file.Write(std::wstring(message + L"\n"), &error);
    }

    // Initializes memory offsets for accessing game resources
    bool InitMemory() {
        const auto resourceModule = reinterpret_cast<uint64_t>(GetModuleHandleW(L"citizen-resources-core.dll"));
        if (!resourceModule) {
            LogError(L"Failed to locate citizen-resources-core.dll");
            return false;
        }

        const auto netFiveModule = reinterpret_cast<uint64_t>(GetModuleHandleW(L"gta-net-five.dll"));
        if (!netFiveModule) {
            LogError(L"Failed to locate gta-net-five.dll");
            return false;
        }

        g_allResources = reinterpret_cast<std::vector<fx::ResourceImpl*>*>(resourceModule + 0xAE6C0);
        g_netLibrary = reinterpret_cast<fx::NetLibrary**>(netFiveModule + 0x1F41D8);

        // Validate pointers
        if (!g_allResources || !*g_allResources || !g_netLibrary || !*g_netLibrary) {
            LogError(L"Memory initialization failed. Offsets might be outdated or memory inaccessible.");
            return false;
        }

        return true;
    }

    // Iterates over all resources, invoking the provided callback
    void ForAllResources(const std::function<void(fx::ResourceImpl*)>& cb) noexcept {
        if (g_allResources && *g_allResources) {
            for (auto* resource : *g_allResources) {
                if (resource) {
                    cb(resource);
                }
            }
        }
    }
}

namespace ch {
    std::wstring g_cachePath = L"C:\\Plugins\\Cache\\";
    std::shared_mutex g_resourcesMutex;

    // Loads cache path from INI configuration
    inline void LoadConfig() {
        try {
            pIni::Archive config(L"C:\\Plugins\\config.ini");
            if (config.Exist(L"Settings")) {
                g_cachePath = config[L"Settings"].GetValue(L"CachePath", L"C:\\Plugins\\Cache\\");
                // Ensure trailing backslash
                if (!g_cachePath.empty() && g_cachePath.back() != L'\\') {
                    g_cachePath += L'\\';
                }
            }
        }
        catch (const std::exception& e) {
            memory::LogError(std::wstring(L"Failed to load config: ") + std::wstring(e.what(), e.what() + strlen(e.what())));
        }
    }

    // Sanitizes a filename to remove illegal characters and reserved patterns
    std::wstring SanitizeFileName(std::wstring_view filename) {
        static const std::unordered_set<wchar_t> illegalChars = {L'\\', L'/', L':', L'*', L'?', L'"', L'<', L'>', L'|'};
        std::wstring result;
        result.reserve(filename.size());

        for (wchar_t c : filename) {
            if (illegalChars.find(c) == illegalChars.end()) {
                result.push_back(c);
            }
        }

        // Remove "http" and reserved Windows names (e.g., CON, PRN)
        result = std::regex_replace(result, std::wregex(L"http", std::regex::icase), L"");
        static const std::vector<std::wstring> reservedNames = {L"CON", L"PRN", L"AUX", L"NUL"};
        for (const auto& reserved : reservedNames) {
            if (result == reserved) {
                return L"_" + result;
            }
        }

        return result.empty() ? L"unnamed" : result;
    }

    class CachedScript {
    public:
        void SetIndex(int index) noexcept { m_index = index; }
        void SetData(const std::wstring& data) { m_data = data; }
        int GetIndex() const noexcept { return m_index; }
        const std::wstring& GetData() const noexcept { return m_data; }

    private:
        int m_index = -1;
        std::wstring m_data;
    };

    class CachedResource {
    public:
        // Adds a cached script, writing it to disk after processing
        bool AddCachedScript(int index, std::vector<uint8_t> data, const std::wstring& directoryPath, std::string* error = nullptr) {
            std::unique_lock<std::shared_mutex> lock(m_mutex);

            if (m_cachedScripts.find(index) != m_cachedScripts.end()) {
                if (error) *error = "Script already cached for index " + std::to_string(index);
                return false;
            }

            // Process Lua script (e.g., remove isoBytes marker)
            std::string processingError;
            if (!isolated::processLuaScript(data, isolated::isoBytes, {}, &processingError)) {
                if (error) *error = "Failed to process Lua script: " + processingError;
                return false;
            }

            // Convert processed bytes to wstring (assuming UTF-8)
            std::wstring scriptData(data.begin(), data.end());
            CachedScript newScript;
            newScript.SetIndex(index);
            newScript.SetData(scriptData);
            m_cachedScripts[index] = std::move(newScript);

            // Create directory and write file
            std::filesystem::path dirPath = directoryPath + SanitizeFileName(GetName());
            try {
                std::filesystem::create_directories(dirPath);
                win32::File file((dirPath / (L"script_" + std::to_wstring(index) + L".lua")).wstring());
                if (!file.Write(scriptData, error)) {
                    return false;
                }
            }
            catch (const std::filesystem::filesystem_error& e) {
                if (error) *error = "Failed to create directory or write file: " + std::string(e.what());
                return false;
            }

            return true;
        }

        void SetName(const std::wstring& name) { m_name = SanitizeFileName(name); }
        const std::wstring& GetName() const noexcept { return m_name; }

    private:
        std::wstring m_name;
        std::unordered_map<int, CachedScript> m_cachedScripts;
        mutable std::shared_mutex m_mutex;
    };

    std::vector<CachedResource> g_cachedResources;

    // Adds a resource to the cache
    void AddCachedResource(const std::wstring& name) {
        std::unique_lock<std::shared_mutex> lock(g_resourcesMutex);
        CachedResource resource;
        resource.SetName(name);
        g_cachedResources.emplace_back(std::move(resource));
    }
}

// Hooks resource script loading to cache scripts
void SetupResourceScriptHook() {
    memory::ForAllResources([](fx::ResourceImpl* resource) {
        if (!resource) return;

        fx::Connect(resource->OnBeforeLoadScript, [](std::vector<char>* scriptBytes) {
            std::vector<uint8_t> unsignedScriptBytes(scriptBytes->begin(), scriptBytes->end());
            std::string error;
            std::wstring resourceName = std::wstring(resource->m_name.begin(), resource->m_name.end());

            // Find or create cached resource
            {
                std::shared_lock<std::shared_mutex> lock(ch::g_resourcesMutex);
                auto it = std::find_if(ch::g_cachedResources.begin(), ch::g_cachedResources.end(),
                    [&](const ch::CachedResource& res) { return res.GetName() == resourceName; });
                if (it == ch::end()) {
                    lock.unlock();
                    ch::AddCachedResource(resourceName);
                    lock.lock();
                    it = std::find_if(ch::g_cachedResources.begin(), ch::g_cachedResources.end(),
                        [&](const ch::CachedResource& res) { return res.GetName() == resourceName; });
                }

                if (it != ch::g_cachedResources.end()) {
                    static int scriptIndex = 0;
                    it->AddCachedScript(scriptIndex++, unsignedScriptBytes, ch::g_cachePath, &error);
                }
            }

            if (!error.empty()) {
                memory::LogError(std::wstring(L"Script caching error: ") + std::wstring(error.begin(), error.end()));
            }

            return true;
        });
    });
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        ch::LoadConfig();
        if (!memory::InitMemory()) {
            return FALSE;
        }
        SetupResourceScriptHook();
    }
    return TRUE;
}