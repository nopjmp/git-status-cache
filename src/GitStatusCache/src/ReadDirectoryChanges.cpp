#include "ReadDirectoryChanges.h"

#include <cassert>
#include <queue>
#include <filesystem>

/*
ReadDirectoryServer();
ReadDirectoryServer(const ReadDirectoryServer&) = delete;
ReadDirectoryServer(ReadDirectoryServer&&) = default;

~ReadDirectoryServer();

bool start();
void stop();

bool add_directory(const std::wstring directory, DWORD dwNotifyFilter, DWORD dwBufSize = 16384);
bool remove_directory(const std::wstring directory);
private:
	void run();

	void process_iocp_success(ULONG_PTR pKey, DWORD dwBytes, OVERLAPPED* pOverlapped);
	bool process_iocp_error(DWORD dwLastError, OVERLAPPED* pOverlapped);

	void process_add_directory(ReadDirectoryRequest*);
	void process_remove_directory(ReadDirectoryRequest*);
	void process_read_directory(ReadDirectoryRequest*);
*/

ReadDirectoryRequest::ReadDirectoryRequest(ReadDirectoryServer* pServer, const std::wstring directory, DWORD dwNotifyFilter, DWORD dwBufSize)
	: OVERLAPPED({}),
	m_pServer(pServer),
	m_hDirectory(INVALID_HANDLE_VALUE),
	m_directory(directory),
	m_dwNotifyFilter(dwNotifyFilter),
	m_dwBufSize(dwBufSize),
	m_buffer(dwBufSize),
	m_backup_buffer(dwBufSize)
{
}

ReadDirectoryRequest::~ReadDirectoryRequest()
{
	assert(m_hDirectory == INVALID_HANDLE_VALUE);
}

bool ReadDirectoryRequest::open_directory()
{
	if (directory_opened())
		return true;

	m_hDirectory = ::CreateFile(m_directory.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DIRECTORY | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL);
	
	return (m_hDirectory != INVALID_HANDLE_VALUE);
}

bool ReadDirectoryRequest::begin_request()
{
	if (!directory_opened())
		return false;

	return !!::ReadDirectoryChangesW(
		handle(),
		&m_buffer[0],
		static_cast<DWORD>(m_buffer.size()),
		TRUE,
		m_dwNotifyFilter,
		NULL,
		this,
		NULL);
}

void ReadDirectoryRequest::cancel_request()
{
	if (directory_opened())
	{
		::CancelIo(m_hDirectory);
		::CloseHandle(m_hDirectory);
		m_hDirectory = INVALID_HANDLE_VALUE;
	}
}

static std::wstring ExpandFileName(const std::wstring wstrFilename)
{
	// If it could be a short filename, expand it.
	std::wstring wszFilename = PathFindFileNameW(wstrFilename.c_str());
	// The maximum length of an 8.3 filename is twelve, including the dot.
	if (wszFilename.length() <= 12 && wszFilename.find_first_of(L"~") != std::wstring::npos)
	{
		// Convert to the long filename form. Unfortunately, this
		// does not work for deletions, so it's an imperfect fix.
		wchar_t wbuf[MAX_PATH];
		if (::GetLongPathName(wstrFilename.c_str(), wbuf, _countof(wbuf)) > 0)
			return { wbuf };
	}

	return wstrFilename;
}

ReadDirectoryRequest::notifications_t ReadDirectoryRequest::process()
{
	assert(m_pServer);
	notifications_t result;
	size_t offset = 0;
	for (;;)
	{
		PFILE_NOTIFY_INFORMATION fni = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(&m_backup_buffer[offset]);
		std::wstring filename{fni->FileName, fni->FileNameLength / sizeof(wchar_t)};
		auto path = std::filesystem::path(m_directory);
		path.append(filename);
		filename = ExpandFileName(path.c_str());

		result.emplace(fni->Action, filename);
		if (!fni->NextEntryOffset) {
			break;
		}

		offset += fni->NextEntryOffset;
	}
	return result;
}