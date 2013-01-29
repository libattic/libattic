#ifndef CHUNKINFO_H_
#define CHUNKINFO_H_
#pragma once

#include <iostream>
#include <string>

#include "jsonserializable.h"

class ChunkInfo : public JsonSerializable
{
public:
    ChunkInfo();
    ChunkInfo(const std::string& chunkName, const std::string& checkSum);
    ~ChunkInfo();

    void Serialize(Json::Value& root);
    void Deserialize(Json::Value& root);

    void GetChunkName(std::string& out) const       { out = m_ChunkName; }
    void GetChecksum(std::string& out) const        { out = m_CheckSum; }
    void GetPlainTextMac(std::string& out) const    { out = m_PlainTextMac; }
    void GetCipherTextMac(std::string& out) const   { out = m_CipherTextMac; }
    void GetIv(std::string& out) const              { out = m_Iv; }

    void SetChunkName(const std::string& name)      { m_ChunkName = name; } 
    void SetChecksum(const std::string& sum)        { m_CheckSum = sum; }
    void SetPlainTextMac(const std::string& mac)    { m_PlainTextMac = mac; } 
    void SetCipherTextMac(const std::string& mac)   { m_CipherTextMac = mac; }
    void SetIv(const std::string& iv)               { m_Iv = iv; }

    bool HasIv() const { return !m_Iv.empty(); }

private:
    std::string m_ChunkName;        // Name of this particular chunk
    std::string m_CheckSum;         // Hash of the chunk before encryption
    std::string m_PlainTextMac;     // Hash of the chunk
    std::string m_CipherTextMac;    // Hash of the Iv
    std::string m_Iv;               // Iv used to encrypt chunk
};

#endif

