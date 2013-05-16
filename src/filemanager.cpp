#include "filemanager.h"

#include <string>
#include <iostream>

#include "filesystem.h"
#include "utils.h"
#include "chunkinfo.h"
#include "folder.h"
#include "constants.h"
#include "logutils.h"

namespace attic {

FileManager::FileManager() : MutexClass() {}
FileManager::~FileManager() {}

int FileManager::Initialize(const std::string &manifestDirectory, 
                            const std::string &workingDirectory,
                            const std::string& tempDirectory) {
    // Set manifest path
    manifest_directory_ = manifestDirectory;
    manifest_.SetDirectory(manifest_directory_);

    // Set working directory
    working_directory_ = workingDirectory;
    temp_directory_ = tempDirectory;

    return manifest_.Initialize();
}

int FileManager::Shutdown() {
    return manifest_.Shutdown();
}

int FileManager::RemoveFile(const std::string &filepath) {
    int status = ret::A_OK;

    Lock();
    if(!manifest_.RemoveFileInfo(filepath))
        status = ret::A_FAIL_FILE_NOT_IN_MANIFEST;
    Unlock();

    return status;
}

void FileManager::GetRelativePath(const std::string& filepath, std::string& relative_out) {
    if(IsPathRelative(filepath)) {
        std::cout<<" Filepath : " << filepath << " ALREADY RELATIVE " << std::endl;
        relative_out = filepath;
        return;
    }

    std::string canonical, relative;
    if(!filepath.empty()) {
        fs::GetCanonicalPath(filepath, canonical);
        fs::MakePathRelative(working_directory_, canonical, relative);
    }
    relative_out = std::string(cnst::g_szWorkingPlaceHolder) + "/" + relative;
    std::cout<<" RELATIVE PATH : " << relative_out << std::endl;
}

void FileManager::ExtractRelativePaths(const FileInfo* pFi, 
                                       std::string& relative_out, 
                                       std::string& parent_relative_out) {
    std::string filepath = pFi->filepath();
    std::string relative, canonical;

    // If filepath is already relative
    if(IsPathRelative(filepath)) {
        // set canonical
        std::cout<<" PATH IS RELATIVE " << std::endl;
        int pos = filepath.find(cnst::g_szWorkingPlaceHolder);
        canonical = working_directory_ + filepath.substr((pos+strlen(cnst::g_szWorkingPlaceHolder)));
        std::cout<<" working : " << working_directory_ << std::endl;
        std::cout<<" CANONICAL IS : " << canonical << std::endl;
        relative = filepath;
    }
    else {
        fs::GetCanonicalPath(filepath, canonical);
        fs::MakePathRelative(working_directory_, canonical, relative);
        // TODO :: fix for windows
        relative = std::string(cnst::g_szWorkingPlaceHolder) + "/" + relative;

        std::cout<<" RELATIVE : " << relative << std::endl;
        //if(parent_relative.empty())
        if(canonical.empty()) {
            std::cout<<" FILEMANAGER - CANONICAL IS EMPTY - " << filepath << std::endl;
        }
    }
    // Extract parent relative :
    std::string filename = pFi->filename();
    int pos = relative.find(filename);
    std::string parent_relative;
    if(pos != std::string::npos)
        parent_relative = relative.substr(0, pos-1); // minus 1 to include /

    std::cout<<"INSERTING TO MANIFEST " << std::endl;
    std::cout<<" filepath        : " << filepath << std::endl;
    std::cout<<" canonical       : " << canonical << std::endl;
    std::cout<<" relative        : " << relative << std::endl;
    std::cout<<" parent_relative : " << parent_relative << std::endl;

    relative_out = relative;
    parent_relative_out = parent_relative;
}

void FileManager::InsertToManifest (FileInfo* pFi) { 
    if(!pFi) return;
    // Calculate relative path
    std::string relative, parent;
    ExtractRelativePaths(pFi, relative, parent);
    pFi->set_filepath(relative);
    Lock();
    // Insert into infotable
    manifest_.InsertFileInfo(*pFi);
    Unlock();
}

int FileManager::RenameFolder(const std::string& old_folderpath,
                              const std::string& new_folderpath) {
    int status = ret::A_OK;

    std::cout<<" OLD FOLDERPATH : " << old_folderpath << std::endl;
    std::cout<<" NEW FOLDERPATH : " << new_folderpath << std::endl;

    std::string alias_old, alias_new;
    if(!IsPathRelative(old_folderpath))
        GetAliasedFilepath(old_folderpath, alias_old);
    else
        alias_old = old_folderpath;

    if(!IsPathRelative(new_folderpath))
        GetAliasedFilepath(new_folderpath, alias_new);
    else
        alias_new = new_folderpath;

    std::cout<<" ALIAS OLD : " << alias_old << std::endl;
    std::cout<<" ALIAS NEW : " << alias_new << std::endl;

    Folder folder;
    if(GetFolderEntry(alias_old, folder)){
        if(!alias_new.empty()) {
            //Update folder path
            Lock();
            manifest_.UpdateFolderPath(folder.folder_post_id(), alias_new);
            Unlock();
            //Update folder contents
            Lock();
            bool ret = manifest_.UpdateAllFileInfoForFolder(folder.folder_post_id());
            Unlock();
        }
        else {
            status = ret::A_FAIL_INVALID_FOLDERPATH;
        }
    }
    else {
        status = ret::A_FAIL_FOLDER_NOT_IN_MANIFEST;
    }

    return status;
}

// Takes in two absolute (canonical) filepaths, and swaps them
int FileManager::RenameFile(const std::string& old_filepath, 
                            const std::string& new_filepath) {
    int status = ret::A_OK;
    std::cout<<" OLD FILEPATH : " << old_filepath << std::endl;
    std::cout<<" NEW FILEPATH : " << new_filepath << std::endl;

    std::string alias_old, alias_new;
    if(!IsPathRelative(old_filepath))
        GetAliasedFilepath(old_filepath, alias_old);
    else
        alias_old = old_filepath;

    if(!IsPathRelative(new_filepath))
        GetAliasedFilepath(new_filepath, alias_new);
    else
        alias_new = new_filepath;

    std::cout<<" ALIAS OLD : " << alias_old << std::endl;
    std::cout<<" ALIAS NEW : " << alias_new << std::endl;

    FileInfo* fi = GetFileInfo(alias_old);
    if(fi && !alias_new.empty()) {
        fi->PushBackAlias(alias_old);
        std::string alias_data;
        fi->GetSerializedAliasData(alias_data);

        std::string filename;
        utils::ExtractFileName(alias_new, filename);
        std::cout<<" SERIALIZED ALIAS DATA : " << alias_data << std::endl;

        Lock();
        bool s = manifest_.UpdateFilepath(alias_old, alias_new);
        if(s) s = manifest_.UpdateFilename(alias_new, filename);
        if(s) s = manifest_.UpdatePastAlias(alias_new, alias_data);
        Unlock();
        if(!s) {
            std::cout<<" FAILED TO UPDATE FILEAPTH " << std::endl;
            status = ret::A_FAIL_TO_QUERY_MANIFEST;
        }
    }
    else {
        status = ret::A_FAIL_FILE_NOT_IN_MANIFEST;
    }
    return status;
}

void FileManager::SetFileVersion(const std::string& filepath, const std::string& version) {
    if(IsPathRelative(filepath)) {
        Lock();
        manifest_.UpdateFileVersion(filepath, version);
        Unlock();
    }
}

void FileManager::SetFileDeleted(const std::string& filepath, const bool del) {
    if(IsPathRelative(filepath)) {
        Lock();
        manifest_.UpdateFileDeleted(filepath, del);
        Unlock();
    }
}

void FileManager::SetFilePostId(const std::string &filepath, const std::string& postid) {
    if(IsPathRelative(filepath)) {
        Lock();
        manifest_.UpdateFilePostID(filepath, postid);
        Unlock();
    }
    else {
        std::cout<<" FILEPATH NOT RELATIVE IN SET POST ID : " << filepath << std::endl;
    }
}

void FileManager::SetFileChunks(const std::string& filepath, FileInfo::ChunkMap& map) {
    FileInfo* fi = GetFileInfo(filepath);
    if(fi) {
        fi->set_chunks(map);
        InsertToManifest(fi);
    }
    else {
        std::cout<<" INVALID FILEINFO OBJECT " << std::endl;
    }
}

FileInfo* FileManager::CreateFileInfo() {
    Lock();
    FileInfo* pFi = file_info_factory_.CreateFileInfoObject();
    Unlock();

    return pFi;
}

bool FileManager::GetAliasedFilepath(const std::string& filepath, std::string& out) {
    size_t pos = filepath.find(working_directory_);
    if(pos != std::string::npos) {
        out = std::string(cnst::g_szWorkingPlaceHolder) + "/" + filepath.substr(pos+working_directory_.size());
        pos = out.find("//");
        if(pos != std::string::npos) {
            out.erase(pos, 1);
        }
    }
    return false;
}

bool FileManager::GetCanonicalFilepath(const std::string& relativepath, std::string& out) {
    std::string relative, canonical;
    std::cout<<" RELATIVE IN : " << relativepath << std::endl;
    if(IsPathRelative(relativepath)) {
        size_t pos = relativepath.find(cnst::g_szWorkingPlaceHolder);
        if(pos != std::string::npos) {
            out = working_directory_ + "/" + relativepath.substr(pos + strlen(cnst::g_szWorkingPlaceHolder) + 1);

            

            std::cout<<" OUT : " << out << std::endl;
            return true;
        }
        else {
            std::cout << " MALFORMED RELATIVE PATH " << relativepath << std::endl;
        }
    }
    else {
        std::cout<< " PATH NOT RELATIVE " << std::endl;
    }

    return false;
}

bool FileManager::IsPathRelative(const std::string& filepath) {
    if(filepath.find(cnst::g_szWorkingPlaceHolder) != std::string::npos)
        return true;
    return false;
}

bool FileManager::DoesFileExist(const std::string& filepath) {
    bool stat = false;
    if(IsPathRelative(filepath)) {
        Lock();
        stat = manifest_.IsFileInManifest(filepath);
        Unlock();
    }
    else {
        std::cout<<" FILEPATH PASSED NOT RELATIVE : "<< filepath << std::endl;
    }

    return stat;
}

bool FileManager::AttemptToGetRelativePath(const std::string& filepath, std::string& out) {
    bool retval = false;

    if(filepath.find(working_directory_) != std::string::npos) {
        std::string relative;
        fs::MakePathRelative(working_directory_, filepath, relative);
        out = std::string(cnst::g_szWorkingPlaceHolder) + "/" + relative;
        retval = true;
        return retval;
    }

    std::cout<<" attempting to get a relative path " << std::endl;
    std::cout<<" for : " << filepath << std::endl;
    std::string rel;
    fs::MakePathRelative(working_directory_, filepath, rel);
    std::cout<<" rel : " << rel << std::endl;
    std::string canonical;
    int pos = 0;
    int last = 0;
    while(pos != std::string::npos) {
        last = pos;
        pos = filepath.find("/", pos + 1);
    }

    if(last) {
        fs::GetCanonicalPath(filepath.substr(0, pos-1), canonical);
        std::cout<<"can?onical? : " << canonical << std::endl;
        //canonical += filepath.substr(pos+1);
    }
    else {
        std::cout<<" getting canonical " << std::endl;
        fs::GetCanonicalPath(filepath, canonical);
    }
    std::cout<<" canonical : " << canonical << std::endl;

    if(canonical.find(working_directory_) != std::string::npos) {
        std::string relative, working;
        Lock();
        working = working_directory_;
        Unlock();
        
        fs::MakePathRelative(working, canonical, relative);
        out = std::string(cnst::g_szWorkingPlaceHolder) + "/" + relative;
        retval = true;
    }
    else {
        std::cout<< " could not find working directory within canonical path " << std::endl;
    }
    std::cout<< " RELATIVE OUT : " << out << std::endl;
    return retval;
} 




FileInfo* FileManager::GetFileInfoByPostId(const std::string& post_id) {
    FileInfo* fi = NULL;

    if(!post_id.empty()) {
        Lock();
        fi = file_info_factory_.CreateFileInfoObject();
        manifest_.QueryForFileByPostId(post_id, *fi);
        Unlock();
    }

    return fi;
}

FileInfo* FileManager::GetFileInfo(const std::string &filepath) {
    FileInfo* pFi = NULL;
    std::string relative;
    if(!IsPathRelative(filepath)) {
        AttemptToGetRelativePath(filepath, relative);
    } 
    else
        relative = filepath;

    // Attempt to get relative path
    if(IsPathRelative(relative)) {
        Lock();
        pFi = file_info_factory_.CreateFileInfoObject();
        manifest_.QueryForFile(relative, *pFi);
        Unlock();

        if(pFi) {
            if(!pFi->IsValid()) {
                pFi = NULL;
            }
        }
        else {
            std::cout<<" NULL FileInfo " << std::endl;
        }
    }
    else {
        std::cout<<"GETFILEINFO FILEPATH PASSED NOT RELATIVE : "<< filepath << std::endl;
    }

    return pFi;
}


bool FileManager::GetFolderEntryByPostId(const std::string& post_id, Folder& folder) {
    bool ret = false;
    if(!post_id.empty()) {
        Lock();
        ret = manifest_.QueryForFolderByPostId(post_id, folder);
        Unlock();
    }
    return ret;
}

// Pass in absoulte folder path
bool FileManager::GetFolderEntry(const std::string& folderpath, Folder& folder) {
    // Check if folder exists
    //  - if it does, fill out class
    //  - else return false
    
    std::string relative;
    if(!IsPathRelative(folderpath)) {
        // Make Relative
        //GetRelativePath(folderpath, relative);
        GetAliasedFilepath(folderpath, relative);
    }
    else {
        relative = folderpath;
    }

    if(relative.empty())
        relative = cnst::g_szWorkingPlaceHolder;

    std::cout<<" Searching for folder entry : " << relative << std::endl;

    Lock();
    bool ret = manifest_.QueryForFolder(relative, folder);
    Unlock();

    return ret;
}

bool FileManager::GetFolderPostId(const std::string& folderpath, std::string& id_out) { 
    Folder folder;
    bool ret = GetFolderEntry(folderpath, folder);
    id_out = folder.folder_post_id();

    return ret;
}

bool FileManager::CreateFolderEntry(const std::string& folderpath, 
                                    const std::string& folder_post_id,
                                    Folder& out) {
    // Create Folder Entry
    std::string relative;
    if(!IsPathRelative(folderpath)) {
        GetRelativePath(folderpath, relative);
    }
    else {
        relative = folderpath;
    }

    if(relative.empty())
        relative = cnst::g_szWorkingPlaceHolder;

    bool ret = false;
    Lock();
    ret = manifest_.InsertFolderInfo(relative, folder_post_id, "");
    if(ret)
        ret = manifest_.QueryForFolder(relative, out);
    Unlock();

    return ret;
}

bool FileManager::UpdateFolderEntry(FolderPost& fp) {
    std::cout<<" FOLDER PATH : " << fp.folder().folderpath() << std::endl;
    std::cout<<" FOLDER POST ID : " << fp.id() << std::endl;

    bool ret = false;
    Lock();
    if(manifest_.IsFolderInManifest(fp.folder().folderpath())) {
        std::string id;
        manifest_.GetFolderPostID(fp.folder().folderpath(), id);
        if(id.empty()) {
            // update id
            ret = manifest_.UpdateFolderPostId(fp.folder().folderpath(), fp.id());
        }
        else if(id != fp.id()) {
            // throw error
            std::string error = " LOCAL CACHE FOLDER POST ID MISMATCH \n";
            error += "\tpost id : ";
            error += fp.id();
            error += "\n";
            error += "\t conficted id : ";
            error += id;
            error += "\n";
            error += "folderpath : ";
            error += fp.folder().folderpath();
            log::LogString("MASDlf", error);
        }
        else {
            // Already ok
            ret = true;
        }
    }
    else {
        // create entry
        Folder folder;
        ret = CreateFolderEntry(fp.folder().folderpath(), fp.id(), folder);
    }
    Unlock();
    return ret;
}



bool FileManager::UpdateFolderContents(Folder& folder) {
    bool ret = false;
    Lock();
    ret = manifest_.UpdateAllFileInfoForFolder(folder.folder_post_id());
    Unlock();

    return ret;
}

bool FileManager::SetFolderPostId(const std::string& folderpath, const std::string& post_id) {
    std::string relative;
    if(!IsPathRelative(folderpath)) {
        GetRelativePath(folderpath, relative);
    }
    else {
        relative = folderpath;
    }

    if(relative.empty())
        relative = cnst::g_szWorkingPlaceHolder;

    std::cout<<" FOLDER PATH : " << folderpath << std::endl;
    std::cout<<" SETTING FOLDER POST ID : " << relative << std::endl;

    bool ret = false;
    Lock();
    if(manifest_.IsFolderInManifest(relative))
        ret = manifest_.UpdateFolderPostId(relative, post_id);
    Unlock();
 
    return ret;
}

int FileManager::GetAllFileInfo(std::deque<FileInfo>& out) {
    Lock();
    int status = manifest_.QueryAllFiles(out);
    Unlock();
    return status;
}

int FileManager::GetAllFileInfoForFolder(const std::string& folderid, std::deque<FileInfo>& out) {
    Lock();
    int status = manifest_.QueryAllFilesForFolder(folderid, out);
    Unlock();
    return status;
}

bool FileManager::LockFile(const std::string& filepath) {
    return file_queue_.LockFile(filepath);
}

bool FileManager::UnlockFile(const std::string& filepath) {
    return file_queue_.UnlockFile(filepath);
}

bool FileManager::IsFileLocked(const std::string& filepath) {
    return file_queue_.IsFileLocked(filepath);
}


}//namespace
