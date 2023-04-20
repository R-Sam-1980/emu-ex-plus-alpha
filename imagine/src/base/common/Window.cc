/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "Window"
#include <imagine/base/ApplicationContext.hh>
#include <imagine/base/Application.hh>
#include <imagine/base/Window.hh>
#include <imagine/base/Screen.hh>
#include <imagine/input/Input.hh>
#include <imagine/util/algorithm.h>
#include <imagine/util/variant.hh>
#include <imagine/logger/logger.h>

namespace IG
{

BaseWindow::BaseWindow(ApplicationContext ctx, WindowConfig config):
	onEvent{config.onEvent},
	onExit
	{
		[this](ApplicationContext ctx, bool backgrounded)
		{
			auto &win = *static_cast<Window*>(this);
			auto savedDrawEventPriority = win.setDrawEventPriority(Window::drawEventPriorityLocked);
			drawEvent.detach();
			if(backgrounded)
			{
				ctx.addOnResume(
					[this, savedDrawEventPriority](ApplicationContext, bool)
					{
						auto &win = *static_cast<Window*>(this);
						win.setDrawEventPriority(savedDrawEventPriority);
						attachDrawEvent();
						return false;
					}, WINDOW_ON_RESUME_PRIORITY
				);
			}
			return true;
		}, ctx, WINDOW_ON_EXIT_PRIORITY}
{
	attachDrawEvent();
}

void BaseWindow::attachDrawEvent()
{
	drawEvent.attach(
		[&win = *static_cast<Window*>(this)]()
		{
			//logDMsg("running window events");
			win.dispatchOnFrame();
			win.dispatchOnDraw();
		});
}

static Window::FrameTimeSource frameClock(const Screen &screen, Window::FrameTimeSource clock)
{
	if(clock == Window::FrameTimeSource::AUTO)
		return screen.supportsTimestamps() ? Window::FrameTimeSource::SCREEN : Window::FrameTimeSource::RENDERER;
	return clock;
}

bool Window::addOnFrame(OnFrameDelegate del, FrameTimeSource clock, int priority)
{
	clock = frameClock(*screen(), clock);
	if(clock == FrameTimeSource::SCREEN)
	{
		return screen()->addOnFrame(del);
	}
	else
	{
		bool added = onFrame.add(del, priority);
		if(drawPhase == DrawPhase::UPDATE)
		{
			// trigger a draw so delegate runs at start of next frame
			setNeedsDraw(true);
		}
		drawEvent.notify();
		return added;
	}
}

bool Window::removeOnFrame(OnFrameDelegate del, FrameTimeSource clock)
{
	clock = frameClock(*screen(), clock);
	if(clock == FrameTimeSource::SCREEN)
	{
		return screen()->removeOnFrame(del);
	}
	else
	{
		return onFrame.remove(del);
	}
}

bool Window::moveOnFrame(Window &srcWin, OnFrameDelegate del, FrameTimeSource src)
{
	srcWin.removeOnFrame(del, src);
	return addOnFrame(del, src);
}

void Window::resetAppData()
{
	appDataPtr.reset();
}

void Window::resetRendererData()
{
	rendererDataPtr.reset();
}

bool Window::isMainWindow() const
{
	return *this == appContext().mainWindow();
}

Screen *Window::screen() const
{
	if constexpr(Config::BASE_MULTI_SCREEN)
		return screen_;
	else
		return &appContext().mainScreen();
}

bool Window::setNeedsDraw(bool needsDraw)
{
	if(needsDraw && (!hasSurface() || drawEventPriority() == drawEventPriorityLocked))
	{
		needsDraw = false;
	}
	drawNeeded = needsDraw;
	return needsDraw;
}

bool Window::needsDraw() const
{
	return drawNeeded;
}

void Window::postDraw(int8_t priority)
{
	if(priority < drawEventPriority())
	{
		logDMsg("skipped posting draw with priority:%u < %u", priority, drawEventPriority());
		return;
	}
	if(!setNeedsDraw(true))
		return;
	if(drawPhase != DrawPhase::DRAW)
		drawEvent.notify();
	//logDMsg("window:%p needs draw", this);
}

void Window::unpostDraw()
{
	setNeedsDraw(false);
	drawEvent.cancel();
	//logDMsg("window:%p cancelled draw", this);
}

void Window::postFrameReady()
{
	drawPhase = DrawPhase::READY;
	if(onFrame.size() || needsDraw())
		drawEvent.notify();
}

void Window::postDrawToMainThread(int8_t priority)
{
	appContext().runOnMainThread(
		[this, priority](ApplicationContext)
		{
			postDraw(priority);
		});
}

void Window::postFrameReadyToMainThread()
{
	postFrameReady();
}

int8_t Window::setDrawEventPriority(int8_t priority)
{
	if(priority == drawEventPriorityLocked)
	{
		unpostDraw();
		drawPhase = DrawPhase::UPDATE;
	}
	return std::exchange(drawEventPriority_, priority);
}

int8_t Window::drawEventPriority() const
{
	return drawEventPriority_;
}

void Window::drawNow(bool needsSync)
{
	draw(needsSync);
}

bool Window::dispatchInputEvent(Input::Event event)
{
	bool handled = onEvent.callCopy(*this, event);
	return visit(overloaded{
		[&](const Input::MotionEvent &e)
		{
			return handled || (e.isAbsolute() && contentBounds().overlaps(e.pos()));
		},
		[&](const Input::KeyEvent &e) { return handled; }
	}, event);
}

bool Window::dispatchRepeatableKeyInputEvent(Input::KeyEvent event)
{
	application().startKeyRepeatTimer(event);
	return dispatchInputEvent(event);
}

void Window::dispatchFocusChange(bool in)
{
	onEvent.callCopy(*this, FocusChangeEvent{in});
}

void Window::dispatchDragDrop(const char *filename)
{
	onEvent.callCopy(*this, DragDropEvent{filename});
}

void Window::dispatchDismissRequest()
{
	if(!onEvent.callCopy(*this, DismissRequestEvent{}))
	{
		appContext().exit();
	}
}

void Window::dispatchSurfaceCreated()
{
	onEvent.callCopy(*this, WindowSurfaceChangeEvent{SurfaceChange::Action::CREATED});
	surfaceChangeFlags |= SurfaceChange::SURFACE_RESIZED;
}

void Window::dispatchSurfaceChanged()
{
	onEvent.callCopy(*this, WindowSurfaceChangeEvent{SurfaceChange{SurfaceChange::Action::CHANGED, std::exchange(surfaceChangeFlags, {})}});
}

void Window::dispatchSurfaceDestroyed()
{
	surfaceChangeFlags = 0;
	unpostDraw();
	onEvent.callCopy(*this, WindowSurfaceChangeEvent{SurfaceChange::Action::DESTROYED});
}

void Window::signalSurfaceChanged(uint8_t flags)
{
	surfaceChangeFlags |= flags;
	postDraw();
}

void Window::dispatchOnDraw(bool needsSync)
{
	if(!needsDraw() || drawPhase == DrawPhase::DRAW)
		return;
	draw(needsSync);
}

void Window::dispatchOnFrame()
{
	if(drawPhase != DrawPhase::READY || !onFrame.size())
	{
		return;
	}
	drawPhase = DrawPhase::UPDATE;
	//logDMsg("running %u onFrame delegates", onFrame.size());
	FrameParams frameParams{SteadyClock::now(), screen()->frameTime()};
	onFrame.runAll([&](OnFrameDelegate del){ return del(frameParams); });
	if(onFrame.size())
	{
		setNeedsDraw(true);
	}
}

void Window::draw(bool needsSync)
{
	DrawParams params;
	params.needsSync = needsSync;
	if(surfaceChangeFlags) [[unlikely]]
	{
		dispatchSurfaceChanged();
		params.wasResized = true;
	}
	drawNeeded = false;
	drawPhase = DrawPhase::DRAW;
	if(!onEvent.callCopy(*this, DrawEvent{params}))
	{
		postFrameReady();
	}
}

bool Window::updateSize(IG::Point2D<int> surfaceSize)
{
	if(isSideways(softOrientation_))
		std::swap(surfaceSize.x, surfaceSize.y);
	auto oldSize = std::exchange(winSizePixels, surfaceSize);
	if(oldSize == winSizePixels)
	{
		logMsg("same window size %d,%d", realWidth(), realHeight());
		return false;
	}
	if constexpr(Config::envIsAndroid)
	{
		updatePhysicalSize(pixelSizeAsMM({realWidth(), realHeight()}), pixelSizeAsScaledMM({realWidth(), realHeight()}));
	}
	else
	{
		updatePhysicalSize(pixelSizeAsMM({realWidth(), realHeight()}));
	}
	surfaceChangeFlags |= SurfaceChange::SURFACE_RESIZED;
	return true;
}

bool Window::updatePhysicalSize(IG::Point2D<float> surfaceSizeMM, IG::Point2D<float> surfaceSizeSMM)
{
	bool changed = false;
	if(isSideways(softOrientation_))
		std::swap(surfaceSizeMM.x, surfaceSizeMM.y);
	auto oldSizeMM = std::exchange(winSizeMM, surfaceSizeMM);
	if(oldSizeMM != winSizeMM)
		changed = true;
	auto pixelSizeFloat = IG::Point2D<float>{(float)width(), (float)height()};
	mmToPixelScaler = pixelSizeFloat / winSizeMM;
	if constexpr(Config::envIsAndroid)
	{
		assert(surfaceSizeSMM.x && surfaceSizeSMM.y);
		if(isSideways(softOrientation_))
			std::swap(surfaceSizeSMM.x, surfaceSizeSMM.y);
		auto oldSizeSMM = std::exchange(winSizeSMM, surfaceSizeSMM);
		if(oldSizeSMM != sizeScaledMM())
			changed = true;
		smmToPixelScaler = pixelSizeFloat / sizeScaledMM();
	}
	if(softOrientation_ == Rotation::UP)
	{
		logMsg("updated window size:%dx%d (%.2fx%.2fmm, scaled %.2fx%.2fmm)",
			width(), height(), sizeMM().x, sizeMM().y, sizeScaledMM().x, sizeScaledMM().y);
	}
	else
	{
		logMsg("updated window size:%dx%d (%.2fx%.2fmm, scaled %.2fx%.2fmm) with rotation, real size:%dx%d",
			width(), height(), sizeMM().x, sizeMM().y, sizeScaledMM().x, sizeScaledMM().y, realWidth(), realHeight());
	}
	return changed;
}

bool Window::updatePhysicalSize(IG::Point2D<float> surfaceSizeMM)
{
	return updatePhysicalSize(surfaceSizeMM, {0., 0.});
}

bool Window::updatePhysicalSizeWithCurrentSize()
{
	#ifdef __ANDROID__
	return updatePhysicalSize(pixelSizeAsMM({realWidth(), realHeight()}), pixelSizeAsScaledMM({realWidth(), realHeight()}));
	#else
	return updatePhysicalSize(pixelSizeAsMM({realWidth(), realHeight()}));
	#endif
}

#ifdef CONFIG_GFX_SOFT_ORIENTATION
bool Window::setValidOrientations(OrientationMask oMask)
{
	if(to_underlying(oMask & OrientationMask::PORTRAIT))
		return requestOrientationChange(Rotation::UP);
	else if(to_underlying(oMask & OrientationMask::LANDSCAPE_RIGHT))
		return requestOrientationChange(Rotation::RIGHT);
	else if(to_underlying(oMask & OrientationMask::PORTRAIT_UPSIDE_DOWN))
		return requestOrientationChange(Rotation::DOWN);
	else if(to_underlying(oMask & OrientationMask::LANDSCAPE_LEFT))
		return requestOrientationChange(Rotation::LEFT);
	else
		return requestOrientationChange(Rotation::UP);
}

bool Window::requestOrientationChange(Rotation o)
{
	if(softOrientation_ != o)
	{
		logMsg("setting orientation %s", wise_enum::to_string(o).data());
		int savedRealWidth = realWidth();
		int savedRealHeight = realHeight();
		softOrientation_ = o;
		updateSize({savedRealWidth, savedRealHeight});
		postDraw();
		if(isMainWindow())
			appContext().setSystemOrientation(o);
		return true;
	}
	return false;
}
#endif

Rotation Window::softOrientation() const
{
	return softOrientation_;
}

void Window::dismiss()
{
	onEvent(*this, DismissEvent{});
	drawEvent.detach();
	application().moveOutWindow(*this);
}

int Window::realWidth() const { return isSideways(softOrientation()) ? height() : width(); }

int Window::realHeight() const { return isSideways(softOrientation()) ? width() : height(); }

int Window::width() const { return winSizePixels.x; }

int Window::height() const { return winSizePixels.y; }

IG::Point2D<int> Window::realSize() const { return {realWidth(), realHeight()}; }

IG::Point2D<int> Window::size() const { return winSizePixels; }

bool Window::isPortrait() const
{
	return width() < height();
}

bool Window::isLandscape() const
{
	return !isPortrait();
}

IG::Point2D<float> Window::sizeMM() const
{
	return winSizeMM;
}

IG::Point2D<float> Window::sizeScaledMM() const
{
	if constexpr(Config::envIsAndroid)
	{
		return winSizeSMM;
	}
	return sizeMM();
}

int Window::widthMMInPixels(float mm) const
{
	return (int)std::round(mm * (mmToPixelScaler.x));
}

int Window::heightMMInPixels(float mm) const
{
	return (int)std::round(mm * (mmToPixelScaler.y));
}

int Window::widthScaledMMInPixels(float mm) const
{
	if constexpr(Config::envIsAndroid)
	{
		return (int)std::round(mm * (smmPixelScaler().x));
	}
	return widthMMInPixels(mm);
}

int Window::heightScaledMMInPixels(float mm) const
{
	if constexpr(Config::envIsAndroid)
	{
		return (int)std::round(mm * (smmPixelScaler().y));
	}
	return heightMMInPixels(mm);
}

IG::Point2D<float> BaseWindow::smmPixelScaler() const
{
	return smmToPixelScaler;
}

IG::WindowRect Window::bounds() const
{
	return {{}, size()};
}

IG::Point2D<int> Window::transformInputPos(IG::Point2D<int> srcPos) const
{
	enum class PointerMode {NORMAL, INVERT};
	const auto xPointerTransform = softOrientation() == Rotation::UP || softOrientation() == Rotation::RIGHT ? PointerMode::NORMAL : PointerMode::INVERT;
	const auto yPointerTransform = softOrientation() == Rotation::UP || softOrientation() == Rotation::LEFT ? PointerMode::NORMAL : PointerMode::INVERT;
	const auto pointerAxis = softOrientation() == Rotation::UP || softOrientation() == Rotation::DOWN ? PointerMode::NORMAL : PointerMode::INVERT;

	IG::Point2D<int> pos;
	// x,y axis is swapped first
	pos.x = pointerAxis == PointerMode::INVERT ? srcPos.y : srcPos.x;
	pos.y = pointerAxis == PointerMode::INVERT ? srcPos.x : srcPos.y;

	// then coordinates are inverted
	if(xPointerTransform == PointerMode::INVERT)
		pos.x = width() - pos.x;
	if(yPointerTransform == PointerMode::INVERT)
		pos.y = height() - pos.y;
	return pos;
}

Viewport Window::viewport(WindowRect rect) const
{
	return {bounds(), rect, softOrientation()};
}

Viewport Window::viewport() const
{
	return {bounds(), softOrientation()};
}

Screen &WindowConfig::screen(ApplicationContext ctx) const
{
	return screen_ ? *screen_ : ctx.mainScreen();
}

ApplicationContext Window::appContext() const
{
	return onExit.appContext();
}

Application &Window::application() const
{
	return appContext().application();
}

}
