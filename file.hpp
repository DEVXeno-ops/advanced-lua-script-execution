#pragma once

#include <string>
#include <Windows.h>
#include <memory>
#include <vector>
#include <iostream>

namespace win32
{
    class File
    {
    private:
        std::string fileName;

    public:
        explicit File(const std::string& fileName) : fileName(fileName) {}
        ~File() = default;

        bool Read(std::string& content)
        {
            HANDLE fileHandle = INVALID_HANDLE_VALUE;
            if (!GetFileHandleForRead(fileHandle))
            {
                return false;
            }

            LARGE_INTEGER fileSize;
            if (!GetFileSizeEx(fileHandle, &fileSize) || fileSize.QuadPart == 0)
            {
                CloseHandle(fileHandle);
                return false;
            }

            std::vector<char> buffer(static_cast<size_t>(fileSize.QuadPart));
            DWORD bytesRead = 0;
            const BOOL result = ReadFile(fileHandle, buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr);
            CloseHandle(fileHandle);

            if (result == 0 || bytesRead != fileSize.QuadPart)
            {
                return false;
            }

            content.assign(buffer.begin(), buffer.end());
            return true;
        }

        bool Write(const std::string& content)
        {
            HANDLE fileHandle = INVALID_HANDLE_VALUE;
            if (!GetFileHandleForWrite(fileHandle))
            {
                return false;
            }

            DWORD bytesWritten = 0;
            const BOOL result = WriteFile(fileHandle, content.c_str(), static_cast<DWORD>(content.size()), &bytesWritten, nullptr);
            CloseHandle(fileHandle);

            return result != 0 && bytesWritten == content.size();
        }

    private:
        bool GetFileHandleForRead(HANDLE& handle) const
        {
            handle = CreateFileA(
                fileName.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );

            return handle != INVALID_HANDLE_VALUE;
        }

        bool GetFileHandleForWrite(HANDLE& handle) const
        {
            handle = CreateFileA(
                fileName.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );

            return handle != INVALID_HANDLE_VALUE;
        }
    };

    inline bool FileExists(const std::string& path)
    {
        const DWORD attributes = GetFileAttributesA(path.c_str());
        return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline bool DirectoryExists(const std::string& path)
    {
        const DWORD attributes = GetFileAttributesA(path.c_str());
        return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline bool CreateNewDirectory(const std::string& path, bool createAlways = false)
    {
        if (DirectoryExists(path))
        {
            return true;
        }

        if (CreateDirectoryA(path.c_str(), nullptr))
        {
            return true;
        }

        DWORD err = GetLastError();
        if (err == ERROR_ALREADY_EXISTS)
        {
            return DirectoryExists(path);
        }

        return false;
    }

    inline void delay(int milliseconds)
    {
        Sleep(static_cast<DWORD>(milliseconds));
    }

    inline void clear()
    {
        system("cls");
    }

    inline void pause()
    {
        system("pause");
    }
}
