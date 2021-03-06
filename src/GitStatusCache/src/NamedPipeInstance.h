#pragma once

#include <functional>
#include <thread>

/**
 * Dedicated pipe instance used to service requests for a single client.
 * Each instantiation services requests on its own thread.
 */
class NamedPipeInstance
{
public:
	enum IoResult
	{
		Success,
		Error,
		Aborted,
	};

private:
	using OnClientRequestCallback = std::function<std::string(const std::string&)>;
	using ReadResult = std::tuple<IoResult, std::string>;
	const size_t BufferSize = 4096;

	bool m_isClosed = false;
	UniqueHandle m_pipe;
	std::thread m_thread;
	std::once_flag m_flag;
	OnClientRequestCallback m_onClientRequestCallback;

	void OnClientRequest();
	ReadResult ReadRequest();
	IoResult WriteResponse(const std::string& response);

public:
	/**
	* Constructor. Callback must be thread-safe.
	* @param onClientRequestCallback Callback with logic to handle the request.
	*/
	NamedPipeInstance(SECURITY_ATTRIBUTES* sa, const OnClientRequestCallback& onClientRequestCallback);
	~NamedPipeInstance();

	/**
	 * Blocks until a client connects. Can be aborted with ::CancelSynchronousIo.
	 */
	IoResult Connect();

	/**
	 * Returns whether the client has disconnected.
	 */
	bool IsClosed() const { return m_isClosed; }
};
