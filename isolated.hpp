#pragma once
#include <algorithm>
#include <vector>
#include <string>

namespace isolated
{
    inline std::vector<uint8_t> isoBytes =
    {
        // "local magic_code = [=====[fivem_latest_marker]=====]"
        0x6C, 0x6F, 0x63, 0x61, 0x6C, 0x20, 0x6D, 0x61, 0x67, 0x69, 0x63, 0x5F, 0x63, 0x6F,
        0x64, 0x65, 0x20, 0x3D, 0x20,
        0x5B, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, // 5x '='
        0x5B, // '['
        // "fivem_latest_marker"
        0x66, 0x69, 0x76, 0x65, 0x6D, 0x5F, 0x6C, 0x61, 0x74, 0x65, 0x73, 0x74, 0x5F, 0x6D, 0x61, 0x72, 0x6B, 0x65, 0x72,
        0x5D, // ']'
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, // 5x '='
        0x5D  // ']'
    };

    inline void replaceByteSequence(std::vector<uint8_t>& byteVec, const std::vector<uint8_t>& oldSeq, const std::vector<uint8_t>& newSeq)
    {
        if (oldSeq.empty() || byteVec.size() < oldSeq.size())
            return;

        auto it = byteVec.begin();
        while ((it = std::search(it, byteVec.end(), oldSeq.begin(), oldSeq.end())) != byteVec.end())
        {
            it = byteVec.erase(it, it + oldSeq.size());
            it = byteVec.insert(it, newSeq.begin(), newSeq.end());
            // ไม่ต้อง advance iterator เพิ่ม
        }
    }
}
