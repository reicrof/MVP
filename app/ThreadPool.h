#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <memory>
#include <utility>
#include <assert.h>

class ThreadPool
{
  public:
   using Job = std::function<void()>;

#include "ThreadPool.h"

   ThreadPool( int threadCount ) : _stopped( false )
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

   template <class F, class... Args>
   auto addJob( F&& f, Args&&... args )
   {
      auto jobTask =
           std::make_shared< std::packaged_task<typename std::result_of<F( Args... )>::type()> >(
            std::bind( std::forward<F>( f ), std::forward<Args>( args )... ) );
      assert( !_stopped );
      auto futureRes = jobTask->get_future();
      {
         std::unique_lock<std::mutex> lock( _jobsMutex );
         _jobs.emplace( [jobTask]() { ( *jobTask )(); } );
      }
      _condition.notify_one();
      return futureRes;
   }

   void stop()
   {
      _stopped = true;
      _condition.notify_all();
   }

   ~ThreadPool()
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