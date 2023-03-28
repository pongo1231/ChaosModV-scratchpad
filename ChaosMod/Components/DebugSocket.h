#pragma once
#ifdef WITH_DEBUG_PANEL_SUPPORT

#include "Component.h"

#include "Effects/EffectIdentifier.h"

#include <ixwebsocket/IXWebSocketServer.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>
#include <unordered_map>

class DebugSocket : public Component
{
  public:
	std::queue<std::function<void()>> m_DelegateQueue;
	std::mutex m_DelegateQueueMutex;

	bool m_IsProfiling = false;
	struct EffectTraceStats
	{
		std::uint64_t MaxTime;
		std::uint64_t EntryTimestamp;
	};
	std::unordered_map<std::string, EffectTraceStats> m_EffectTraceStats;

  private:
	std::unique_ptr<ix::WebSocketServer> m_Server;

  public:
	DebugSocket();
	~DebugSocket();

  private:
	void Connect();

  public:
	void Close();

	void ScriptLog(std::string_view scriptName, std::string_view text);

	virtual void OnModPauseCleanup() override;
	virtual void OnRun() override;

	template <class T>
	requires std::is_base_of_v<Component, T>
	friend struct ComponentHolder;
};

#endif