#pragma once

#include "Git.h"
#include "DirectoryMonitor.h"
#include "StatusCache.h"

#include <chrono>
#include <shared_mutex>

// ignore warnings from nlohmann headers
#pragma warning(push, 0)
#pragma warning (disable : ALL_CODE_ANALYSIS_WARNINGS)
#include <nlohmann/json.hpp>
#pragma warning(pop)

/**
 * Services requests for git status information.
 */
class StatusController
{
private:
	using ReadLock = std::shared_lock<std::shared_mutex>;
	using WriteLock = std::unique_lock<std::shared_mutex>;

	uint64_t m_totalNanosecondsInGetStatus = 0;
	uint64_t m_minNanosecondsInGetStatus = UINT64_MAX;
	uint64_t m_maxNanosecondsInGetStatus = 0;
	uint64_t m_totalGetStatusCalls = 0;
	std::shared_mutex m_getStatusStatisticsMutex;

	Git m_git;
	StatusCache m_cache;
	UniqueHandle m_requestShutdown;

	/**
	 * Creates JSON response for errors.
	 */
	static std::string CreateErrorResponse(const std::string& request, std::string&& error, std::exception *e = nullptr);

	/**
	 * Records timing datapoint for GetStatus.
	 */
	void RecordGetStatusTime(uint64_t nanosecondsInGetStatus);

	/**
	* Retrieves current git status.
	*/
	std::string GetStatus(const nlohmann::json& document, const std::string& request);

	/**
	* Retrieves information about cache's performance.
	*/
	std::string GetCacheStatistics();

public:
	StatusController();
	StatusController(const StatusController&) = delete;
	~StatusController();

	/**
	* Deserializes request and returns serialized response.
	*/
	std::string StatusController::HandleRequest(const std::string& request);

	/**
	 * Shuts down the service.
	 */
	void StatusController::Shutdown();

	/**
	 * Blocks until shutdown request received.
	 */
	void WaitForShutdownRequest();
};