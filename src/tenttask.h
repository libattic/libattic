
#ifndef TENTTASK_H_
#define TENTTASK_H_
#pragma once

#include <string>

#include "tentapp.h"
#include "task.h"

#include "filemanager.h"
#include "connectionmanager.h"
#include "credentialsmanager.h"

class TentTask : public Task
{

public:
    TentTask( TentApp* pApp, 
              FileManager* pFm, 
              ConnectionManager* pCon, 
              CredentialsManager* pCm,
              const AccessToken& at,
              const std::string& entity,
              const std::string& filepath,
              const std::string& tempdir, 
              const std::string& workingdir,
              const std::string& configdir,
              void (*callback)(int, void*))
    {
        m_pTentApp = pApp;
        m_pFileManager = pFm;
        m_pConnectionManager = pCon; 
        m_pCredentialsManager = pCm;

        m_At = at;

        m_Entity = entity;
        m_Filepath = filepath;
        m_TempDirectory = tempdir;
        m_WorkingDirectory = workingdir;
        m_ConfigDirectory = configdir;

        mCallback = *callback;
    }

    virtual void SetFinishedState()                                         
    {                                                                       
        Task::SetFinishedState();
        if(mCallback)                                              
            mCallback(ret::A_OK, NULL);                            
    }                                                                       

    virtual ~TentTask()
    {

    }

    /*
    virtual void RunTask() {}
    */

    AccessToken* GetAccessToken() { return &m_At; }
    
    void GetEntity(std::string &out)            { out = m_Entity; }
    void GetFilepath(std::string &out)          { out = m_Filepath; }
    void GetTempDirectory(std::string &out)     { out = m_TempDirectory; } 
    void GetWorkingDirectory(std::string &out)  { out = m_WorkingDirectory; }
    void GetConfigDirectory(std::string &out)   { out = m_ConfigDirectory; }

    TentApp* GetTentApp()                       { return m_pTentApp; }
    FileManager* GetFileManager()               { return m_pFileManager; } 
    ConnectionManager* GetConnectionManager()   { return m_pConnectionManager; } 
    CredentialsManager* GetCredentialsManager() { return m_pCredentialsManager; } 

    void SetTentApp(TentApp* pApp)                        { m_pTentApp = pApp; }
    void SetFileManager(FileManager* pFm)                 { m_pFileManager = pFm; }
    void SetConnectionManager(ConnectionManager* pCon)    { m_pConnectionManager = pCon; }
    void SetCredentialsManager(CredentialsManager* pCm)   { m_pCredentialsManager = pCm; }
    void SetAccessToken(const AccessToken& at)                  { m_At = at; }
    void SetEntity(const std::string& entity)                   { m_Entity = entity; }
    void SetFilepath(const std::string& filepath)               { m_Filepath = filepath; }
    void SetTempDirectory(const std::string& tempdir)           { m_TempDirectory = tempdir; }
    void SetWorkingDirectory(const std::string& workingdir)     { m_WorkingDirectory = workingdir; }
    void SetConfigDirectory(const std::string& configdir)       { m_ConfigDirectory = configdir; }
    void SetCallback(void (*cb)(int, void*))                    { mCallback = cb; }


    void Reset()
    {
        m_pTentApp = NULL;
        m_pFileManager = NULL;
        m_pConnectionManager = NULL;
        m_pCredentialsManager = NULL;

        m_At.Reset();

        m_Entity.clear();
        m_Filepath.clear();
        m_TempDirectory.clear();
        m_WorkingDirectory.clear();
        m_ConfigDirectory.clear();

        mCallback = NULL;
    }

protected:
    void Callback(int code, void* p)
    {
        std::cout<<" Callback " << std::endl;
        if(mCallback)
            mCallback(code, p); 
    }

private:
    AccessToken          m_At;

    std::string          m_Entity;
    std::string          m_Filepath;
    std::string          m_TempDirectory;
    std::string          m_WorkingDirectory;
    std::string          m_ConfigDirectory;

    TentApp*             m_pTentApp; 
    FileManager*         m_pFileManager;
    ConnectionManager*   m_pConnectionManager;
    CredentialsManager*  m_pCredentialsManager;

    void (*mCallback)(int, void*);
};

#endif

