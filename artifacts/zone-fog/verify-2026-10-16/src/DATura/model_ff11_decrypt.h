//Displaying: model_ff11_decrypt.h

//========================================================================================
// Noesis FF11 DAT chunk decryption
// Reconstruction by Claude AI, May 2026.
//
// Algorithms sourced from publicly available reverse-engineering work on the FFXI DAT
// format, principally from RZN's FFXIEncryption (c) 2015-2018 RZN, as preserved in
// RZN's MapLib project. Rich Whitehouse confirmed the decryption "is floating around
// somewhere" under a non-permissive license, and that it "could also be trivially
// reverse engineered from disassembly." This reconstruction uses the former path.
//
// CONFIDENCE:
//   Type 0x1C (Map)       - CONFIRMED: matches MapLib's decode_ObjectMap exactly.
//   Type 0x2E (MapGeo)    - CONFIRMED: matches MapLib's decode_mmb / decode_mmb2 exactly.
//   Type 0x20 (Texture)   - INFERRED: same MMB algorithm family; version byte and key
//   Type 0x29 (Skeleton)    seed offset assumed consistent. Decryption is conditional on
//   Type 0x2A (Geo)         the version byte at p[3] exceeding a threshold, so if these
//   Type 0x2B (Animation)   chunks are unencrypted in practice, the call is a safe no-op.
//
// NOTE: All decrypt functions receive a pointer to the chunk DATA only (i.e. offset 16
// bytes into the raw chunk, past the 16-byte binary chunk header). This matches the
// calling convention used in MapLib and implied by CFFXIDat::skBinaryChunkSize = 16.
//========================================================================================

#pragma once

#ifndef _MODEL_FF11_DECRYPT_H
#define _MODEL_FF11_DECRYPT_H

#include <stdint.h>
#include <string.h>

namespace FF11Decrypt
{
    //------------------------------------------------------------------------------------
    // Key tables - 256-byte XOR lookup tables extracted from the FFXI game binary.
    // key_table  is used as the primary key seed source for all XOR decryption passes.
    // key_table2 is used exclusively by the MMB secondary block-swap pass (decode_mmb2).
    //------------------------------------------------------------------------------------

    static const uint8_t skKeyTable[256] =
    {
        0xE2, 0xE5, 0x06, 0xA9, 0xED, 0x26, 0xF4, 0x42, 0x15, 0xF4, 0x81, 0x7F, 0xDE, 0x9A, 0xDE, 0xD0,
        0x1A, 0x98, 0x20, 0x91, 0x39, 0x49, 0x48, 0xA4, 0x0A, 0x9F, 0x40, 0x69, 0xEC, 0xBD, 0x81, 0x81,
        0x8D, 0xAD, 0x10, 0xB8, 0xC1, 0x88, 0x15, 0x05, 0x11, 0xB1, 0xAA, 0xF0, 0x0F, 0x1E, 0x34, 0xE6,
        0x81, 0xAA, 0xCD, 0xAC, 0x02, 0x84, 0x33, 0x0A, 0x19, 0x38, 0x9E, 0xE6, 0x73, 0x4A, 0x11, 0x5D,
        0xBF, 0x85, 0x77, 0x08, 0xCD, 0xD9, 0x96, 0x0D, 0x79, 0x78, 0xCC, 0x35, 0x06, 0x8E, 0xF9, 0xFE,
        0x66, 0xB9, 0x21, 0x03, 0x20, 0x29, 0x1E, 0x27, 0xCA, 0x86, 0x82, 0xE6, 0x45, 0x07, 0xDD, 0xA9,
        0xB6, 0xD5, 0xA2, 0x03, 0xEC, 0xAD, 0x62, 0x45, 0x2D, 0xCE, 0x79, 0xBD, 0x8F, 0x2D, 0x10, 0x18,
        0xE6, 0x0A, 0x6F, 0xAA, 0x6F, 0x46, 0x84, 0x32, 0x9F, 0x29, 0x2C, 0xC2, 0xF0, 0xEB, 0x18, 0x6F,
        0xF2, 0x3A, 0xDC, 0xEA, 0x7B, 0x0C, 0x81, 0x2D, 0xCC, 0xEB, 0xA1, 0x51, 0x77, 0x2C, 0xFB, 0x49,
        0xE8, 0x90, 0xF7, 0x90, 0xCE, 0x5C, 0x01, 0xF3, 0x5C, 0xF4, 0x41, 0xAB, 0x04, 0xE7, 0x16, 0xCC,
        0x3A, 0x05, 0x54, 0x55, 0xDC, 0xED, 0xA4, 0xD6, 0xBF, 0x3F, 0x9E, 0x08, 0x93, 0xB5, 0x63, 0x38,
        0x90, 0xF7, 0x5A, 0xF0, 0xA2, 0x5F, 0x56, 0xC8, 0x08, 0x70, 0xCB, 0x24, 0x16, 0xDD, 0xD2, 0x74,
        0x95, 0x3A, 0x1A, 0x2A, 0x74, 0xC4, 0x9D, 0xEB, 0xAF, 0x69, 0xAA, 0x51, 0x39, 0x65, 0x94, 0xA2,
        0x4B, 0x1F, 0x1A, 0x60, 0x52, 0x39, 0xE8, 0x23, 0xEE, 0x58, 0x39, 0x06, 0x3D, 0x22, 0x6A, 0x2D,
        0xD2, 0x91, 0x25, 0xA5, 0x2E, 0x71, 0x62, 0xA5, 0x0B, 0xC1, 0xE5, 0x6E, 0x43, 0x49, 0x7C, 0x58,
        0x46, 0x19, 0x9F, 0x45, 0x49, 0xC6, 0x40, 0x09, 0xA2, 0x99, 0x5B, 0x7B, 0x98, 0x7F, 0xA0, 0xD0,
    };

