#include "stdafx.h"
#include "StatusController.h"

#include <cstring>
#ifdef _MSC_VER 
//not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

constexpr uint32_t VERSION = 1;

StatusController::StatusController()
	: m_requestShutdown(MakeUniqueHandle(INVALID_HANDLE_VALUE))
{
	auto requestShutdown = ::CreateEvent(
		nullptr /*lpEventAttributes*/,
		true    /*manualReset*/,
		false   /*bInitialState*/,
		nullptr /*lpName*/);
	if (requestShutdown == nullptr)
	{
		//Log("StatusController.Constructor.CreateEventFailed", Severity::Error)
		//	<< "Failed to create event to signal shutdown request.";
		throw std::runtime_error("CreateEvent failed unexpectedly.");
	}
	m_requestShutdown = MakeUniqueHandle(requestShutdown);
}

StatusController::~StatusController()
{
}

/*static*/ std::string StatusController::CreateErrorResponse(const std::string& request, std::string&& error, std::exception *e)
{
	//Log("StatusController.FailedRequest", Severity::Warning)
	//	<< R"(Failed to service request. { "error": ")" << error << R"(", "request": ")" << request << R"(" })";
	nlohmann::json document {
		{ "Version", VERSION },
		{ "Error", error }
	};

	if (e != nullptr)
	{
		document["Exception"] = e->what();
	}

	return document.dump();
}

void StatusController::RecordGetStatusTime(uint64_t nanosecondsInGetStatus)
{
	WriteLock writeLock{m_getStatusStatisticsMutex};
	++m_totalGetStatusCalls;
	m_totalNanosecondsInGetStatus += nanosecondsInGetStatus;
	m_minNanosecondsInGetStatus = (std::min)(nanosecondsInGetStatus, m_minNanosecondsInGetStatus);
	m_maxNanosecondsInGetStatus = (std::max)(nanosecondsInGetStatus, m_maxNanosecondsInGetStatus);
}

std::string StatusController::GetStatus(const nlohmann::json& document, const std::string& request)
{
	if (!document["Path"].is_string())
	{
		return CreateErrorResponse(request, "'Path' must be specified.");
	}
	auto path = document["Path"].get<std::string>();

	auto repositoryPath = m_git.DiscoverRepository(path);
	if (!std::get<0>(repositoryPath))
	{
		return CreateErrorResponse(request, "Requested 'Path' is not part of a git repository.");
	}

	auto status = m_cache.GetStatus(std::get<1>(repositoryPath));
	if (!std::get<0>(status))
	{
		return CreateErrorResponse(request, "Failed to retrieve status of git repository at provided 'Path'.");
	}

	auto& statusToReport = std::get<1>(status);

	nlohmann::json response {
		{ "Version", VERSION },
		{ "Path", path },
		{ "RepoPath", statusToReport.RepositoryPath },
		{ "WorkingDir", statusToReport.WorkingDirectory },
		{ "State", statusToReport.State },
		{ "Branch", statusToReport.Branch },
		{ "Upstream", statusToReport.Upstream },
		{ "UpstreamGone", statusToReport.UpstreamGone },
		{ "AheadBy", statusToReport.AheadBy },
		{ "BehindBy", statusToReport.BehindBy },
		{ "IndexAdded", statusToReport.IndexAdded },
		{ "IndexModified", statusToReport.IndexModified },
		{ "IndexTypeChange", statusToReport.IndexTypeChange },
		{ "IndexRenamed", nlohmann::json::array() },
		{ "WorkingAdded", statusToReport.WorkingAdded },
		{ "WorkingModified", statusToReport.WorkingModified },
		{ "WorkingDeleted", statusToReport.WorkingDeleted },
		{ "WorkingTypeChange", statusToReport.WorkingTypeChange },
		{ "WorkingRenamed", nlohmann::json::array() },
		{ "WorkingUnreadable", statusToReport.WorkingUnreadable },
		{ "Ignored", statusToReport.Ignored },
		{ "Conflicted", statusToReport.Conflicted },
		{ "Stashes", nlohmann::json::array() }
	};

	for (const auto& value : statusToReport.IndexRenamed)
	{
		response["IndexRenamed"].push_back({
			{ "Old", value.first },
			{ "New", value.second }
		});
	}

	for (const auto& value : statusToReport.WorkingRenamed)
	{
		response["WorkingRenamed"].push_back({
			{ "Old", value.first },
			{ "New", value.second }
		});
	}

	for (const auto& value : statusToReport.Stashes)
	{
		response["Stashes"].push_back({
			{ "Name", "stash@{" + std::to_string(value.Index) + "}" },
			{ "Sha1Id", value.Sha1Id },
			{ "Message", value.Message }
		});
	}

	return response.dump();
}

