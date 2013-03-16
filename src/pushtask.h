#ifndef PUSHTASK_H_
#define PUSHTASK_H_
#pragma once

#include <string>
#include <vector>

#include "tenttask.h"

class TaskDelegate;

class PushTask : public TentTask {
    
    int PushFile(const std::string& filepath);

public:
    PushTask(TentApp* pApp, 
             FileManager* pFm, 
             CredentialsManager* pCm,
             TaskArbiter* pTa,
             TaskFactory* pTf,
             const AccessToken& at,
             const Entity& entity,
             const std::string& filepath,
             const std::string& tempdir, 
             const std::string& workingdir,
             const std::string& configdir,
             TaskDelegate* callbackDelegate);

    ~PushTask();

    virtual void OnStart() {} 
    virtual void OnPaused() {} 
    virtual void OnFinished() {}

    int GetUploadSpeed();

    void RunTask();

private:
};

#endif

