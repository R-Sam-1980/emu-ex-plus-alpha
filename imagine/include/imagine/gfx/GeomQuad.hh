#pragma once

#include <imagine/config/defs.hh>
#include <imagine/gfx/Gfx.hh>
#include <imagine/util/rectangle2.h>
#include <imagine/gfx/ProjectionPlane.hh>
#include <imagine/util/edge.h>

namespace Gfx
{

template<class Vtx>
class QuadGeneric
{
public:
	constexpr QuadGeneric() {}
	void init(GC x, GC y, GC x2, GC y2, GC x3, GC y3, GC x4, GC y4);
	void deinit();
	void setPos(GC x, GC y, GC x2, GC y2, GC x3, GC y3, GC x4, GC y4);
	void draw(RendererCommands &r) const;

	// as rectangle
	void init(GC x, GC y, GC x2, GC y2)
	{
		return init(x, y,  x, y2,  x2, y2,  x2, y);
	}

	void init(const GCRect &d)
	{
		return init(d.x, d.y, d.x2, d.y2);
	}

	void setPos(GC x, GC y, GC x2, GC y2)
	{
		setPos(x, y,  x, y2,  x2, y2,  x2, y);
	}

	void setPos(const QuadGeneric &quad)
	{
		v = quad.v;
	}

	void setPos(const IG::WindowRect &b, const ProjectionPlane &proj)
	{
		auto pos = proj.unProjectRect(b);
		setPos(pos.x, pos.y, pos.x2, pos.y2);
	}

	void setPos(const GCRect &r)
	{
		setPos(r.x, r.y, r.x2, r.y2);
	}

	void setPosRel(GC x, GC y, GC xSize, GC ySize)
	{
		setPos(x, y, x+xSize, y+ySize);
	}

	static void draw(RendererCommands &cmds, const IG::WindowRect &b, const ProjectionPlane &proj)
	{
		draw(cmds, proj.unProjectRect(b));
	}

	static void draw(RendererCommands &cmds, const GCRect &d)
	{
		QuadGeneric<Vtx> rect;
		rect.init(d);
		rect.draw(cmds);
	}

protected:
	std::array<Vtx, 4> v;
};

using Quad = QuadGeneric<Vertex>;

class TexQuad : public QuadGeneric<TexVertex>
{
public:
	constexpr TexQuad() { }
	void mapImg(GTexC leftTexU, GTexC topTexV, GTexC rightTexU, GTexC bottomTexV);
};

class ColQuad : public QuadGeneric<ColVertex>
{
public:
	constexpr ColQuad() { }
	void setColor(ColorComp r, ColorComp g, ColorComp b, ColorComp a, uint edges = EDGE_AI);
	void setColorRGB(ColorComp r, ColorComp g, ColorComp b, uint edges = EDGE_AI);
	void setColorAlpha(ColorComp a, uint edges = EDGE_AI);
};

class ColTexQuad : public QuadGeneric<ColTexVertex>
{
public:
	constexpr ColTexQuad() { }
	void mapImg(GTexC leftTexU, GTexC topTexV, GTexC rightTexU, GTexC bottomTexV);
	void setColor(ColorComp r, ColorComp g, ColorComp b, ColorComp a, uint edges = EDGE_AI);
	void setColorRGB(ColorComp r, ColorComp g, ColorComp b, uint edges = EDGE_AI);
	void setColorAlpha(ColorComp a, uint edges = EDGE_AI);
};

std::array<Vertex, 4> makeVertArray(GCRect pos);
std::array<ColVertex, 4> makeColVertArray(GCRect pos, VertexColor col);
std::array<VertexIndex, 6> makeRectIndexArray(VertexIndex baseIdx);

template<class Vtx>
static void drawQuads(RendererCommands &cmds, std::array<Vtx, 4> *quad, uint quads, std::array<VertexIndex, 6> *quadIdx, uint quadIdxs)
{
	cmds.bindTempVertexBuffer();
	cmds.vertexBufferData(quad[0].data(), sizeof(quad[0]) * quads);
	Vtx::bindAttribs(cmds, quad[0].data());
	cmds.drawPrimitiveElements(Primitive::TRIANGLE, quadIdx[0].data(), quadIdxs * 6);
}

}
