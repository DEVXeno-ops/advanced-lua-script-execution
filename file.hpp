#pragma once

#include <string>
#include <Windows.h>
#include <memory>
#include <vector>

namespace win32
{
    class File
    {
    private:
        std::string fileName;

    public:
        File(const std::string& fileName) : fileName(fileName) {}
        ~File() = default;

        bool Read(std::string& content)
        {
            HANDLE fileHandle = INVALID_HANDLE_VALUE;
            if (!GetFileHandle(OPEN_EXISTING, fileHandle))
            {
                return false;
            }

            const DWORD fileSize = GetFileSize(fileHandle, nullptr);
            if (fileSize == INVALID_FILE_SIZE)
            {
                CloseHandle(fileHandle);
                return false;
            }

            std::vector<char> buffer(fileSize);
            DWORD bytesRead = 0;
            const BOOL result = ReadFile(fileHandle, buffer.data(), fileSize, &bytesRead, nullptr);
            CloseHandle(fileHandle);

            if (result == 0 || bytesRead != fileSize)
            {
                return false;
            }

            content.assign(buffer.begin(), buffer.end());
            return true;
        }

        bool Write(const std::string& content)
        {
            HANDLE fileHandle = INVALID_HANDLE_VALUE;
            if (!GetFileHandle(CREATE_ALWAYS, fileHandle))
            {
                return false;
            }

            DWORD bytesWritten = 0;
            const BOOL result = WriteFile(fileHandle, content.c_str(), content.size(), &bytesWritten, nullptr);
            CloseHandle(fileHandle);

            return result != 0 && bytesWritten == content.size();
        }

    private:
        bool GetFileHandle(DWORD flag, HANDLE& handle) const
        {
            handle = CreateFileA(fileName.c_str(), GENERIC_READ | GENERIC_WRITE,
                0, nullptr, flag, FILE_ATTRIBUTE_NORMAL, nullptr);

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
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline bool CreateNewDirectory(const std::string& path, bool createAlways = false)
    {
        if (DirectoryExists(path) && !createAlways)
        {
            return true;
        }

        if (CreateDirectoryA(path.c_str(), nullptr) == 0) // Check if directory creation failed
        {
            DWORD error = GetLastError();
            // Optionally, log or handle error here
            return false;
        }

        return true;
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
