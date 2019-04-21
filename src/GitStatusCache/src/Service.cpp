#include "stdafx.h"

#include "NamedPipeServer.h"
#include "Service.h"
#include "StatusController.h"

#define SVCNAME L"GitStatusCache"
#define SVCDISPLAYNAME L"Git Status Cache"

void WINAPI SvcMain(DWORD argc, LPWSTR* argv);
void ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

static SERVICE_TABLE_ENTRY ServiceDispatchTable[] = {
	{ SVCNAME, SvcMain },
	{ NULL, NULL }
};

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;

std::unique_ptr<StatusController> gStatusController;

void SvcInstall()
{
	SC_HANDLE schManager;
	SC_HANDLE schService;
	std::vector<wchar_t> pathBuffer;

	DWORD copied = 0;
	do {
		pathBuffer.resize(pathBuffer.size() + MAX_PATH);
		copied = GetModuleFileName(0, &pathBuffer[0], MAX_PATH);
	} while (copied >= pathBuffer.size());

	std::wstring szPath(pathBuffer.begin(), pathBuffer.end());

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (schManager == NULL)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	schService = CreateService(
		schManager,
		SVCNAME,
		SVCDISPLAYNAME,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		szPath.c_str(),
		NULL, NULL, NULL, NULL, NULL
	);

	if (schService == NULL)
	{
		printf("CreateService failed (%d)\n", GetLastError());
		CloseServiceHandle(schManager);
		return;
	}

	printf("Service installed successfully\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);
}

void SvcUninstall()
{
	SERVICE_STATUS_PROCESS ssp;
	SC_HANDLE schService;
	SC_HANDLE schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	ULONGLONG dwStartTime = GetTickCount64();
	ULONGLONG dwTimeout = 30000; // 30-second time-out
	DWORD dwWaitTime;
	DWORD dwBytesNeeded;

	if (schManager == NULL)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	schService = OpenService(schManager,
		SVCNAME,
		SERVICE_STOP |
		SERVICE_QUERY_STATUS |
		SERVICE_ENUMERATE_DEPENDENTS |
		DELETE);

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schManager);
		return;
	}

	if (!QueryServiceStatusEx(
		schService,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)& ssp,
		sizeof(SERVICE_STATUS_PROCESS),
		&dwBytesNeeded))
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
		goto stop_cleanup;
	}

	if (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		while (ssp.dwCurrentState == SERVICE_STOP_PENDING)
		{
			printf("Service stop pending...\n");

			// Do not wait longer than the wait hint, only wait one-tenth of the wait hint
			// Additionally, wait only between 1 second and 10 seconds.
			dwWaitTime = max(1000, min(ssp.dwWaitHint / 10, 10000));
			
			Sleep(dwWaitTime);

			if (!QueryServiceStatusEx(
				schService,
				SC_STATUS_PROCESS_INFO,
				(LPBYTE)& ssp,
				sizeof(SERVICE_STATUS_PROCESS),
				&dwBytesNeeded))
			{
				printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
				goto stop_cleanup;
			}

			if (GetTickCount64() - dwStartTime > dwTimeout)
			{
				printf("Service stop timed out. Service will be marked for deletion...\n");
				break;
			}
		}
	}

	if (!ControlService(
		schService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)& ssp))
	{
		printf("ControlService failed (%d)\n", GetLastError());
		goto stop_cleanup;
	}

	while (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		// Do not wait longer than the wait hint, only wait one-tenth of the wait hint
		// Additionally, wait only between 1 second and 10 seconds.
		dwWaitTime = max(1000, min(ssp.dwWaitHint / 10, 10000));

		Sleep(dwWaitTime);

		if (!QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)& ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded))
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
			goto stop_cleanup;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
			break;

		if (GetTickCount64() - dwStartTime > dwTimeout)
		{
			printf("Service stop timed out. Service will be marked for deletion...\n");
			break;
		}
	}

	if (!DeleteService(schService))
	{
		printf("DeleteService failed (%d)\n", GetLastError());
		goto stop_cleanup;
	}

	printf("Service uninstalled successfully\n");

stop_cleanup:
	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);
}

void WINAPI SvcChange(DWORD dwCtrl)
{
	// Handle the requested control code. 

	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

		// Signal the service to stop.
		gStatusController->Shutdown();

		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

		return;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}
}

void ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.

	gSvcStatus.dwCurrentState = dwCurrentState;
	gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus.dwServiceSpecificExitCode = 0;
	gSvcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		gSvcStatus.dwControlsAccepted = 0;
	else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED))
		gSvcStatus.dwCheckPoint = 0;
	else gSvcStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

void WINAPI SvcMain(DWORD argc, LPWSTR* argv)
{
	gSvcStatusHandle = RegisterServiceCtrlHandler(SVCNAME, SvcChange);
	if (!gSvcStatusHandle)
	{
		// Log
		return;
	}

	gSvcStatus.dwServiceType = SERVICE_WIN32;
	gSvcStatus.dwServiceSpecificExitCode = 0;
	gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	gStatusController = std::make_unique<StatusController>();
	NamedPipeServer server([](const std::string & request) { return gStatusController->HandleRequest(request); });

	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	gStatusController->WaitForShutdownRequest();

	ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

int SvcStart()
{
	if (!StartServiceCtrlDispatcher(ServiceDispatchTable)) {
		// TODO: Logging
		return 1;
	}
	return 0;
}
