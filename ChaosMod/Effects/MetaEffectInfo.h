#pragma once

#include <string>

#include "EffectsInfo.h"

struct MetaEffectInfo
{
	float EffectDurationModifier = 1;
	float TimerSpeedModifier = 1;
	bool ShouldHideChaosUI = false;
	bool AdditionalEffectsToDispatch = 0;
};

inline MetaEffectInfo g_metaInfo;