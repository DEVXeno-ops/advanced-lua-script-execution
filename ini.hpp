#pragma once

#include <map>
#include <sstream>
#include "file.hpp"
#include "lua.hpp" // Include Lua header

namespace pIni
{
    class Section
    {
    private:
        std::map<std::string, std::string> m_data;
    public:
        const std::map<std::string, std::string>& GetData() const
        {
            return this->m_data;
        }

        bool Exist(const std::string& data)
        {
            return m_data.find(data) != m_data.end();
        }

        std::string& operator[](const std::string& key)
        {
            return this->m_data[key];
        }
    };

    class Archive
    {
    private:
        struct
        {
            std::string m_fileName;
            std::map<std::string, Section> m_sections;
        };

    public:
        Archive(const std::string& filename) : m_fileName(filename)
        {
            auto content = std::string();
            auto file = win32::File(filename);

            if (!file.Read(content))
            {
                return;
            }

            auto line = std::string();
            auto section = std::string();
            auto contentStream = std::istringstream(content);

            while (std::getline(contentStream, line))
            {
                if (line.empty()) continue;

                if (line[0] == '[' && line.back() == ']')
                {
                    section = line.substr(1, line.size() - 2);
                }
                else
                {
                    auto delimiterPos = line.find('=');
                    if (delimiterPos != std::string::npos)
                    {
                        this->m_sections[section][line.substr(0, delimiterPos)] = line.substr(delimiterPos + 1);
                    }
                }
            }
        }

        void Save()
        {
            auto contentStream = std::ostringstream();
            for (const auto& section : this->m_sections)
            {
                contentStream << "[" << section.first << "]\n";
                for (const auto& entry : section.second.GetData())
                {
                    contentStream << entry.first << "=" << entry.second << "\n";
                }
                contentStream << "\n";
            }

            auto file = win32::File(this->m_fileName);
            file.Write(contentStream.str());
        }

        bool Exist(const std::string& section)
        {
            return this->m_sections.find(section) != this->m_sections.end();
        }

        Section& operator[](const std::string& section)
        {
            return this->m_sections[section];
        }

        void ExecuteLuaScript(lua_State* L, const std::string& section)
        {
            if (!Exist(section)) return;

            for (const auto& entry : this->m_sections[section].GetData())
            {
                std::string script = entry.second;
                if (luaL_dostring(L, script.c_str()) != LUA_OK)
                {
                    printf("Lua Error: %s\n", lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
        }
    };
}
