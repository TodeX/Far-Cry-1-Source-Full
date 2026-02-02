#include "StdAfx.h"
#include "JobManager.h"
#include <ISystem.h>

#if defined(WIN32) || defined(WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#endif

CJobManager::CJobManager()
{
    m_hJobAvailable = NULL;
    m_hAllJobsCompleted = NULL;
    m_bShutdown = false;
    m_nJobsPending = 0;
}

CJobManager::~CJobManager()
{
    ShutDown();
}

bool CJobManager::Init(int nThreads)
{
#if defined(WIN32) || defined(WIN64)
    // Manual-reset, initially unsignaled
    m_hJobAvailable = CreateEvent(NULL, TRUE, FALSE, NULL);
    // Manual-reset, initially signaled (0 jobs)
    m_hAllJobsCompleted = CreateEvent(NULL, TRUE, TRUE, NULL);

    if (!m_hJobAvailable || !m_hAllJobsCompleted)
        return false;

    m_bShutdown = false;

    // Determine number of threads if nThreads <= 0
    if (nThreads <= 0)
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        nThreads = sysInfo.dwNumberOfProcessors;
        if (nThreads < 1) nThreads = 1;
    }

    for (int i = 0; i < nThreads; i++)
    {
        DWORD dwThreadId;
        THREAD_HANDLE hThread = CreateThread(NULL, 0, WorkerThreadEntry, this, 0, &dwThreadId);
        if (hThread)
        {
            m_threads.push_back(hThread);
        }
    }

    return !m_threads.empty();
#else
    return false; // Not implemented for non-Windows yet
#endif
}

void CJobManager::ShutDown()
{
    m_bShutdown = true;

#if defined(WIN32) || defined(WIN64)
    if (m_hJobAvailable)
    {
        // Signal event to wake up all threads (Manual Reset)
        SetEvent((HANDLE)m_hJobAvailable);
    }

    for (size_t i = 0; i < m_threads.size(); i++)
    {
        if (m_threads[i])
        {
            WaitForSingleObject((HANDLE)m_threads[i], 1000); // Wait 1 sec
            CloseHandle((HANDLE)m_threads[i]);
        }
    }
    m_threads.clear();

    if (m_hJobAvailable)
    {
        CloseHandle((HANDLE)m_hJobAvailable);
        m_hJobAvailable = NULL;
    }
    if (m_hAllJobsCompleted)
    {
        CloseHandle((HANDLE)m_hAllJobsCompleted);
        m_hAllJobsCompleted = NULL;
    }
#endif
}

void CJobManager::AddJob(JobFunc pFunc, void* pData)
{
    AUTO_LOCK(m_csJobQueue);

    if (m_nJobsPending == 0)
    {
#if defined(WIN32) || defined(WIN64)
        ResetEvent((HANDLE)m_hAllJobsCompleted);
#endif
    }

    m_jobQueue.push_back(Job(pFunc, pData));
    m_nJobsPending++;

#if defined(WIN32) || defined(WIN64)
    SetEvent((HANDLE)m_hJobAvailable);
#endif
}

void CJobManager::WaitForAllJobs()
{
    while (true)
    {
        m_csJobQueue.Lock();
        if (m_nJobsPending == 0)
        {
            m_csJobQueue.Unlock();
            return;
        }

        if (!m_jobQueue.empty())
        {
            // Steal a job
            Job job = m_jobQueue.front();
            m_jobQueue.pop_front();

            if (m_jobQueue.empty())
            {
#if defined(WIN32) || defined(WIN64)
                ResetEvent((HANDLE)m_hJobAvailable);
#endif
            }
            m_csJobQueue.Unlock();

            // Execute job
            if (job.pFunc)
            {
                job.pFunc(job.pData);
            }

            // Decrement pending
            m_csJobQueue.Lock();
            m_nJobsPending--;
            if (m_nJobsPending == 0)
            {
#if defined(WIN32) || defined(WIN64)
                SetEvent((HANDLE)m_hAllJobsCompleted);
#endif
            }
            m_csJobQueue.Unlock();
        }
        else
        {
            m_csJobQueue.Unlock();
#if defined(WIN32) || defined(WIN64)
            WaitForSingleObject((HANDLE)m_hAllJobsCompleted, INFINITE);
#else
            return;
#endif
        }
    }
}

DWORD WINAPI CJobManager::WorkerThreadEntry(void* pParam)
{
    CJobManager* pMgr = (CJobManager*)pParam;
    pMgr->WorkerThread();
    return 0;
}

void CJobManager::WorkerThread()
{
#if defined(WIN32) || defined(WIN64)
    while (true)
    {
        WaitForSingleObject((HANDLE)m_hJobAvailable, INFINITE);

        m_csJobQueue.Lock();
        if (m_bShutdown)
        {
            m_csJobQueue.Unlock();
            break;
        }

        if (m_jobQueue.empty())
        {
            ResetEvent((HANDLE)m_hJobAvailable);
            m_csJobQueue.Unlock();
            continue;
        }

        Job job = m_jobQueue.front();
        m_jobQueue.pop_front();

        if (m_jobQueue.empty())
        {
            ResetEvent((HANDLE)m_hJobAvailable);
        }

        m_csJobQueue.Unlock();

        // Execute job
        if (job.pFunc)
        {
            job.pFunc(job.pData);
        }

        m_csJobQueue.Lock();
        m_nJobsPending--;
        if (m_nJobsPending == 0)
        {
            SetEvent((HANDLE)m_hAllJobsCompleted);
        }
        m_csJobQueue.Unlock();
    }
#endif
}
