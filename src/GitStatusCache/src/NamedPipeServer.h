#pragma once

#include "NamedPipeInstance.h"

/**
 * Waits on a background thread for client connection requests.
 * When a new client connects the server creates a new NamedPipeInstance to
 * service the clients requests.
 */
class NamedPipeServer
{
public:
	/**
	 * Callback for request handling logic. Request provided in argument.
	 * Returns response.
	 */
	using OnClientRequestCallback = std::function<std::string(const std::string&)>;

private:
	SECURITY_ATTRIBUTES* m_SecurityAttr;
	std::thread m_serverThread;
	std::vector<std::unique_ptr<NamedPipeInstance>> m_pipeInstances;
	OnClientRequestCallback m_onClientRequestCallback;

	void WaitForClientRequest();
	void RemoveClosedPipeInstances();

public:
	/**
	 * Constructor.
	 * @param onClientRequestCallback Callback with logic to handle the request.
	 * Callback must be thread-safe.
	 */
	NamedPipeServer(const OnClientRequestCallback& onClientRequestCallback);
	NamedPipeServer(const NamedPipeServer&) = delete;
	~NamedPipeServer();
};