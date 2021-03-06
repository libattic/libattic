#ifndef TASKMANAGER_H_
#define TASKMANAGER_H_
#pragma once

#include <string>

#include "accesstoken.h"
#include "entity.h"
#include "taskfactory.h"
#include "event.h"
#include "task.h"
#include "mutexclass.h"

namespace attic { 

class TentApp;
class FileManager;
class CredentialsManager;
class TaskDelegate;

class TaskManager : public event::EventListener {
public:
    TaskManager(FileManager* pFm, 
                CredentialsManager* pCm,
                const AccessToken& at,
                const Entity& entity,
                const std::string& tempdir, 
                const std::string& workingdir,
                const std::string& configdir
               );

    ~TaskManager();

    void Dispatch();

    int Initialize();
    int Shutdown();

    virtual void OnEventRaised(const event::Event& event);

    virtual void OnTaskCreate(Task* t);
    virtual void OnTaskInsert(Task* t);

    void CreateFolder(const std::string& folderpath, 
            TaskDelegate* del);
    void RenameFolder(const std::string& original_folderpath, 
            const std::string& new_folderpath);
    void DeleteFolder(const std::string& folderpath, 
            TaskDelegate* del);

    // Sync Tasks
    void UploadFile(const std::string& filepath,        // Begins upload process
            TaskDelegate* del);        

    void UploadPublicFile(const std::string& filepath,
            TaskDelegate* del);

    void ProcessUploadFile(const std::string& postid,   // Begins actual file processing
            TaskDelegate* del);   

    void DownloadFile(const std::string& filepath, 
            TaskDelegate* del);

    void DeleteFile(const std::string& filepath, 
            TaskDelegate* del);

    void RenameFile(const std::string& original_filepath, 
            const std::string& new_filepath);

    void GetFileHistory(const std::string& filepath, TaskDelegate* del);

    void DownloadFileToDirectory(const std::string& post_id, 
            const std::string& version, 
            const std::string& filepath,
            TaskDelegate* del);

    void DeletePost(const std::string& post_id, 
            const std::string& version, 
            TaskDelegate* del);

    void MakePostNewHead(const std::string& post_id, 
            const std::string& version, 
            TaskDelegate* del);

    void SharePostWithEntity(const std::string& post_id, 
            const std::string& entity_url,
            TaskDelegate* del);

    // Service Tasks
    void QueryManifest(TaskDelegate* del);
    void CreatePostTree(const std::string& filepath, 
            TaskDelegate* del);
    void RetrieveEntityPublicKey(const std::string& entity_url,
            TaskDelegate* del);

    TaskContext CreateServiceContext(void);
    // Config Tasks
    void AddRootDirectory(const std::string& directory_path, 
            TaskDelegate* del);
    void UnlinkRootDirectory(const std::string& directory_path, 
            TaskDelegate* del);
    void RemoveRootDirectory(const std::string& directory_path, 
            TaskDelegate* del);

    // Info tasks
    int TaskCount(const Task::TaskType);

    void PushContextBack(TaskContext& tc);
    void RetrieveContextQueue(TaskContext::ContextQueue& out);

    FileManager* file_manager() { return file_manager_; }
    CredentialsManager* credentials_manager() { return credentials_manager_; }
    const AccessToken& access_token() const { return access_token_; }
    const Entity& entity() const { return entity_; }
    const std::string temp_directory() const { return temp_directory_; }
    const std::string working_directory() const { return working_directory_; }
    const std::string config_directory() const { return config_directory_; }


private:
    TaskFactory             task_factory_; // Local to upload manager

    FileManager*            file_manager_;
    CredentialsManager*     credentials_manager_;

    AccessToken             access_token_;
    Entity                  entity_;

    std::string             temp_directory_;
    std::string             working_directory_;
    std::string             config_directory_;

    MutexClass cxt_mtx;
    TaskContext::ContextQueue context_queue_; // hold queue
};

}//namespace
#endif

