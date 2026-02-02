#ifndef _JOBMANAGER_H_
#define _JOBMANAGER_H_

#pragma once

#include <platform.h>
#include <IJobManager.h>
#include "CritSection.h"
#include <deque>
#include <vector>

class CJobManager : public IJobManager
{
public:
    CJobManager();
    virtual ~CJobManager();

    bool Init(int nThreads);
    void ShutDown();

    // IJobManager interface
    virtual void AddJob(JobFunc pFunc, void* pData);
    virtual void WaitForAllJobs();

private:
    void WorkerThread();
    static DWORD WINAPI WorkerThreadEntry(void* pParam);

private:
    std::deque<Job> m_jobQueue;
    CCritSection    m_csJobQueue;

    std::vector<THREAD_HANDLE> m_threads;

    EVENT_HANDLE m_hJobAvailable;
    bool         m_bShutdown;

    int             m_nJobsPending; // Jobs in queue + jobs running
    EVENT_HANDLE    m_hAllJobsCompleted;
};

#endif // _JOBMANAGER_H_