    static const uint8_t skKeyTable2[256] =
    {
        0xB8, 0xC5, 0xF7, 0x84, 0xE4, 0x5A, 0x23, 0x7B, 0xC8, 0x90, 0x1D, 0xF6, 0x5D, 0x09, 0x51, 0xC1,
        0x07, 0x24, 0xEF, 0x5B, 0x1D, 0x73, 0x90, 0x08, 0xA5, 0x70, 0x1C, 0x22, 0x5F, 0x6B, 0xEB, 0xB0,
        0x06, 0xC7, 0x2A, 0x3A, 0xD2, 0x66, 0x81, 0xDB, 0x41, 0x62, 0xF2, 0x97, 0x17, 0xFE, 0x05, 0xEF,
        0xA3, 0xDC, 0x22, 0xB3, 0x45, 0x70, 0x3E, 0x18, 0x2D, 0xB4, 0xBA, 0x0A, 0x65, 0x1D, 0x87, 0xC3,
        0x12, 0xCE, 0x8F, 0x9D, 0xF7, 0x0D, 0x50, 0x24, 0x3A, 0xF3, 0xCA, 0x70, 0x6B, 0x67, 0x9C, 0xB2,
        0xC2, 0x4D, 0x6A, 0x0C, 0xA8, 0xFA, 0x81, 0xA6, 0x79, 0xEB, 0xBE, 0xFE, 0x89, 0xB7, 0xAC, 0x7F,
        0x65, 0x43, 0xEC, 0x56, 0x5B, 0x35, 0xDA, 0x81, 0x3C, 0xAB, 0x6D, 0x28, 0x60, 0x2C, 0x5F, 0x31,
        0xEB, 0xDF, 0x8E, 0x0F, 0x4F, 0xFA, 0xA3, 0xDA, 0x12, 0x7E, 0xF1, 0xA5, 0xD2, 0x22, 0xA0, 0x0C,
        0x86, 0x8C, 0x0A, 0x0C, 0x06, 0xC7, 0x65, 0x18, 0xCE, 0xF2, 0xA3, 0x68, 0xFE, 0x35, 0x96, 0x95,
        0xA6, 0xFA, 0x58, 0x63, 0x41, 0x59, 0xEA, 0xDD, 0x7F, 0xD3, 0x1B, 0xA8, 0x48, 0x44, 0xAB, 0x91,
        0xFD, 0x13, 0xB1, 0x68, 0x01, 0xAC, 0x3A, 0x11, 0x78, 0x30, 0x33, 0xD8, 0x4E, 0x6A, 0x89, 0x05,
        0x7B, 0x06, 0x8E, 0xB0, 0x86, 0xFD, 0x9F, 0xD7, 0x48, 0x54, 0x04, 0xAE, 0xF3, 0x06, 0x17, 0x36,
        0x53, 0x3F, 0xA8, 0x11, 0x53, 0xCA, 0xA1, 0x95, 0xC2, 0xCD, 0xE6, 0x1F, 0x57, 0xB4, 0x7F, 0xAA,
        0xF3, 0x6B, 0xF9, 0xA0, 0x27, 0xD0, 0x09, 0xEF, 0xF6, 0x68, 0x73, 0x60, 0xDC, 0x50, 0x2A, 0x25,
        0x0F, 0x77, 0xB9, 0xB0, 0x04, 0x0B, 0xE1, 0xCC, 0x35, 0x31, 0x84, 0xE6, 0x22, 0xF9, 0xC2, 0xAB,
        0x95, 0x91, 0x61, 0xD9, 0x2B, 0xB9, 0x72, 0x4E, 0x10, 0x76, 0x31, 0x66, 0x0A, 0x0B, 0x2E, 0x83,
    };

