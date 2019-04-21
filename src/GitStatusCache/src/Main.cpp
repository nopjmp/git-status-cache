#include "stdafx.h"
#include "DirectoryMonitor.h"
#include "NamedPipeServer.h"
#include "Service.h"
#include "StatusCache.h"
#include "StatusController.h"

int IsService(void)
{
	BOOL		member;
	PSID		ServiceSid;
	PSID		LocalSystemSid;
	SID_IDENTIFIER_AUTHORITY NtAuthority = { SECURITY_NT_AUTHORITY };

	/* First check for LocalSystem */
	if (!AllocateAndInitializeSid(&NtAuthority, 1,
		SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0,
		&LocalSystemSid))
	{
		return -1;
	}

	if (!CheckTokenMembership(NULL, LocalSystemSid, &member))
	{
		FreeSid(LocalSystemSid);
		return -1;
	}
	FreeSid(LocalSystemSid);

	if (member)
	{
		return 1;
	}

	/* Check for service group membership */
	if (!AllocateAndInitializeSid(&NtAuthority, 1,
		SECURITY_SERVICE_RID, 0, 0, 0, 0, 0, 0, 0,
		&ServiceSid))
	{
		return -1;
	}

	if (!CheckTokenMembership(NULL, ServiceSid, &member))
	{
		FreeSid(ServiceSid);
		return -1;
	}
	FreeSid(ServiceSid);

	return member;
}

int wmain(int argc, char** argv)
{
	if (argv[1] != NULL)
	{
		if (_strcmpi(argv[1], "install") == 0)
		{
			SvcInstall();
			return 0;
		}

		if (_strcmpi(argv[1], "uninstall") == 0)
		{
			SvcUninstall();
			return 0;
		}

		if (_strcmpi(argv[1], "debug") == 0)
		{
			StatusController statusController;
			NamedPipeServer server([&statusController](const std::string & request) { return statusController.HandleRequest(request); });

			statusController.WaitForShutdownRequest();
			return 0;
		}
	}

	if (IsService() == 1) {
		return SvcStart();
	}

	return 1;
}
