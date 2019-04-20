#include "stdafx.h"
#include "CachePrimer.h"

CachePrimer::CachePrimer(const std::shared_ptr<Cache>& cache)
	: m_cache(cache)
	, m_stopPrimingThread(MakeUniqueHandle(INVALID_HANDLE_VALUE))
{
	auto stopPrimingThread = ::CreateEvent(
		nullptr /*lpEventAttributes*/,
		true    /*manualReset*/,
		false   /*bInitialState*/,
		nullptr /*lpName*/);
	if (stopPrimingThread == nullptr)
	{
		//Log("CachePrimer.StartingPrimingThread.CreateEventFailed", Severity::Error)
		//	<< "Failed to create event to signal thread on exit.";
		throw std::runtime_error("CreateEvent failed unexpectedly.");
	}
	m_stopPrimingThread = MakeUniqueHandle(stopPrimingThread);

	//Log("CachePrimer.StartingPrimingThread", Severity::Spam)
	//	<< "Attempting to start background thread for cache priming.";
	m_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
	m_primingThread = std::thread(&CachePrimer::WaitForPrimingTimerExpiration, this);
}

CachePrimer::~CachePrimer()
{
	//Log("CachePrimer.Shutdown.StoppingPrimingThread", Severity::Spam)
	//	<< R"(Shutting down cache priming thread. { "threadId": 0x)" << std::hex << m_primingThread.get_id() << " }";

	::SetEvent(m_stopPrimingThread);
	m_primingThread.join();
}

void CachePrimer::Prime()
{
	std::unordered_set<std::string> repositoriesToPrime;
	{
		LockGuard lock(m_primingMutex);
		m_repositoriesToPrime.swap(repositoriesToPrime);
	}

	if (!repositoriesToPrime.empty())
	{
		for (auto repositoryPath : repositoriesToPrime)
			m_cache->PrimeCacheEntry(repositoryPath);
	}
}

void CachePrimer::WaitForPrimingTimerExpiration()
{
	//Log("CachePrimer.WaitForPrimingTimerExpiration.Start", Severity::Verbose) << "Thread for cache priming started.";

	do
	{
		std::chrono::steady_clock::time_point deadline;
		{
			LockGuard lock(m_primingMutex);
			deadline = m_deadline;
		}
		
		if (deadline < std::chrono::steady_clock::now())
		{
			Prime();

			{
				LockGuard lock(m_primingMutex);
				m_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(5));
	} while (::WaitForSingleObject(m_stopPrimingThread, 5) != WAIT_OBJECT_0);

	//Log("CachePrimer.WaitForPrimingTimerExpiration.Stop", Severity::Verbose) << "Thread for cache priming stopping.";
}

void CachePrimer::SchedulePrimingForRepositoryPath(const std::string& repositoryPath)
{
	LockGuard lock(m_primingMutex);
	m_repositoriesToPrime.insert(repositoryPath);
	m_deadline += std::chrono::seconds(5);
}