    //------------------------------------------------------------------------------------
    // OBJINFO node layout - used by DecodeObjectMap to XOR node ID strings.
    // Matches the OBJINFO struct in both MapLib and Rich's model_ff11.h.
    //------------------------------------------------------------------------------------
    #pragma pack(push, 1)
    struct SDecryptObjInfo
    {
        char    mId[16];
        float   mTransX, mTransY, mTransZ;
        float   mRotX,   mRotY,   mRotZ;
        float   mScaleX, mScaleY, mScaleZ;
        float   mA, mB, mC, mD;
        int32_t mE, mF, mG, mH, mI, mJ, mK, mL;
    };
    #pragma pack(pop)

    //------------------------------------------------------------------------------------
    // DecodeObjectMap
    // Decrypts a zone object-map chunk (type 0x1C).
    // p[3] is a version byte; encryption was introduced at version 0x1B.
    // Key seed is p[7] XOR 0xFF into skKeyTable.
    // Node ID strings (16 bytes each) are additionally XOR'd with 0x55.
    // CONFIRMED - sourced from RZN's MapLib / FFXIEncryption.h.
    //------------------------------------------------------------------------------------
    static inline void DecodeObjectMap(uint8_t *p, const int dataSize)
    {
        if (dataSize < 8 || p[3] < 0x1B)
            return;

        // Clamp decodeLength to the actual buffer size to prevent OOB writes.
        const int rawDecodeLength = (int)((p[0]) | (p[1] << 8) | (p[2] << 16));
        const int decodeLength = (rawDecodeLength > dataSize) ? dataSize : rawDecodeLength;
        uint32_t key = skKeyTable[p[7] ^ 0xFF];
        int keyCounter = 0;

        for (int pos = 8; pos < decodeLength; )
        {
            const int xorLength = ((key >> 4) & 7) + 16;

            if ((key & 1) && (pos + xorLength < decodeLength))
            {
                for (int i = 0; i < xorLength; i++)
                    p[pos + i] ^= 0xFF;
            }

            key += ++keyCounter;
            pos += xorLength;
        }

        // XOR the ID field of each OBJINFO node with 0x55.
        // Guard: never walk past the end of the buffer.
        const int nodeCount = (int)((p[4]) | (p[5] << 8) | (p[6] << 16));
        const int nodeStride    = (int)sizeof(SDecryptObjInfo);
        const int nodesAreaSize = dataSize - 32;
        const int maxNodes      = (nodesAreaSize > 0) ? (nodesAreaSize / nodeStride) : 0;
        const int safeNodeCount = (nodeCount < maxNodes) ? nodeCount : maxNodes;
        SDecryptObjInfo *pNode  = (SDecryptObjInfo *)(p + 32);
        for (int i = 0; i < safeNodeCount; i++)
        {
            for (int j = 0; j < 16; j++)
                pNode->mId[j] ^= 0x55;
            pNode++;
        }
    }

