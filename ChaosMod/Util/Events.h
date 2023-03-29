#pragma once

#include <functional>
#include <vector>

template <typename... Args> class ChaosEvent
{
	std::vector<std::function<void(Args...)>> m_Listeners;

  public:
	void AddListener(std::function<void(Args...)> listener)
	{
		m_Listeners.push_back(listener);
	}

	void Fire(Args... args)
	{
		for (const auto &listener : m_Listeners)
		{
			listener(args...);
		}
	}
};

template <typename... Args> class ChaosCancellableEvent
{
	std::vector<std::function<bool(Args...)>> m_Listeners;

  public:
	void AddListener(std::function<bool(Args...)> listener)
	{
		m_Listeners.push_back(listener);
	}

	bool Fire(Args... args)
	{
		bool result = true;
		for (const auto &listener : m_Listeners)
		{
			if (!listener(args...))
			{
				result = false;
			}
		}

		return result;
	}
};