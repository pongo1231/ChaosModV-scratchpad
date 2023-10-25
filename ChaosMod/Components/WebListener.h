#pragma once

#include "Component.h"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <mutex>
#include <queue>
#include <string>

class WebListener : public Component
{
  private:
	bool do_reconnect = false;
	websocketpp::client<websocketpp::config::asio_client> client;
	websocketpp::client<websocketpp::config::asio_client>::connection_ptr connection;

  public:
	std::queue<std::string> effects_queue;
	std::mutex effects_queue_mutex;

	std::string password;
	std::uint16_t cooldown;

	float ping_timer = 0.f;

	WebListener();
	~WebListener();

  private:
	void Connect();

  public:
	void Close();

	virtual void OnRun() override;

	template <class T>
	requires std::is_base_of_v<Component, T>
	friend struct ComponentHolder;
};