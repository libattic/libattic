#ifndef CHUNKPOST_H_
#define CHUNKPOST_H_
#pragma once

#include <string>
#include <vector>
#include <map>

#include "post.h"

class ChunkInfo;

class ChunkPost : public Post
{
public:
    ChunkPost();
    ~ChunkPost();

    virtual void Serialize(Json::Value& root);  
    virtual void Deserialize(Json::Value& root);

    int SetChunkInfoList(std::map<std::string, ChunkInfo>& pList);

    int GetChunkSize() { m_ChunkList.size(); }
    std::vector<ChunkInfo>* GetChunkList() { return &m_ChunkList; }

private:
    std::vector<ChunkInfo>  m_ChunkList;

};


#endif
