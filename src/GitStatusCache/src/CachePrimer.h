#pragma once
#include "Cache.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_set>

/**
* Actively updates invalidated cache entries to reduce cache misses on client requests.
* This class is thread-safe.
*/
class CachePrimer
{
private:
	using LockGuard = std::lock_guard<std::mutex>;

	std::shared_ptr<Cache> m_cache;

	UniqueHandle m_stopPrimingThread;
	std::thread m_primingThread;
	std::chrono::time_point<std::chrono::steady_clock> m_deadline;
	std::unordered_set<std::string> m_repositoriesToPrime;
	std::mutex m_primingMutex;

	/**
	* Primes cache by computing status for scheduled repositories.
	*/
	void Prime();

	/**
	* Reserves thread for priming operations until cache shuts down.
	*/
	void WaitForPrimingTimerExpiration();

public:
	CachePrimer(const std::shared_ptr<Cache>& cache);
	CachePrimer(const CachePrimer&) = delete;
	~CachePrimer();

	/**
	* Cancels any currently scheduled priming and reschedules for five seconds in the future.
	* Called repeatedly on file changes to refresh status for repositories five seconds after
	* a wave file change events (ex. a build) subsides.
	*/
	void SchedulePrimingForRepositoryPath(const std::string& repositoryPath);
};