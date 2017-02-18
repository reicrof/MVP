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

   class JobQueue
   {
      std::queue<Job> _jobs;
      std::mutex _mutex;
      std::condition_variable _isReady;
      bool _stopped;

     public:
      JobQueue() : _stopped( false ) {}
      template <typename T>
      bool tryAddJob( std::shared_ptr<std::packaged_task<T> >&& job )
      {
         {
            std::unique_lock<std::mutex> lock( _mutex, std::try_to_lock );
            if ( !lock.owns_lock() )
               return false;
            _jobs.emplace( [job]() { ( *job )(); } );
         }
         _isReady.notify_one();
         return true;
      }

      // This will block the thread, waiting to add the job
      template <typename T>
      void addJob( std::shared_ptr<std::packaged_task<T> >&& job )
      {
         {
            std::unique_lock<std::mutex> lock( _mutex );
            _jobs.emplace( [job]() { ( *job )(); } );
         }
         _isReady.notify_one();
      }

      bool tryGetJob( Job& job )
      {
         std::unique_lock<std::mutex> lock( _mutex, std::try_to_lock );
         if ( !lock.owns_lock() || _jobs.empty() )
            return false;
         job = std::move( _jobs.front() );
         _jobs.pop();
         return true;
      }

      // This will block the thread, waiting for a job.
      bool getJob( Job& job )
      {
         std::unique_lock<std::mutex> lock( _mutex );
         while ( _jobs.empty() && !_stopped )
            _isReady.wait( lock );
         if ( _jobs.empty() )
            return false;
         job = std::move( _jobs.front() );
         _jobs.pop();
         return true;
      }

      void stop()
      {
         {
            std::unique_lock<std::mutex> lock( _mutex );
            _stopped = true;
         }
         _isReady.notify_all();
      }
   };


   std::vector<std::thread> _threads;
   std::vector<JobQueue> _queues;
   std::atomic<size_t> _queueIndex;
   size_t _queueCount;

  public:
   ThreadPool( size_t threadCount )
       : _queues( threadCount ), _queueIndex( 0 ), _queueCount( threadCount )
   {
      _threads.reserve( threadCount );
      for ( size_t i = 0; i < threadCount; ++i )
      {
         _threads.emplace_back( [this, threadCount, i]() {
            for ( ;; )
            {
               Job job;
               {
                  // Try to get a job from current queue. If it fails, steal
                  // a job from one of the other queue.
                  for ( size_t j = 0; j < ( threadCount * 32 ) && !job; ++j )
                  {
                     _queues[ ( i + j ) % threadCount ].tryGetJob( job );
                  }

                  // If stealing did not work either, wait until we have a
                  // job in our queue. If getJob fails, it means the thread
                  // is shutting down, so our work is done here.
                  if ( !job )
                  {
                     if ( !_queues[ i ].getJob( job ) )
                        return;
                  }

                  // We should have a job if we get here.
                  assert( job );
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
         std::make_shared<std::packaged_task<typename std::result_of<F( Args... )>::type()> >(
            std::bind( std::forward<F>( f ), std::forward<Args>( args )... ) );

      auto futureRes = jobTask->get_future();

      auto i = _queueIndex++;
      for ( size_t j = 0; j < _queueCount; ++j )
      {
         const size_t qIndex = ( i + j ) % _queueCount;
         if ( _queues[ qIndex ].tryAddJob( std::forward<decltype( jobTask )>( jobTask ) ) )
         {
            return futureRes;
         }
      }

      // Blocks until we have added the job
      _queues[ i % _queueCount ].addJob( std::forward<decltype( jobTask )>( jobTask ) );

      return futureRes;
   }

   void stop()
   {
      for ( auto& q : _queues )
      {
         q.stop();
      }
   }

   ~ThreadPool()
   {
      stop();
      for ( auto& t : _threads )
      {
         t.join();
      }
   }
};

#endif  // _THREAD_POOL_H_