#pragma once

#include "Git.h"
#include "DirectoryMonitor.h"
#include "StatusCache.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <chrono>
#include <shared_mutex>

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
	* Adds named string to JSON response.
	*/
	static void AddStringToJson(rapidjson::Writer<rapidjson::StringBuffer>& writer, std::string&& name, std::string&& value);

	/**
	* Adds bool to JSON response.
	*/
	static void AddBoolToJson(rapidjson::Writer<rapidjson::StringBuffer>& writer, std::string&& name, bool value);

	/**
	* Adds uint32_t to JSON response.
	*/
	static void AddUintToJson(rapidjson::Writer<rapidjson::StringBuffer>& writer, std::string&& name, uint32_t value);

	/**
	* Adds uint64_t to JSON response.
	*/
	static void AddUint64ToJson(rapidjson::Writer<rapidjson::StringBuffer>& writer, std::string&& name, uint64_t value);

	/**
	* Adds double to JSON response.
	*/
	static void AddDoubleToJson(rapidjson::Writer<rapidjson::StringBuffer>& writer, std::string&& name, double value);

	/**
	* Adds vector of strings to JSON response.
	*/
	static void AddArrayToJson(rapidjson::Writer<rapidjson::StringBuffer>& writer, std::string&& name, const std::vector<std::string>& value);

	/**
	 * Adds version to JSON response.
	 */
	static void AddVersionToJson(rapidjson::Writer<rapidjson::StringBuffer>& writer);

	/**
	 * Creates JSON response for errors.
	 */
	static std::string CreateErrorResponse(const std::string& request, std::string&& error);

	/**
	 * Records timing datapoint for GetStatus.
	 */
	void RecordGetStatusTime(uint64_t nanosecondsInGetStatus);

	/**
	* Retrieves current git status.
	*/
	std::string GetStatus(const rapidjson::Document& document, const std::string& request);

	/**
	* Retrieves information about cache's performance.
	*/
	std::string GetCacheStatistics();

	/**
	 * Shuts down the service.
	 */
	std::string StatusController::Shutdown();

public:
	StatusController();
	StatusController(const StatusController&) = delete;
	~StatusController();

	/**
	* Deserializes request and returns serialized response.
	*/
	std::string StatusController::HandleRequest(const std::string& request);

	/**
	 * Blocks until shutdown request received.
	 */
	void WaitForShutdownRequest();
};