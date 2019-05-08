#pragma once

#include "stdafx.h"
#include <string>

struct ReadDirectoryRequest;

struct ReadDirectoryServer
{
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
};

struct ReadDirectoryRequest:
	public OVERLAPPED
{
	using notifications_t = std::queue<std::pair<DWORD, std::wstring>>;
	ReadDirectoryRequest(ReadDirectoryServer* pServer, const std::wstring directory, DWORD dwNotifyFilter, DWORD dwBufSize);
	~ReadDirectoryRequest();

	bool open_directory();

	bool begin_request();
	void cancel_request();
	
	HANDLE handle() { return m_hDirectory; }
	notifications_t process();

private:
	bool directory_opened() { return m_hDirectory != INVALID_HANDLE_VALUE;  }

	ReadDirectoryServer* m_pServer;
	HANDLE m_hDirectory;

	std::wstring m_directory;
	DWORD m_dwNotifyFilter;
	DWORD m_dwBufSize;

	std::vector<BYTE> m_buffer;
	std::vector<BYTE> m_backup_buffer;
};