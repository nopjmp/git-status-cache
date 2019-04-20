#pragma once
#include "Git.h"
#include "CacheStatistics.h"

#include <mutex>

/**
* Simple cache that retrieves and stores git status information.
* This class is thread-safe.
*/
class Cache
{
private:
	using LockGuard = std::lock_guard<std::mutex>;

	Git m_git;
	std::unordered_map<std::string, std::tuple<bool, Git::Status>> m_cache;
	std::mutex m_cacheMutex;

	std::atomic<uint64_t> m_cacheHits = 0;
	std::atomic<uint64_t> m_cacheMisses = 0;
	std::atomic<uint64_t> m_cacheEffectivePrimeRequests = 0;
	std::atomic<uint64_t> m_cacheTotalPrimeRequests = 0;
	std::atomic<uint64_t> m_cacheEffectiveInvalidationRequests = 0;
	std::atomic<uint64_t> m_cacheTotalInvalidationRequests = 0;
	std::atomic<uint64_t> m_cacheInvalidateAllRequests = 0;

public:
	Cache() = default;
	Cache(const Cache&) = delete;
	Cache(Cache&&) = default;

	/**
	* Retrieves current git status for repository at provided path.
	* Returns from cache if present, otherwise queries git and adds to cache.
	*/
	std::tuple<bool, Git::Status> GetStatus(const std::string& repositoryPath);

	/**
	* Computes status and loads cache entry if it's not already present.
	*/
	void PrimeCacheEntry(const std::string& repositoryPath);

	/**
	* Invalidates cached git status for repository at provided path.
	*/
	bool InvalidateCacheEntry(const std::string& repositoryPath);

	/**
	* Invalidates all cached git status information.
	*/
	void InvalidateAllCacheEntries();

	/**
	 * Returns information about cache's performance.
	 */
	CacheStatistics GetCacheStatistics();
};