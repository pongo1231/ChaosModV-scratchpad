#pragma once
#ifdef WITH_DEBUG_PANEL_SUPPORT

#include "Component.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>
#include <mutex>
#include <queue>
#include <set>

class DebugSocket : public Component
{
  private:
	websocketpp::server<websocketpp::config::asio> m_Server;

  public:
	std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> m_Connections;

	std::queue<std::function<void()>> m_DelegateQueue;
	std::mutex m_DelegateQueueMutex;

	DebugSocket();
	~DebugSocket();

  private:
	void Connect();

  public:
	void Close();

	virtual void OnModPauseCleanup() override;
	virtual void OnRun() override;

	template <class T>
	requires std::is_base_of_v<Component, T>
	friend struct ComponentHolder;
};

#endif