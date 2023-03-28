#include <stdafx.h>

#include "EffectThreads.h"

#include "Util/Script.h"

static std::list<std::unique_ptr<EffectThread>> m_rgThreads;
static DWORD64 m_ullLastTimestamp;

static EffectThread *ThreadIdToThread(DWORD64 ullThreadId)
{
	const auto &result = std::find(m_rgThreads.begin(), m_rgThreads.end(), ullThreadId);

	if (result != m_rgThreads.end())
	{
		return result->get();
	}

	return nullptr;
}

static auto _StopThreadImmediately(auto it)
{
	auto &thread = *it;

	// Give thread a chance to stop gracefully
	// OK so maybe not really immediately but it's still blocking
	thread->Stop();
	int count = 0;
	while (!thread->HasStopped() && count++ < 20)
	{
		SwitchToFiber(g_MainThread);
		thread->OnRun();
	}

	return m_rgThreads.erase(it);
}

namespace EffectThreads
{
	DWORD64 CreateThread(RegisteredEffect *pEffect, bool bIsTimed)
	{
		std::unique_ptr<EffectThread> pThread = std::make_unique<EffectThread>(pEffect, bIsTimed);

		DWORD64 threadId                      = pThread->m_ullId;

		m_rgThreads.push_back(std::move(pThread));

		return threadId;
	}

	void StopThread(DWORD64 threadId)
	{
		const auto &result = std::find(m_rgThreads.begin(), m_rgThreads.end(), threadId);
		if (result != m_rgThreads.end())
		{
			(*result)->Stop();
		}
	}

	void StopThreadImmediately(DWORD64 threadId)
	{
		const auto &result = std::find(m_rgThreads.begin(), m_rgThreads.end(), threadId);
		if (result != m_rgThreads.end())
		{
			_StopThreadImmediately(result);
		}
	}

	void StopThreads()
	{
		for (std::unique_ptr<EffectThread> &pThread : m_rgThreads)
		{
			pThread->Stop();
		}
	}

	void StopThreadsImmediately()
	{
		for (auto it = m_rgThreads.begin(); it != m_rgThreads.end();)
		{
			it = _StopThreadImmediately(it);
		}
	}

	void PutThreadOnPause(DWORD ulTimeMs)
	{
		PVOID fiber          = GetCurrentFiber();
		const auto &ppResult = std::find(m_rgThreads.begin(), m_rgThreads.end(), fiber);
		if (ppResult != m_rgThreads.end())
		{
			(*ppResult)->m_iPauseTime = ulTimeMs;
		}
	}

	void _RunThread(auto &it, DWORD64 curTimestamp)
	{
		std::unique_ptr<EffectThread> &thread = *it;

		if (thread->HasStopped())
		{
			it = m_rgThreads.erase(it);
			return;
		}

		if (thread->m_iPauseTime > 0 && m_ullLastTimestamp)
		{
			thread->m_iPauseTime -= curTimestamp - m_ullLastTimestamp;
		}

		if (thread->m_iPauseTime <= 0)
		{
			thread->OnRun();
		}

		it++;
	}

	void RunThreads()
	{
		auto curTimestamp = GetTickCount64();
		for (auto it = m_rgThreads.begin(); it != m_rgThreads.end();)
		{
			_RunThread(it, curTimestamp);
		}
	}

	void RunThread(DWORD threadId)
	{
		for (auto it = m_rgThreads.begin(); it != m_rgThreads.end();)
		{
			if ((*it)->m_ullId == threadId)
			{
				_RunThread(it, GetTickCount64());
				break;
			}
		}
	}

	bool DoesThreadExist(DWORD64 threadId)
	{
		EffectThread *pThread = ThreadIdToThread(threadId);

		return pThread;
	}

	bool HasThreadOnStartExecuted(DWORD64 threadId)
	{
		EffectThread *pThread = ThreadIdToThread(threadId);

		return pThread ? pThread->HasOnStartExecuted() : true;
	}
}