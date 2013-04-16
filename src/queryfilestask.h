#ifndef QUERYFILESTASK_H_
#define QUERYFILESTASK_H_
#pragma once

#include "manifesttask.h"
#include "fileinfo.h"

namespace attic { 

class QueryFilesTask : public ManifestTask {
    int CreateCStringListsAndCallBack(std::vector<FileInfo>& vec);
public:
    QueryFilesTask(FileManager* pFm,
                   const TaskContext& context,
                   void (*callback)(int, char**, int, int));

    ~QueryFilesTask();

    virtual void OnStart() { } 
    virtual void OnPaused() { } 
    virtual void OnFinished() { }

    void RunTask();
private:
    int m_Stride;
};

}//namespace
#endif

