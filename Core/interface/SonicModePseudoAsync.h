#ifndef SonicCMS_Core_SonicModePseudoAsync
#define SonicCMS_Core_SonicModePseudoAsync

#include "FWCore/Concurrency/interface/WaitingTaskWithArenaHolder.h"

#include <memory>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>

//pretend to be async + non-blocking by waiting for blocking calls to return in separate std::thread
class SonicModePseudoAsync {
	public:
		//constructor
		SonicModePseudoAsync() : hasCall_(false), stop_(false) {
			thread_ = std::make_unique<std::thread>([this](){ waitForNext(); });
		}
		//destructor
		virtual ~SonicModePseudoAsync() {
			{
				std::lock_guard<std::mutex> guard(mutex_);
				stop_ = true;
			}
			cond_.notify_one();
			if(thread_){
				thread_->join();
				thread_.reset();
			}
		}
		//accessor
		void predict(edm::WaitingTaskWithArenaHolder holder) {
			//do all read/writes inside lock to ensure cache synchronization
			{
				std::lock_guard<std::mutex> guard(mutex_);
				holder_ = std::move(holder);
			
				//activate thread to wait for response, and return
				hasCall_ = true;
			}
			cond_.notify_one();
		}
		
	protected:
		//like sync mode, pseudo-async doesn't pass holder
		virtual void predictImpl() = 0;
	
		void waitForNext() {
			while(true){
				//wait for condition
				{
					std::unique_lock<std::mutex> lk(mutex_);
					cond_.wait(lk, [this](){return (hasCall_ or stop_);});
					if(stop_) break;

					//do everything inside lock
					predictImpl();
					
					//pseudo-async calls holder at the end (inside std::thread)
					hasCall_ = false;
					holder_.doneWaiting(exceptionPtr);
				}
			}
		}
	
		//members
		edm::WaitingTaskWithArenaHolder holder_;
		bool hasCall_;
		std::mutex mutex_;
		std::condition_variable cond_;
		std::atomic<bool> stop_;
		std::unique_ptr<std::thread> thread_;
};

#endif