std::string StatusController::GetCacheStatistics()
{
	auto statistics = m_cache.GetCacheStatistics();

	static const int nanosecondsPerMillisecond = 1000000;
	uint64_t totalGetStatusCalls;
	uint64_t totalNanosecondsInGetStatus;
	uint64_t minNanosecondsInGetStatus;
	uint64_t maxNanosecondsInGetStatus;
	{
		ReadLock readLock{m_getStatusStatisticsMutex};
		totalGetStatusCalls = m_totalGetStatusCalls;
		totalNanosecondsInGetStatus = m_totalNanosecondsInGetStatus;
		minNanosecondsInGetStatus = m_minNanosecondsInGetStatus;
		maxNanosecondsInGetStatus = m_maxNanosecondsInGetStatus;
	}
	
	auto averageNanosecondsInGetStatus = totalGetStatusCalls != 0 ? totalNanosecondsInGetStatus / totalGetStatusCalls : 0;
	auto averageMillisecondsInGetStatus = static_cast<double>(averageNanosecondsInGetStatus) / nanosecondsPerMillisecond;
	auto minMillisecondsInGetStatus = static_cast<double>(minNanosecondsInGetStatus) / nanosecondsPerMillisecond;
	auto maxMillisecondsInGetStatus = static_cast<double>(maxNanosecondsInGetStatus) / nanosecondsPerMillisecond;

	nlohmann::json response {
		{ "Version", VERSION },
		{ "TotalGetStatusRequests", totalGetStatusCalls },
		{ "AverageMillisecondsInGetStatus", averageMillisecondsInGetStatus },
		{ "MinimumMillisecondsInGetStatus", minMillisecondsInGetStatus },
		{ "MaximumMillisecondsInGetStatus", maxMillisecondsInGetStatus },
		{ "CacheHits",  statistics.CacheHits },
		{ "CacheMisses", statistics.CacheMisses },
		{ "EffectiveCachePrimes", statistics.CacheEffectivePrimeRequests },
		{ "TotalCachePrimes", statistics.CacheTotalPrimeRequests },
		{ "EffectiveCacheInvalidations", statistics.CacheEffectiveInvalidationRequests },
		{ "TotalCacheInvalidations", statistics.CacheTotalInvalidationRequests },
		{ "FullCacheInvalidations", statistics.CacheInvalidateAllRequests }
	};

	return response.dump();
}

std::string StatusController::Shutdown()
{
	//Log("StatusController.Shutdown", Severity::Info) << R"(Shutting down due to client request.")";
	::SetEvent(m_requestShutdown);

	nlohmann::json response {
		{ "Version", VERSION },
		{ "Result", "Shutting down." }
	};

	return response.dump();
}

std::string StatusController::HandleRequest(const std::string& request)
{
	nlohmann::json document;
	try
	{
		document = nlohmann::json::parse(request);
	}
	catch (nlohmann::json::parse_error &e)
	{
		return CreateErrorResponse(request, "Request must be valid JSON.", &e);
	}

	if (!document["Version"].is_number())
		return CreateErrorResponse(request, "'Version' must be specified.");

	if (document["Version"] != 1)
		return CreateErrorResponse(request, "Requested 'Version' unknown.");

	if (!document["Action"].is_string())
		return CreateErrorResponse(request, "'Action' must be specified.");
	auto action = document["Action"].get<std::string>();

	if (strcasecmp(action.c_str(), "GetStatus") == 0)
	{
		auto start = std::chrono::steady_clock::now();
		auto result = GetStatus(document, request);
		RecordGetStatusTime((start - std::chrono::steady_clock::now()).count());
		return result;
	}

	if (strcasecmp(action.c_str(), "GetCacheStatistics") == 0)
		return GetCacheStatistics();

	if (strcasecmp(action.c_str(), "Shutdown") == 0)
		return Shutdown();

	return CreateErrorResponse(request, "'Action' unrecognized.");
}

void StatusController::WaitForShutdownRequest()
{
	::WaitForSingleObject(m_requestShutdown, INFINITE);
}