#ifndef FILEQUEUE_H_
#define FILEQUEUE_H_
#pragma once

#include <map>
#include <string>

#include "mutexclass.h"
#include "eventsystem.h"

class FileQueue
{
public:
    FileQueue() {}
    ~FileQueue() {}

    virtual bool LockFile(const std::string& filepath)
    {
        if(!m_FileQueue[filepath]) {
            m_FileQueue[filepath] = true;
            return true;
        }
        return false;
    }

    virtual bool UnlockFile(const std::string& filepath)
    {
        if(m_FileQueue[filepath]) {
            m_FileQueue[filepath] = false;
            return true;
        }
        return false;
    }

private:
    // filepath, locked
    std::map<std::string, bool> m_FileQueue;
};

class CentralFileQueue : public MutexClass
{
    CentralFileQueue() {}
    CentralFileQueue(const CentralFileQueue& rhs) {}
    CentralFileQueue operator=(const CentralFileQueue& rhs) { return *this; }
public:
    ~CentralFileQueue() {}

    bool LockFile(const std::string& filepath) {
        bool ret = false;
        Lock();
        ret = m_FileQueue.LockFile(filepath);
        event::RaiseEvent(Event::FILE_LOCK, filepath, NULL);
        Unlock();
        return ret;
    }

    bool UnlockFile(const std::string& filepath) {
        bool ret = false;
        Lock();
        ret = m_FileQueue.UnlockFile(filepath);
        event::RaiseEvent(Event::FILE_UNLOCK, filepath, NULL);
        Unlock();
        return ret;
    }

    static CentralFileQueue* GetInstance() {
        if(!m_pInstance)
            m_pInstance = new CentralFileQueue();
        return m_pInstance;
    }

private:
    FileQueue m_FileQueue;
    static CentralFileQueue* m_pInstance;
};

#endif


