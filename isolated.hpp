#pragma once
#include <algorithm>
#include <vector>
#include <string>

namespace isolated
{
    inline std::vector<uint8_t> isoBytes =
    {
        0x6C, 0x6F, 0x63, 0x61, 0x6C, 0x20, 0x6D, 0x61, 0x67, 0x69, 0x63, 0x5F, 0x63, 0x6F,
        0x64, 0x65, 0x20, 0x3D, 0x20, 0x5B, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D,
        0x3D, 0x3D, 0x3D, 0x3D, 0x5B, 0x72, 0x65, 0x70, 0x6C, 0x7A, 0x5D, 0x3D,
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x5D
    };

    // Replace bytes in the vector
    inline void replaceByteSequence(std::vector<uint8_t>& byteVec, const std::vector<uint8_t>& oldSeq, const std::vector<uint8_t>& newSeq)
    {
        if (oldSeq.empty()) return; // Avoid infinite loop on empty sequence

        auto it = byteVec.begin();
        while ((it = std::search(it, byteVec.end(), oldSeq.begin(), oldSeq.end())) != byteVec.end())
        {
            it = byteVec.erase(it, it + oldSeq.size()); // Remove old sequence
            it = byteVec.insert(it, newSeq.begin(), newSeq.end()); // Insert new sequence
            std::advance(it, newSeq.size()); // Move iterator forward
        }
    }
}
