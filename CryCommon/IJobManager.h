#ifndef _IJOBMANAGER_H_
#define _IJOBMANAGER_H_

#pragma once

// Job function prototype
typedef void (*JobFunc)(void* pData);

struct Job
{
    JobFunc pFunc;
    void*   pData;

    Job() : pFunc(0), pData(0) {}
    Job(JobFunc f, void* d) : pFunc(f), pData(d) {}
};

struct IJobManager
{
    virtual ~IJobManager() {}

    // Initialize the job manager with a number of threads
    // virtual bool Init(int nThreads) = 0; // Usually called by System

    // Add a job to the queue
    virtual void AddJob(JobFunc pFunc, void* pData) = 0;

    // Wait for all current jobs to finish
    // This is a simple synchronization barrier
    virtual void WaitForAllJobs() = 0;
};

#endif // _IJOBMANAGER_H_
