#pragma once

#include <string>
#include <Windows.h>
#include <memory>
#include <vector>
#include <iostream>
#include <stdexcept>

namespace win32
{
    // RAII wrapper for HANDLE
    class HandleGuard
    {
    public:
        explicit HandleGuard(HANDLE h) : handle(h) {}
        ~HandleGuard() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        HandleGuard(const HandleGuard&) = delete;
        HandleGuard& operator=(const HandleGuard&) = delete;
        HANDLE get() const noexcept { return handle; }
    private:
        HANDLE handle;
    };

    class File
    {
    private:
        std::wstring fileName;

    public:
        explicit File(const std::wstring& fileName) : fileName(fileName) {}
        ~File() = default;

        bool Read(std::wstring& content, std::string* error = nullptr) const noexcept
        {
            HandleGuard fileHandle(INVALID_HANDLE_VALUE);
            if (!GetFileHandleForRead(fileHandle))
            {
                if (error) *error = "Failed to open file for reading: " + GetLastErrorAsString();
                return false;
            }

            LARGE_INTEGER fileSize;
            if (!GetFileSizeEx(fileHandle.get(), &fileSize) || fileSize.QuadPart == 0)
            {
                if (error) *error = "Failed to get file size or file is empty: " + GetLastErrorAsString();
                return false;
            }

            // Limit to avoid excessive memory usage
            constexpr size_t maxFileSize = 1ULL << 30; // 1GB limit
            if (fileSize.QuadPart > maxFileSize)
            {
                if (error) *error = "File size exceeds maximum limit of 1GB";
                return false;
            }

            std::vector<wchar_t> buffer(static_cast<size_t>(fileSize.QuadPart));
            DWORD bytesRead = 0;
            const BOOL result = ReadFile(fileHandle.get(), buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr);

            if (result == 0 || bytesRead != fileSize.QuadPart)
            {
                if (error) *error = "Failed to read file: " + GetLastErrorAsString();
                return false;
            }

            content.assign(buffer.begin(), buffer.end());
            return true;
        }

        bool Write(const std::wstring& content, std::string* error = nullptr) const noexcept
        {
            HandleGuard fileHandle(INVALID_HANDLE_VALUE);
            if (!GetFileHandleForWrite(fileHandle))
            {
                if (error) *error = "Failed to open file for writing: " + GetLastErrorAsString();
                return false;
            }

            DWORD bytesWritten = 0;
            const BOOL result = WriteFile(fileHandle.get(), content.c_str(), static_cast<DWORD>(content.size() * sizeof(wchar_t)), &bytesWritten, nullptr);

            if (result == 0 || bytesWritten != content.size() * sizeof(wchar_t))
            {
                if (error) *error = "Failed to write file: " + GetLastErrorAsString();
                return false;
            }

            return true;
        }

    private:
        bool GetFileHandleForRead(HandleGuard& handle) const noexcept
        {
            HANDLE h = CreateFileW(
                fileName.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );
            handle = HandleGuard(h);
            return h != INVALID_HANDLE_VALUE;
        }

        bool GetFileHandleForWrite(HandleGuard& handle) const noexcept
        {
            HANDLE h = CreateFileW(
                fileName.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );
            handle = HandleGuard(h);
            return h != INVALID_HANDLE_VALUE;
        }

        static std::string GetLastErrorAsString()
        {
            DWORD error = GetLastError();
            if (error == 0) return "No error";

            LPWSTR buffer = nullptr;
            size_t size = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPWSTR>(&buffer),
                0,
                nullptr
            );

            std::wstring message(buffer, size);
            LocalFree(buffer);

            // Convert wstring to string for simplicity
            std::string result;
            for (wchar_t c : message)
            {
                result += static_cast<char>(c);
            }
            return result;
        }
    };

    inline bool FileExists(const std::wstring& path) noexcept
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline bool DirectoryExists(const std::wstring& path) noexcept
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline bool CreateNewDirectory(const std::wstring& path) noexcept
    {
        if (DirectoryExists(path))
        {
            return true;
        }

        if (CreateDirectoryW(path.c_str(), nullptr))
        {
            return true;
        }

        DWORD err = GetLastError();
        return err == ERROR_ALREADY_EXISTS && DirectoryExists(path);
    }

    inline void delay(int milliseconds) noexcept
    {
        Sleep(static_cast<DWORD>(milliseconds));
    }

    inline void clear() noexcept
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

        DWORD written;
        DWORD size = csbi.dwSize.X * csbi.dwSize.Y;
        FillConsoleOutputCharacterW(hConsole, L' ', size, {0, 0}, &written);
        FillConsoleOutputAttribute(hConsole, csbi.wAttributes, size, {0, 0}, &written);
        SetConsoleCursorPosition(hConsole, {0, 0});
    }

    inline void pause() noexcept
    {
        std::wcout << L"Press any key to continue . . .\n";
        std::wcin.get();
    }
}