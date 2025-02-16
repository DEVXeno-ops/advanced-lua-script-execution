#pragma once

#include <string>
#include <Windows.h>
#include <memory>

namespace win32
{
    class File
    {
    private:
        std::string fileName;

    public:
        ~File() = default;
        File(const std::string& fileName) : fileName(fileName) {}

    public:
        bool Read(std::string& content)
        {
            HANDLE fileHandle = INVALID_HANDLE_VALUE; // Initialize handle.

            if (!this->GetFileHandle(OPEN_EXISTING, fileHandle))
            {
                return false; // Return false if handle is invalid.
            }

            const DWORD fileSize = GetFileSize(fileHandle, nullptr); // Get file size.

            if (fileSize == INVALID_FILE_SIZE) // Check for errors in getting file size.
            {
                CloseHandle(fileHandle);
                return false;
            }

            std::vector<char> buffer(fileSize); // Use vector for automatic memory management.

            DWORD bytesRead = 0;
            const BOOL result = ReadFile(fileHandle, buffer.data(), fileSize, &bytesRead, nullptr);

            CloseHandle(fileHandle); // Close the file handle.

            if (result == 0 || bytesRead != fileSize) // Check if reading was successful.
            {
                return false;
            }

            content.assign(buffer.begin(), buffer.end()); // Assign buffer to string.

            return true;
        }

        bool Write(const std::string& content)
        {
            HANDLE fileHandle = INVALID_HANDLE_VALUE; // Initialize handle.

            if (!this->GetFileHandle(CREATE_ALWAYS, fileHandle))
            {
                return false; // Return false if handle is invalid.
            }

            DWORD bytesWritten = 0;
            const BOOL result = WriteFile(fileHandle, content.c_str(), content.size(), &bytesWritten, NULL);

            CloseHandle(fileHandle); // Close the file handle.

            return result != 0 && bytesWritten == content.size(); // Ensure the write was successful.
        }

    private:
        bool GetFileHandle(DWORD flag, HANDLE& handle)
        {
            handle = CreateFileA(this->fileName.c_str(), GENERIC_READ | GENERIC_WRITE,
                0, nullptr, flag, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (handle == INVALID_HANDLE_VALUE) // Check if handle is valid.
            {
                return false; // Return false if the handle is invalid.
            }

            return true; // Handle is valid.
        }
    };

    inline bool FileExists(const std::string& path)
    {
        const DWORD attributes = GetFileAttributesA(path.c_str()); // Get file attributes.

        return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY); // Check if it is a file.
    }

    inline bool DirectoryExists(const std::string& path)
    {
        const DWORD attributes = GetFileAttributesA(path.c_str()); // Get directory attributes.

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY); // Check if it is a directory.
    }

    inline bool CreateNewDirectory(const std::string& path, bool createAlways = false)
    {
        if (DirectoryExists(path) && !createAlways) // Check if directory exists and if it should not be created.
        {
            return true; // Return true since the directory is already present.
        }

        return CreateDirectoryA(path.c_str(), nullptr) != 0; // Try to create the directory.
    }

    inline void delay(int milliseconds)
    {
        Sleep(static_cast<DWORD>(milliseconds)); // Sleep for specified milliseconds.
    }

    inline void clear()
    {
        system("cls"); // Clear the console.
    }

    inline void pause()
    {
        system("pause"); // Pause the system (wait for user input).
    }
}
