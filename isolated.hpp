#pragma once
#include <algorithm>
#include <vector>
#include <string>
#include <span>
#include <stdexcept>

namespace isolated
{
    // Byte sequence representing the Lua string literal: "local magic_code = [=====[fivem_latest_marker]=====]"
    inline const std::vector<uint8_t> isoBytes = {
        0x6C, 0x6F, 0x63, 0x61, 0x6C, 0x20, 0x6D, 0x61, 0x67, 0x69, 0x63, 0x5F, 0x63, 0x6F,
        0x64, 0x65, 0x20, 0x3D, 0x20,
        0x5B, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, // 5x '='
        0x5B, // '['
        0x66, 0x69, 0x76, 0x65, 0x6D, 0x5F, 0x6C, 0x61, 0x74, 0x65, 0x73, 0x74, 0x5F, 0x6D, 0x61, 0x72, 0x6B, 0x65, 0x72,
        0x5D, // ']'
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, // 5x '='
        0x5D  // ']'
    };

    // Replaces all occurrences of oldSeq with newSeq in byteVec
    // Returns the number of replacements made or throws on error
    inline size_t replaceByteSequence(std::vector<uint8_t>& byteVec,
                                     std::span<const uint8_t> oldSeq,
                                     std::span<const uint8_t> newSeq,
                                     std::string* error = nullptr)
    {
        if (oldSeq.empty())
        {
            if (error) *error = "Old sequence is empty";
            throw std::invalid_argument("Old sequence cannot be empty");
        }
        if (byteVec.size() < oldSeq.size())
        {
            if (error) *error = "Input vector is too small for old sequence";
            return 0;
        }
        if (newSeq.empty() && !byteVec.empty())
        {
            if (error) *error = "New sequence is empty";
            throw std::invalid_argument("New sequence cannot be empty when replacing");
        }

        size_t replacements = 0;
        // Reserve space to minimize reallocations (estimate based on worst case)
        byteVec.reserve(byteVec.size() + (newSeq.size() - oldSeq.size()) * (byteVec.size() / oldSeq.size()));

        auto it = byteVec.begin();
        while ((it = std::search(it, byteVec.end(), oldSeq.begin(), oldSeq.end())) != byteVec.end())
        {
            it = byteVec.erase(it, it + oldSeq.size());
            it = byteVec.insert(it, newSeq.begin(), newSeq.end());
            ++replacements;
            ++it; // Advance past the inserted sequence
        }

        return replacements;
    }

    // Utility to process Lua scripts from a byte vector (e.g., from pIni::Archive or fx::Resource)
    inline bool processLuaScript(std::vector<uint8_t>& scriptBytes,
                                const std::vector<uint8_t>& marker = isoBytes,
                                const std::vector<uint8_t>& replacement = {},
                                std::string* error = nullptr)
    {
        try
        {
            size_t replacements = replaceByteSequence(scriptBytes, marker, replacement, error);
            return replacements > 0;
        }
        catch (const std::exception& e)
        {
            if (error) *error = e.what();
            return false;
        }
    }
}