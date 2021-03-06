#ifndef POSTFILESTRATEGY_H_
#define POSTFILESTRATEGY_H_
#pragma once

#include <map>
#include <vector>
#include <string>

#include "httpstrategy.h"
#include "netlib.h"
#include "credentials.h"
#include "connection.h"
#include "chunkpost.h"
#include "fileinfo.h"
#include "filepost.h"

/* This strategy will begin the file chunking pipeline */
namespace attic {

class FileManager;

class PostFileStrategy : public HttpStrategyInterface {
public:
    typedef std::map<unsigned int, ChunkPost> ChunkPostList; // key, chunk group
private:
    bool RetrieveFileInfo(const std::string& filepath, FileInfo& out);

    int RetrieveChunkPosts(const std::string& entity,
                           const std::string& post_id,
                           ChunkPostList& out);

    int ChunkFile(const std::string& filepath,
                  const Credentials& file_credentials,
                  const std::string& file_post_id,
                  ChunkPostList& chunk_list,
                  FileInfo::ChunkMap& chunk_map);

    void ExtractChunkInfo(ChunkPostList& list,
                          FileInfo::ChunkMap& out);

    int RetrieveFilePost(const std::string& post_id, FilePost& out);
    int UpdateFilePost(FileInfo& fi);
    int UpdateFilePostTransitState(const std::string& post_id, bool in_transit);


    bool VerifyChunks(ChunkPost& cp, const std::string& filepath);

    bool UpdateFilePostVersion(const FileInfo* fi, const std::string& meta_post_id);

public:
    std::map<std::string, bool> verification_map_;
    PostFileStrategy() {}
    ~PostFileStrategy() {}

    int Execute(FileManager* fm, CredentialsManager* cm);


};

}//namespace
#endif

