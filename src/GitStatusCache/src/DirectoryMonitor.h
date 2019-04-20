#pragma once
#include <ReadDirectoryChanges.h>

#include <filesystem>
#include <functional>
#include <shared_mutex>

/**
 * Monitors directories for changes and provides notifications by callback.
 */
class DirectoryMonitor
{
public:
	/**
	 * Action that triggered change notification.
	 */
	enum FileAction
	{
		Unknown,
		Added,
		Removed,
		Modified,
		RenamedFrom,
		RenamedTo
	};

	/**
	* Identifies AddDirectory call. Passed back with file change notifications.
	*/
	using Token = uint32_t;

	/**
	 * Callback for change notifications. Provides path and change action.
	 */
	using OnChangeCallback = std::function<void(Token, const std::filesystem::path&, FileAction)>;

	/**
	 * Callback for events lost notification. Notifications may be lost if changes
	 * occur more rapidly than they are processed.
	 */
	using OnEventsLostCallback = std::function<void(void)>;

private:
	DirectoryMonitor(const DirectoryMonitor&) = delete;

	HANDLE m_stopNotificationThread = INVALID_HANDLE_VALUE;
	std::thread m_notificationThread;

	OnChangeCallback m_onChangeCallback;
	OnEventsLostCallback m_onEventsLostCallback;

	CReadDirectoryChanges m_readDirectoryChanges;

	std::unordered_map<std::wstring, Token> m_directories;
	std::shared_mutex m_directoriesMutex;

	void WaitForNotifications();

public:
	/**
	 * Constructor. Callbacks will always be invoked on the same thread.
	 * @param onChangeCallback Callback for change notification.
	 * @param onEventsLostCallback Callback for lost events notification.
	 */
	DirectoryMonitor(const OnChangeCallback& onChangeCallback, const OnEventsLostCallback& onEventsLostCallback);
	~DirectoryMonitor();

	/**
	 * Registers a directory for change notifications.
	 * This method is thread-safe.
	 */
	Token AddDirectory(const std::wstring& directory);
};