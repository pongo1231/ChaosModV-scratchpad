#include <stdafx.h>

#ifdef WITH_DEBUG_PANEL_SUPPORT
#include "DebugSocket.h"

#include "Components/EffectDispatcher.h"
#include "Effects/EnabledEffectsMap.h"
#include "Util/OptionsFile.h"
#include "Util/json.hpp"

#include <websocketpp/close.hpp>
#include <websocketpp/frame.hpp>

#define LISTEN_PORT 31819

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

using server      = websocketpp::server<websocketpp::config::asio>;
using message_ptr = websocketpp::config::asio::message_type::ptr;

using nlohmann::json;

static void PushDelegate(DebugSocket *debugSocket, std::function<void()> delegate)
{
	std::lock_guard lock(debugSocket->m_DelegateQueueMutex);
	debugSocket->m_DelegateQueue.push(delegate);
}

static void OnFetchEffects(DebugSocket *debugSocket, server *server, websocketpp::connection_hdl handle,
                           const json &payloadJson)
{
	json effectsJson;
	for (const auto &[effectId, effectData] : g_dictEnabledEffects)
	{
		json effectInfoJson;
		effectInfoJson["id"]   = effectId.GetEffectId();
		effectInfoJson["name"] = effectData.Name;

		effectsJson["effects"].push_back(effectInfoJson);
	}

	server->send(handle, effectsJson.dump(), websocketpp::frame::opcode::text);
}

static void OnTriggerEffect(DebugSocket *debugSocket, server *server, websocketpp::connection_hdl handle,
                            const json &payloadJson)
{
	if (!ComponentExists<EffectDispatcher>())
	{
		return;
	}

	if (!payloadJson.contains("effect_id") || !payloadJson["effect_id"].is_string())
	{
		return;
	}

	auto targetEffectId = payloadJson["effect_id"].get<std::string>();

	auto result         = g_dictEnabledEffects.find(targetEffectId);
	if (result != g_dictEnabledEffects.end())
	{
		PushDelegate(debugSocket, [result]() { GetComponent<EffectDispatcher>()->DispatchEffect(result->first); });
	}
}

static void OnMessage(DebugSocket *debugSocket, server *server, websocketpp::connection_hdl handle, message_ptr msg)
{
	auto payload = msg->get_payload();

	json payloadJson;
	try
	{
		payloadJson = json::parse(payload);
	}
	catch (json::parse_error &exception)
	{
		LOG("Received invalid message: " << exception.what());
		return;
	}

	if (!payloadJson.contains("command"))
	{
		return;
	}

	auto command = payloadJson["command"].get<std::string>();
#define SET_HANDLER(cmd, handler) \
	if (command == cmd)           \
		handler(debugSocket, server, handle, payloadJson);

	SET_HANDLER("fetch_effects", OnFetchEffects);
	SET_HANDLER("trigger_effect", OnTriggerEffect);

#undef HANDLER
}

static void OnOpen(websocketpp::connection_hdl handle, server &server, DebugSocket *debugSocket)
{
	debugSocket->m_Connections.insert(handle);
}

static void OnClose(websocketpp::connection_hdl handle, server &server, DebugSocket *debugSocket)
{
	debugSocket->m_Connections.erase(handle);
}

DebugSocket::DebugSocket()
{
	m_Server.init_asio();

	m_Server.set_open_handler(bind([&](websocketpp::connection_hdl handle) { OnOpen(handle, m_Server, this); }, ::_1));
	m_Server.set_close_handler(
	    bind([&](websocketpp::connection_hdl handle) { OnClose(handle, m_Server, this); }, ::_1));
	m_Server.set_message_handler(bind([&](::server *server, websocketpp::connection_hdl handle, message_ptr msg)
	                                  { OnMessage(this, server, handle, msg); },
	                                  &m_Server, ::_1, ::_2));

	m_Server.listen(LISTEN_PORT);
	m_Server.start_accept();

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

	LOG("Listening for incoming connections on port " STR(LISTEN_PORT));

#undef STR_HELPER
#undef STR
}

DebugSocket::~DebugSocket()
{
	Close();
}

void DebugSocket::Close()
{
	m_Server.stop_listening();

	for (const auto &handle : m_Connections)
	{
		m_Server.pause_reading(handle);
		m_Server.close(handle, websocketpp::close::status::going_away, "");
	}

	m_Connections.clear();

	m_Server.stop();
}

void DebugSocket::OnModPauseCleanup()
{
	Close();
}

void DebugSocket::OnRun()
{
	if (!m_DelegateQueue.empty())
	{
		std::lock_guard lock(m_DelegateQueueMutex);
		while (!m_DelegateQueue.empty())
		{
			m_DelegateQueue.front()();
			m_DelegateQueue.pop();
		}
	}

	m_Server.poll_one();
}

#endif