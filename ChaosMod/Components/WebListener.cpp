#include <stdafx.h>

#include "Components/EffectDispatcher.h"
#include "Util/OptionsFile.h"
#include "WebListener.h"

#define WEB_SERVER "ws://gopong.dev:6909"

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

using client      = websocketpp::client<websocketpp::config::asio_client>;
using message_ptr = websocketpp::config::asio_client::message_type::ptr;

static void on_open(websocketpp::connection_hdl handle, client &client, WebListener *web_listener)
{
	std::string effects_str;
	for (const auto &[identifier, data] : g_dictEnabledEffects)
		effects_str += identifier.GetEffectId() + "ä" + (data.HasCustomName() ? data.CustomName : data.Name) + "ö";

	client.send(handle,
	            "initß" + web_listener->password + "ß" + std::to_string(web_listener->cooldown) + "ß" + effects_str,
	            websocketpp::frame::opcode::text);
}

static void on_message(client *client, websocketpp::connection_hdl handle, message_ptr msg, WebListener *web_listener)
{
	const auto &payload = msg->get_payload();

	if (payload == "ping")
	{
		web_listener->ping_timer = 6.f;
		return;
	}

	bool found = false;
	for (const auto &[identifier, data] : g_dictEnabledEffects)
	{
		if (payload == identifier)
		{
			found = true;
			break;
		}
	}

	if (!found)
		return;

	std::lock_guard lock(web_listener->effects_queue_mutex);
	web_listener->effects_queue.push(payload);
}

WebListener::WebListener()
{
	OptionsFile options("chaosmod/webpanel.ini");
	password = options.ReadValueString("password", "fatcat310");
	cooldown = options.ReadValue("cooldown", 900);

	client.init_asio();

	client.set_open_handler(bind([&](websocketpp::connection_hdl handle) { on_open(handle, client, this); }, ::_1));
	client.set_close_handler(bind([&](websocketpp::connection_hdl handle) { do_reconnect = true; }, ::_1));
	client.set_fail_handler(bind([&](websocketpp::connection_hdl handle) { do_reconnect = true; }, ::_1));
	client.set_message_handler(bind([&](::client *client, websocketpp::connection_hdl handle, message_ptr msg)
	                                { on_message(client, handle, msg, this); },
	                                &client, ::_1, ::_2));

	Connect();
}

WebListener::~WebListener()
{
	Close();
}

void WebListener::Connect()
{
	do_reconnect = false;

	websocketpp::lib::error_code error_code;
	connection = client.get_connection(WEB_SERVER, error_code);

	if (error_code || !connection)
		do_reconnect = true;
	else
	{
		try
		{
			client.connect(connection);
			ping_timer = 6.f;
		}
		catch (const websocketpp::exception &exception)
		{
			do_reconnect = true;
		}
	}
}

void WebListener::Close()
{
	try
	{
		if (connection)
			connection->close(websocketpp::close::status::normal, "stop_thread");
	}
	catch (const websocketpp::exception &exception)
	{
	}
}

void WebListener::OnRun()
{
	auto time = GetTickCount64();
	if (do_reconnect || (ping_timer -= GET_FRAME_TIME()) <= 0.f)
	{
		Close();
		Connect();
	}

	try
	{
		client.poll();
	}
	catch (const websocketpp::exception &exception)
	{
		do_reconnect = true;
	}

	if (!effects_queue.empty())
	{
		std::lock_guard lock(effects_queue_mutex);

		while (!effects_queue.empty())
		{
			if (ComponentExists<EffectDispatcher>())
				GetComponent<EffectDispatcher>()->DispatchEffect(effects_queue.front());

			effects_queue.pop();
		}
	}
}