#ifndef RENAMESTRATEGY_H_
#define RENAMESTRATEGY_H_
#pragma once

#include <string>
#include "httpstrategy.h"
#include "filepost.h"
#include "folderpost.h"

namespace attic { 

class FileManager;
class FileInfo;

class RenameStrategy : public HttpStrategyInterface {
    int RenameFile();
    int RenameFolder();

    bool RetrieveFolder(const std::string& folderpath, Folder& out);
    int RetrieveFolderPost(const std::string& post_id, FolderPost& fp);
    int RetrieveFilePost(const std::string& post_id, FilePost& fp);

    int UpdateFileMetaData(const std::string& old_filepath, 
                           const std::string& new_filepath);

    int UpdateFileMetaPost(const std::string& post_id, const FilePost& fp);
    int UpdateFolderMetaPost(const std::string& post_id, const FolderPost& fp); 

    bool HaveCommonWorkingDir(const std::string& old_folderpath, 
                              const std::string& new_folderpath, 
                              std::string& directory_out,
                              std::string& post_id_out);
public:
    RenameStrategy();
    ~RenameStrategy();

    int Execute(FileManager* pFileManager, CredentialsManager* pCredentialsManager);
};

} //namespace
#endif

