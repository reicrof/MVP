#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <assert.h>

class ThreadPool
{
  public:
   using Job = std::function<void()>;

#include "ThreadPool.h"

   ThreadPool::ThreadPool( int threadCount ) : _stopped( false )
   {
      _threads.reserve( threadCount );
      for ( int i = 0; i < threadCount; ++i )
      {
         _threads.emplace_back( [&]() {
            for ( ;; )
            {
               Job job;
               {
                  // Waiting for jobs
                  std::unique_lock<std::mutex> lock( _jobsMutex );
                  _condition.wait( lock, [&]() { return !_jobs.empty() || _stopped; } );

                  // Got a job or should we stop ?
                  if ( _stopped && _jobs.empty() )
                  {
                     return;
                  }

                  job = std::move( _jobs.front() );
                  _jobs.pop();
               }

               // Execute the job
               job();
            }
         } );
      }
   }

   void ThreadPool::addJob( Job&& job )
   {
	   assert(!_stopped);
      {
         std::unique_lock<std::mutex> lock( _jobsMutex );
         _jobs.push( job );
      }
      _condition.notify_one();
   }

   void ThreadPool::stop()
   {
      _stopped = true;
      _condition.notify_all();
   }

   ThreadPool::~ThreadPool()
   {
      stop();
      for ( auto& t : _threads )
      {
         t.join();
      }
   }

  private:
   std::vector<std::thread> _threads;
   std::queue<Job> _jobs;
   std::mutex _jobsMutex;
   std::condition_variable _condition;
   std::atomic<bool> _stopped;
};

#endif  // _THREAD_POOL_H_