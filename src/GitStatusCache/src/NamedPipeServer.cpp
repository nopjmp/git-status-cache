#include "stdafx.h"
#include "NamedPipeServer.h"
#include "NamedPipeInstance.h"


const DWORD NAMED_PIPE_OWNER_PERMISSIONS = GENERIC_READ | GENERIC_WRITE;
const DWORD NAMED_PIPE_EVERYONE_PERMISSIONS = GENERIC_READ | GENERIC_WRITE;

struct security_attr_objs {
	PSID sidEveryone;
	PACL pDacl;
};

static void FreePipeSecurityAttr(SECURITY_ATTRIBUTES* sa) {
	if (sa)
	{
		HANDLE hHeap = GetProcessHeap();

		security_attr_objs* attr = (security_attr_objs*)(((char*)sa) + sizeof(SECURITY_ATTRIBUTES));
		HeapFree(hHeap, 0, attr->pDacl);
		FreeSid(attr->sidEveryone);
		HeapFree(hHeap, 0, sa);
	}
}

static int PipeSecurityAttr(SECURITY_ATTRIBUTES * *psa) {
	HANDLE hHeap = GetProcessHeap();

	SID_IDENTIFIER_AUTHORITY sidWorld = SECURITY_WORLD_SID_AUTHORITY;
	PSID sidEveryone = 0;
	HANDLE hToken = 0;
	SECURITY_ATTRIBUTES* sa = 0;
	SECURITY_DESCRIPTOR* sd = 0;
	PTOKEN_USER pOwnerToken = 0;
	PACL pDacl = 0;
	PSID sidOwner = 0;
	DWORD dwOwnerToken = 0, dwDacl = 0;

	if (!AllocateAndInitializeSid(&sidWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &sidEveryone))
		goto error;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		goto error;

	GetTokenInformation(hToken, TokenUser, 0, dwOwnerToken, &dwOwnerToken);

	sd = (SECURITY_DESCRIPTOR*)HeapAlloc(hHeap, 0, sizeof(SECURITY_DESCRIPTOR));
	sa = (SECURITY_ATTRIBUTES*)HeapAlloc(hHeap, 0, sizeof(SECURITY_ATTRIBUTES) + sizeof(security_attr_objs));
	pOwnerToken = (PTOKEN_USER)HeapAlloc(hHeap, 0, dwOwnerToken);
	if (!pOwnerToken || !sd || !sa)
		goto error;

	if (!GetTokenInformation(hToken, TokenUser, pOwnerToken, dwOwnerToken, &dwOwnerToken))
		goto error;
	sidOwner = pOwnerToken->User.Sid;

	if (!IsValidSid(sidOwner))
		goto error;

	dwDacl = sizeof(ACL) + (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) * 2 +
		GetLengthSid(sidEveryone) + GetLengthSid(sidOwner);

	pDacl = (PACL)HeapAlloc(hHeap, 0, dwDacl);
	if (!pDacl) goto error;
	memset(pDacl, 0, dwDacl);

	if (!InitializeAcl(pDacl, dwDacl, ACL_REVISION))
		goto error;

	if (!AddAccessAllowedAce(pDacl, ACL_REVISION, NAMED_PIPE_EVERYONE_PERMISSIONS, sidEveryone))
		goto error;

	if (!AddAccessAllowedAce(pDacl, ACL_REVISION, NAMED_PIPE_OWNER_PERMISSIONS, sidOwner))
		goto error;

	if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
		goto error;

	if (!SetSecurityDescriptorDacl(sd, true, pDacl, false))
		goto error;

	sa->nLength = sizeof(*sa);
	sa->bInheritHandle = true;
	sa->lpSecurityDescriptor = sd;

	security_attr_objs * attr = (security_attr_objs*)(((char*)sa) + sizeof(SECURITY_ATTRIBUTES));
	attr->sidEveryone = sidEveryone;
	attr->pDacl = pDacl;
	*psa = sa;

	CloseHandle(hToken);
	return 0;
error:
	if (sidEveryone) FreeSid(sidEveryone);
	if (sidOwner) FreeSid(sidOwner);
	if (hToken) CloseHandle(hToken);
	if (pOwnerToken) HeapFree(hHeap, 0, pOwnerToken);
	if (sa) HeapFree(hHeap, 0, sa);
	if (pDacl) HeapFree(hHeap, 0, pDacl);
	return -1;
}

void NamedPipeServer::WaitForClientRequest()
{
	//Log("NamedPipeServer.WaitForClientRequest.Start", Severity::Verbose) << "Server thread started.";

	while (true)
	{
		RemoveClosedPipeInstances();

		//Log("NamedPipeServer.WaitForClientRequest", Severity::Verbose) << "Creating named pipe instance and waiting for client.";
		auto pipe = std::make_unique<NamedPipeInstance>(m_SecurityAttr, m_onClientRequestCallback);
		auto connectResult = pipe->Connect();
		m_pipeInstances.emplace_back(std::move(pipe));

		if (connectResult != NamedPipeInstance::IoResult::Success)
		{
			//Log("NamedPipeServer.WaitForClientRequest.Stop", Severity::Verbose) << "Server thread stopping.";
			break;
		}
	}
}

void NamedPipeServer::RemoveClosedPipeInstances()
{
	auto originalSize = m_pipeInstances.size();

	m_pipeInstances.erase(
		std::remove_if(
			m_pipeInstances.begin(),
			m_pipeInstances.end(),
			[](const std::unique_ptr<NamedPipeInstance>& pipeInstance) { return pipeInstance->IsClosed(); }),
		m_pipeInstances.end());

	auto newSize = m_pipeInstances.size();
	if (newSize != originalSize)
	{
		//Log("NamedPipeServer.RemoveClosedPipeInstances", Severity::Spam)
		//	<< R"(Removed closed pipe instancs. { "instancesRemoved": )" << originalSize - newSize << " }";
	}
}

NamedPipeServer::NamedPipeServer(const OnClientRequestCallback& onClientRequestCallback)
	: m_onClientRequestCallback(onClientRequestCallback)
{
	PipeSecurityAttr(&m_SecurityAttr);
	//Log("NamedPipeServer.StartingBackgroundThread", Severity::Spam) << "Attempting to start server thread.";
	m_serverThread = std::thread(&NamedPipeServer::WaitForClientRequest, this);
}

NamedPipeServer::~NamedPipeServer()
{
	//Log("NamedPipeServer.ShutDown.StoppingBackgroundThread", Severity::Spam)
	//	<< R"(Shutting down server thread. { "threadId": 0x)" << std::hex << m_serverThread.get_id() << " }";
	::CancelSynchronousIo(m_serverThread.native_handle());
	m_serverThread.join();
	FreePipeSecurityAttr(m_SecurityAttr);
}
