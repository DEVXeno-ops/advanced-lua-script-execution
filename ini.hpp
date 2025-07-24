#pragma once

#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include "file.hpp" // Assumes updated win32::File with wstring support
#include "lua.hpp"  // Lua header

namespace pIni
{
    // Trims whitespace from both ends of a string view, returning a string
    inline std::wstring trim(std::wstring_view s) noexcept
    {
        auto start = s.begin();
        while (start != s.end() && std::isspace(*start)) ++start;

        auto end = s.end();
        do {
            --end;
        } while (std::distance(start, end) > 0 && std::isspace(*end));

        return std::wstring(start, end + 1);
    }

    // Represents a section in an INI file, storing key-value pairs
    class Section
    {
    private:
        std::map<std::wstring, std::wstring> m_data;

    public:
        // Returns the key-value map (const access)
        const std::map<std::wstring, std::wstring>& GetData() const noexcept
        {
            return m_data;
        }

        // Checks if a key exists
        bool Exist(std::wstring_view key) const noexcept
        {
            return m_data.find(key) != m_data.end();
        }

        // Non-const access to a key's value, creating it if it doesn't exist
        std::wstring& operator[](std::wstring_view key)
        {
            return m_data[key];
        }

        // Const access to a key's value, returning a default if not found
        const std::wstring& GetValue(std::wstring_view key, const std::wstring& defaultValue = L"") const noexcept
        {
            auto it = m_data.find(key);
            return (it != m_data.end()) ? it->second : defaultValue;
        }
    };

    // Manages an INI file, parsing sections and executing Lua scripts
    class Archive
    {
    private:
        std::wstring m_fileName;
        std::map<std::wstring, Section> m_sections;
        mutable std::mutex m_mutex; // For thread-safe access

    public:
        explicit Archive(const std::wstring& filename) : m_fileName(filename)
        {
            std::wstring content;
            win32::File file(filename);
            std::string error;

            if (!file.Read(content, &error))
            {
                throw std::runtime_error("Failed to read INI file: " + error);
            }

            std::wistringstream contentStream(content);
            std::wstring line, section;

            while (std::getline(contentStream, line))
            {
                // Remove \r for Windows line endings
                if (!line.empty() && line.back() == L'\r') line.pop_back();

                // Skip empty lines or comments
                if (line.empty() || line[0] == L';' || line[0] == L'#') continue;

                // Parse section header
                if (line.front() == L'[' && line.back() == L']')
                {
                    section = trim(line.substr(1, line.size() - 2));
                    if (section.empty())
                    {
                        throw std::runtime_error("Invalid empty section name in INI file");
                    }
                }
                // Parse key-value pair
                else if (!section.empty())
                {
                    auto delimiterPos = line.find(L'=');
                    if (delimiterPos != std::wstring::npos)
                    {
                        std::wstring key = trim(line.substr(0, delimiterPos));
                        std::wstring value = trim(line.substr(delimiterPos + 1));
                        if (key.empty())
                        {
                            throw std::runtime_error("Invalid empty key in section [" + std::string(section.begin(), section.end()) + "]");
                        }
                        m_sections[section][key] = value;
                    }
                    else
                    {
                        throw std::runtime_error("Malformed key-value pair in section [" + std::string(section.begin(), section.end()) + "]");
                    }
                }
            }
        }

        // Saves the INI data to the file
        void Save() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::wostringstream contentStream;
            for (const auto& section : m_sections)
            {
                contentStream << L"[" << section.first << L"]\n";
                for (const auto& entry : section.second.GetData())
                {
                    contentStream << entry.first << L"=" << entry.second << L"\n";
                }
                contentStream << L"\n";
            }

            win32::File file(m_fileName);
            std::string error;
            if (!file.Write(contentStream.str(), &error))
            {
                throw std::runtime_error("Failed to write INI file: " + error);
            }
        }

        // Checks if a section exists
        bool Exist(std::wstring_view section) const noexcept
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_sections.find(section) != m_sections.end();
        }

        // Non-const access to a section, creating it if it doesn't exist
        Section& operator[](std::wstring_view section)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_sections[section];
        }

        // Executes Lua scripts from a section's values
        std::vector<std::string> ExecuteLuaScript(lua_State* L, std::wstring_view section) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::vector<std::string> errors;

            if (!Exist(section)) return errors;

            for (const auto& entry : m_sections.at(section).GetData())
            {
                const std::wstring& script = entry.second;
                // Convert wstring to string for Lua (assuming ASCII-compatible scripts)
                std::string scriptStr(script.begin(), script.end());
                if (luaL_loadstring(L, scriptStr.c_str()) != LUA_OK || lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)
                {
                    errors.emplace_back(lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }

            return errors;
        }
    };
}