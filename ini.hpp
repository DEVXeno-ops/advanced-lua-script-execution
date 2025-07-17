#pragma once

#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include "file.hpp"
#include "lua.hpp" // Include Lua header

namespace pIni
{
    inline std::string trim(const std::string& s)
    {
        auto start = s.begin();
        while (start != s.end() && std::isspace(*start)) ++start;

        auto end = s.end();
        do {
            --end;
        } while (std::distance(start, end) > 0 && std::isspace(*end));

        return std::string(start, end + 1);
    }

    class Section
    {
    private:
        std::map<std::string, std::string> m_data;
    public:
        const std::map<std::string, std::string>& GetData() const
        {
            return this->m_data;
        }

        bool Exist(const std::string& data) const
        {
            return m_data.find(data) != m_data.end();
        }

        std::string& operator[](const std::string& key)
        {
            return this->m_data[key];
        }

        const std::string& GetValue(const std::string& key, const std::string& defaultValue = "") const
        {
            auto it = m_data.find(key);
            return (it != m_data.end()) ? it->second : defaultValue;
        }
    };

    class Archive
    {
    private:
        std::string m_fileName;
        std::map<std::string, Section> m_sections;

    public:
        explicit Archive(const std::string& filename) : m_fileName(filename)
        {
            std::string content;
            win32::File file(filename);

            if (!file.Read(content))
            {
                // อาจ log หรือแจ้งเตือนที่นี่
                return;
            }

            std::istringstream contentStream(content);
            std::string line, section;

            while (std::getline(contentStream, line))
            {
                // ตัด \r ที่อาจมีติดท้าย (จาก Windows line ending)
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (line.empty() || line[0] == ';' || line[0] == '#') continue;

                if (line.front() == '[' && line.back() == ']')
                {
                    section = trim(line.substr(1, line.size() - 2));
                }
                else if (!section.empty())
                {
                    auto delimiterPos = line.find('=');
                    if (delimiterPos != std::string::npos)
                    {
                        std::string key = trim(line.substr(0, delimiterPos));
                        std::string value = trim(line.substr(delimiterPos + 1));
                        m_sections[section][key] = value;
                    }
                }
            }
        }

        void Save() const
        {
            std::ostringstream contentStream;
            for (const auto& section : m_sections)
            {
                contentStream << "[" << section.first << "]\n";
                for (const auto& entry : section.second.GetData())
                {
                    contentStream << entry.first << "=" << entry.second << "\n";
                }
                contentStream << "\n";
            }

            win32::File file(m_fileName);
            file.Write(contentStream.str());
        }

        bool Exist(const std::string& section) const
        {
            return m_sections.find(section) != m_sections.end();
        }

        // เปลี่ยนให้สร้าง section ใหม่ถ้าไม่มี
        Section& operator[](const std::string& section)
        {
            return m_sections[section];
        }

        void ExecuteLuaScript(lua_State* L, const std::string& section) const
        {
            if (!Exist(section)) return;

            for (const auto& entry : m_sections.at(section).GetData())
            {
                const std::string& script = entry.second;
                if (luaL_loadstring(L, script.c_str()) != LUA_OK || lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK)
                {
                    printf("Lua Error: %s\n", lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
        }
    };
}
