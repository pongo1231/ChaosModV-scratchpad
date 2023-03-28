#include <stdafx.h>

#ifdef WITH_DEBUG_PANEL_SUPPORT
#include "DebugSocket.h"

#include "Components/EffectDispatcher.h"
#include "Effects/EnabledEffectsMap.h"
#include "Util/OptionsFile.h"

#include <json.hpp>

#define LISTEN_PORT 31819

using nlohmann::json;

static void QueueDelegate(DebugSocket *debugSocket, std::function<void()> delegate)
{
	std::lock_guard lock(debugSocket->m_DelegateQueueMutex);
	debugSocket->m_DelegateQueue.push(delegate);
}

static void OnFetchEffects(DebugSocket *debugSocket, std::shared_ptr<ix::ConnectionState> connectionState,
                           ix::WebSocket &webSocket, const json &payloadJson)
{
	// TODO: This isn't thread safe currently!

	json effectsJson;
	effectsJson["command"] = "result_fetch_effects";
	for (const auto &[effectId, effectData] : g_dictEnabledEffects)
	{
		if (effectData.TimedType == EEffectTimedType::Permanent || effectData.IsHidden())
		{
			continue;
		}

		json effectInfoJson;
		effectInfoJson["id"]   = effectId.GetEffectId();
		effectInfoJson["name"] = effectData.Name;

		effectsJson["effects"].push_back(effectInfoJson);
	}

	webSocket.send(effectsJson.dump());
}

static void OnTriggerEffect(DebugSocket *debugSocket, std::shared_ptr<ix::ConnectionState> connectionState,
                            ix::WebSocket &webSocket, const json &payloadJson)
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
	if (targetEffectId.empty())
	{
		return;
	}

	QueueDelegate(debugSocket,
	              [targetEffectId]
	              {
		              auto result = g_dictEnabledEffects.find(targetEffectId);
		              if (result != g_dictEnabledEffects.end())
		              {
			              GetComponent<EffectDispatcher>()->DispatchEffect(result->first);
		              }
	              });
}

static void OnExecScript(DebugSocket *debugSocket, std::shared_ptr<ix::ConnectionState> connectionState,
                         ix::WebSocket &webSocket, const json &payloadJson)
{
	if (!ComponentExists<EffectDispatcher>())
	{
		return;
	}

	if (!payloadJson.contains("script_raw") || !payloadJson["script_raw"].is_string())
	{
		return;
	}

	auto script = payloadJson["script_raw"].get<std::string>();
	if (script.empty())
	{
		return;
	}

	// Generate random hex value for script name
	std::string scriptName;
	scriptName.resize(8);
	for (int i = 0; i < 8; i++)
	{
		sprintf(scriptName.data() + i, "%x", g_Random.GetRandomInt(0, 16));
	}

	json json;
	json["command"]     = "result_exec_script";
	json["script_name"] = scriptName;
	webSocket.send(json.dump());

	QueueDelegate(debugSocket, [payloadJson, scriptName]
	              { LuaScripts::RegisterScriptRawTemporary(scriptName, payloadJson["script_raw"]); });
}

static void OnFetchProfiles(DebugSocket *debugSocket, std::shared_ptr<ix::ConnectionState> connectionState,
                            ix::WebSocket &webSocket, const json &payloadJson)
{
	if (!payloadJson.contains("state") || !payloadJson["state"].is_string())
	{
		return;
	}

	const auto &state = payloadJson["state"];
	if (state == "start")
	{
		debugSocket->m_IsProfiling = true;
	}
	else if (state == "stop")
	{
		debugSocket->m_IsProfiling = false;
		QueueDelegate(debugSocket, [&] { debugSocket->m_EffectTraceStats.clear(); });
	}
	else if (state == "fetch")
	{
		QueueDelegate(debugSocket,
		              [&]
		              {
			              json resultJson;
			              resultJson["command"] = "profile_state";
			              for (const auto &[effectId, traceStats] : debugSocket->m_EffectTraceStats)
			              {
				              json profileJson;
				              profileJson["effect_id"]       = effectId;
				              profileJson["last_entry_time"] = traceStats.EntryTimestamp;
				              profileJson["max_exec_time"]   = traceStats.MaxTime;

				              resultJson["profiles"].push_back(profileJson);
			              }

			              webSocket.send(resultJson.dump());
		              });
	}
}

static void OnMessage(DebugSocket *debugSocket, std::shared_ptr<ix::ConnectionState> connectionState,
                      ix::WebSocket &webSocket, const ix::WebSocketMessagePtr &msg)
{
	if (msg->type != ix::WebSocketMessageType::Message)
	{
		return;
	}

	auto payload = msg->str;

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
		handler(debugSocket, connectionState, webSocket, payloadJson);

	SET_HANDLER("fetch_effects", OnFetchEffects);
	SET_HANDLER("trigger_effect", OnTriggerEffect);
	SET_HANDLER("exec_script", OnExecScript);
	SET_HANDLER("profile_state", OnFetchProfiles);

#undef HANDLER
}

static bool EventOnPreRunEffect(DebugSocket *debugSocket, const EffectIdentifier &identifier)
{
	if (!debugSocket->m_IsProfiling)
	{
		return true;
	}

	debugSocket->m_EffectTraceStats[identifier.GetEffectId()].EntryTimestamp = GetTickCount64();
	return true;
}

static void EventOnPostRunEffect(DebugSocket *debugSocket, const EffectIdentifier &identifier)
{
	if (!debugSocket->m_IsProfiling)
	{
		return;
	}

	const auto &effectId = identifier.GetEffectId();
	if (!debugSocket->m_EffectTraceStats.contains(effectId))
	{
		return;
	}

	auto execTime = GetTickCount64() - debugSocket->m_EffectTraceStats[effectId].EntryTimestamp;
	if (execTime > debugSocket->m_EffectTraceStats[effectId].MaxTime)
	{
		debugSocket->m_EffectTraceStats[effectId].MaxTime = execTime;
	}
}

DebugSocket::DebugSocket()
{
	m_Server = std::make_unique<ix::WebSocketServer>(LISTEN_PORT, "127.0.0.1");

	m_Server->setOnClientMessageCallback([&](std::shared_ptr<ix::ConnectionState> connectionState,
	                                         ix::WebSocket &webSocket, const ix::WebSocketMessagePtr &msg)
	                                     { OnMessage(this, connectionState, webSocket, msg); });
	m_Server->listenAndStart();

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
	LOG("Listening for incoming connections on port " STR(LISTEN_PORT));
#undef STR_HELPER
#undef STR

	if (ComponentExists<EffectDispatcher>())
	{
		GetComponent<EffectDispatcher>()->OnPreRunEffect.AddListener([&](const EffectIdentifier &identifier)
		                                                             { return EventOnPreRunEffect(this, identifier); });
		GetComponent<EffectDispatcher>()->OnPostRunEffect.AddListener([&](const EffectIdentifier &identifier)
		                                                              { EventOnPostRunEffect(this, identifier); });
	}
}

DebugSocket::~DebugSocket()
{
	Close();
}

void DebugSocket::Close()
{
	m_Server->stop();
}

void DebugSocket::ScriptLog(std::string_view scriptName, std::string_view text)
{
	if (!m_Server)
	{
		return;
	}

	json json;
	json["command"]     = "script_log";
	json["script_name"] = scriptName;
	json["text"]        = text;

	for (auto client : m_Server->getClients())
	{
		client->send(json.dump());
	}
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
}

#endif