/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/EmuTiming.hh>
#include <imagine/util/utility.h>
#include <imagine/util/math/int.hh>
#include <imagine/logger/logger.h>
#include <cmath>

namespace EmuEx
{

EmuFrameTimeInfo EmuTiming::advanceFramesWithTime(SteadyClockTimePoint time)
{
	if(!hasTime(startFrameTime)) [[unlikely]]
	{
		// first frame
		startFrameTime = time;
		lastFrame = 0;
		return {1, timePerVideoFrameScaled + startFrameTime};
	}
	assumeExpr(timePerVideoFrameScaled.count() > 0);
	assumeExpr(startFrameTime.time_since_epoch().count() > 0);
	assumeExpr(time > startFrameTime);
	auto timeTotal = time - startFrameTime;
	auto now = divRoundClosestPositive(timeTotal.count(), timePerVideoFrameScaled.count());
	int elapsedFrames = now - lastFrame;
	lastFrame = now;
	return {elapsedFrames, now * timePerVideoFrameScaled + startFrameTime};
}

void EmuTiming::setFrameTime(FloatSeconds time)
{
	timePerVideoFrame = std::chrono::round<SteadyClockTime>(time);
	updateScaledFrameTime();
	logMsg("configured frame time:%lldns (%.2f fps)", (long long)timePerVideoFrame.count(), 1. / time.count());
	reset();
}

void EmuTiming::reset()
{
	startFrameTime = {};
}

void EmuTiming::setSpeedMultiplier(double newSpeed)
{
	assumeExpr(newSpeed > 0.);
	if(speed == newSpeed)
		return;
	speed = newSpeed;
	updateScaledFrameTime();
	reset();
}

void EmuTiming::updateScaledFrameTime()
{
	timePerVideoFrameScaled = speed == 1. ? timePerVideoFrame : std::chrono::round<SteadyClockTime>(FloatSeconds{timePerVideoFrame} / speed);
}

}