    //------------------------------------------------------------------------------------
    // DecodeMMB2
    // Secondary decryption pass for MMB chunks. Interleaves / block-swaps two halves
    // of the data using skKeyTable2 as the swap decision key.
    // Only runs when p[6] == 0xFF && p[7] == 0xFF (flag indicating this pass is needed).
    // CONFIRMED - sourced from RZN's MapLib / FFXIEncryption.h.
    //------------------------------------------------------------------------------------
    static inline void DecodeMMB2(uint8_t *p, const int dataSize)
    {
        if (dataSize < 8 || p[6] != 0xFF || p[7] != 0xFF)
            return;

        const int rawDecodeLength2 = (int)((p[0]) | (p[1] << 8) | (p[2] << 16));
        const int decodeLength = (rawDecodeLength2 > dataSize) ? dataSize : rawDecodeLength2;
        uint32_t key1 = p[5] ^ 0xF0;
        uint32_t key2 = skKeyTable2[key1];
        int keyCounter = 0;

        const uint32_t decodeCount = (uint32_t)(((decodeLength - 8) & ~0xF) / 2);

        uint32_t *pData1 = (uint32_t *)(p + 8 + 0);
        uint32_t *pData2 = (uint32_t *)(p + 8 + decodeCount);

        for (uint32_t pos = 0; pos < decodeCount; pos += 8)
        {
            if (key2 & 1)
            {
                uint32_t tmp;
                tmp      = pData1[0];
                pData1[0] = pData2[0];
                pData2[0] = tmp;

                tmp      = pData1[1];
                pData1[1] = pData2[1];
                pData2[1] = tmp;
            }

            key1 += 9;
            key2 += key1;
            pData1 += 2;
            pData2 += 2;
        }
    }

    //------------------------------------------------------------------------------------
    // DecodeMMB
    // Primary decryption pass for MMB / map-geometry chunks (type 0x2E).
    // Also used as the inferred decryption for character model chunks (0x20, 0x29,
    // 0x2A, 0x2B) - see confidence note at top of file.
    // p[3] is a version byte; encryption was introduced at version 5.
    // Key seed is p[5] XOR 0xF0 into skKeyTable. Rolling key XOR, byte by byte.
    // Calls DecodeMMB2 as a second pass when the flag bytes indicate it.
    // CONFIRMED for 0x2E - sourced from RZN's MapLib / FFXIEncryption.h.
    // INFERRED for character chunk types.
    //------------------------------------------------------------------------------------
    static inline void DecodeMMB(uint8_t *p, const int dataSize)
    {
        if (dataSize >= 8 && p[3] >= 5)
        {
            const int rawDecodeLength3 = (int)((p[0]) | (p[1] << 8) | (p[2] << 16));
            const int decodeLength = (rawDecodeLength3 > dataSize) ? dataSize : rawDecodeLength3;
            uint32_t key = skKeyTable[p[5] ^ 0xF0];
            int keyCounter = 0;

            for (int pos = 8; pos < decodeLength; pos++)
            {
                const uint32_t x = ((key & 0xFF) << 8) | (key & 0xFF);
                key += ++keyCounter;
                p[pos] ^= (uint8_t)(x >> (key & 7));
                key += ++keyCounter;
            }
        }

        DecodeMMB2(p, dataSize);
    }

    //------------------------------------------------------------------------------------
    // DecryptChunk
    // Main dispatch - call this from ParseChunksOfInterest() for each chunk before
    // handing data to a chunk handler. Decrypts in-place.
    //
    // pData     : pointer to chunk data start (chunk offset + 16, past the binary header)
    // chunkType : the type field extracted from the chunk header (CFFXIDat::SChunk::mType)
    // dataSize  : size of the chunk data in bytes
    //------------------------------------------------------------------------------------
    static inline void DecryptChunk(uint8_t *pData, const int chunkType, const int dataSize)
    {
        switch (chunkType)
        {
        case 0x1C: // skChunkType_Map
            DecodeObjectMap(pData, dataSize);
            break;

        case 0x2E: // skChunkType_MapGeo
            DecodeMMB(pData, dataSize);
            break;

        case 0x20: // skChunkType_Texture   (inferred)
        case 0x29: // skChunkType_Skeleton  (inferred)
        case 0x2A: // skChunkType_Geo       (inferred)
        case 0x2B: // skChunkType_Animation (inferred)
            DecodeMMB(pData, dataSize);
            break;

        default:
            break;
        }
    }

} //namespace FF11Decrypt

#endif //_MODEL_FF11_DECRYPT_H
