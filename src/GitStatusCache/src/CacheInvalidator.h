#pragma once
#include "DirectoryMonitor.h"
#include "Cache.h"
#include "CachePrimer.h"

#include <filesystem>
#include <mutex>

/**
* Invalidates cache entries in response to file system changes.
* This class is thread-safe.
*/
class CacheInvalidator
{
private:
	using LockGuard = std::lock_guard<std::mutex>;

	std::shared_ptr<Cache> m_cache;
	CachePrimer m_cachePrimer;

	std::unique_ptr<DirectoryMonitor> m_directoryMonitor;
	std::unordered_map<DirectoryMonitor::Token, std::string> m_tokensToRepositories;
	std::mutex m_tokensToRepositoriesMutex;

	/**
	* Checks if the file change can be safely ignored.
	*/
	static bool ShouldIgnoreFileChange(const std::filesystem::path& path);

	/**
	* Handles file change notifications by invalidating cache entries and scheduling priming.
	*/
	void OnFileChanged(DirectoryMonitor::Token token, const std::filesystem::path& path, DirectoryMonitor::FileAction action);

public:
	CacheInvalidator(const std::shared_ptr<Cache>& cache);

	/**
	* Registers working directory and repository directory for file change monitoring.
	*/
	void MonitorRepositoryDirectories(const Git::Status& status);
};