#include "stdafx.h"
#include "Cache.h"

std::tuple<bool, Git::Status> Cache::GetStatus(const std::string& repositoryPath)
{
	{
		LockGuard lock(m_cacheMutex);
		auto cacheEntry = m_cache.find(repositoryPath);
		if (cacheEntry != m_cache.end())
		{
			++m_cacheHits;
			//Log("Cache.GetStatus.CacheHit", Severity::Info)
			//	<< R"(Found git status in cache. { "repositoryPath": ")" << repositoryPath << R"(" })";
			return cacheEntry->second;
		}
	}

	++m_cacheMisses;
	//Log("Cache.GetStatus.CacheMiss", Severity::Warning)
	//	<< R"(Failed to find git status in cache. { "repositoryPath": ")" << repositoryPath << R"(" })";

	auto status = m_git.GetStatus(repositoryPath);
	{
		LockGuard lock(m_cacheMutex);
		m_cache[repositoryPath] = status;
	}

	return status;
}

void Cache::PrimeCacheEntry(const std::string& repositoryPath)
{
	++m_cacheTotalPrimeRequests;
	{
		LockGuard lock(m_cacheMutex);
		auto cacheEntry = m_cache.find(repositoryPath);
		if (cacheEntry != m_cache.end())
			return;
	}

	++m_cacheEffectivePrimeRequests;
	//Log("Cache.PrimeCacheEntry", Severity::Info)
	//	<< R"(Priming cache entry. { "repositoryPath": ")" << repositoryPath << R"(" })";

	auto status = m_git.GetStatus(repositoryPath);

	{
		LockGuard lock(m_cacheMutex);
		m_cache[repositoryPath] = status;
	}
}

bool Cache::InvalidateCacheEntry(const std::string& repositoryPath)
{
	++m_cacheTotalInvalidationRequests;
	bool invalidatedCacheEntry = false;
	{
		LockGuard lock(m_cacheMutex);
		auto cacheEntry = m_cache.find(repositoryPath);
		if (cacheEntry != m_cache.end())
		{
			cacheEntry = m_cache.find(repositoryPath);
			if (cacheEntry != m_cache.end())
			{
				m_cache.erase(cacheEntry);
				invalidatedCacheEntry = true;
			}
		}
	}

	if (invalidatedCacheEntry)
		++m_cacheEffectiveInvalidationRequests;
	return invalidatedCacheEntry;
}

void Cache::InvalidateAllCacheEntries()
{
	++m_cacheInvalidateAllRequests;
	{
		LockGuard lock(m_cacheMutex);
		m_cache.clear();
	}

	//Log("Cache.InvalidateAllCacheEntries.", Severity::Warning)
	//	<< R"(Invalidated all git status information in cache.)";
}

CacheStatistics Cache::GetCacheStatistics()
{
	CacheStatistics statistics;
	statistics.CacheHits = m_cacheHits;
	statistics.CacheMisses = m_cacheMisses;
	statistics.CacheEffectivePrimeRequests = m_cacheEffectivePrimeRequests;
	statistics.CacheTotalPrimeRequests = m_cacheTotalPrimeRequests;
	statistics.CacheEffectiveInvalidationRequests = m_cacheEffectiveInvalidationRequests;
	statistics.CacheTotalInvalidationRequests = m_cacheTotalInvalidationRequests;
	statistics.CacheInvalidateAllRequests = m_cacheInvalidateAllRequests;
	return statistics;
}
