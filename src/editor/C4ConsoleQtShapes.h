/*
* OpenClonk, http://www.openclonk.org
*
* Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de/
* Copyright (c) 2013, The OpenClonk Team and contributors
*
* Distributed under the terms of the ISC license; see accompanying file
* "COPYING" for details.
*
* "Clonk" is a registered trademark of Matthes Bender, used with permission.
* See accompanying file "TRADEMARK" for details.
*
* To redistribute this file separately, substitute the full license texts
* for the above references.
*/

/* Editable shapes in the viewports (like e.g. AI guard range rectangles) */

#ifndef INC_C4ConsoleQtShapes
#define INC_C4ConsoleQtShapes
#ifdef WITH_QT_EDITOR

#include "editor/C4ConsoleGUI.h" // for glew.h
#include "editor/C4ConsoleQt.h"
#include "script/C4Value.h"

// Shape base class
class C4ConsoleQtShape : public QObject
{
	Q_OBJECT
protected:
	C4Value rel_obj; // Object relative to which shape is defined
	bool is_relative;
	int32_t dragging_border;
	uint32_t border_color;
	const class C4PropertyDelegateShape *parent_delegate;

protected:
	// Return shape color, or dragged border color if index is the border currently being dragged
	uint32_t GetBorderColor(int32_t border_index, bool dragging_border_is_bitmask, uint32_t default_color = 0u) const;
public:
	C4ConsoleQtShape(class C4Object *for_obj, C4PropList *props, const class C4PropertyDelegateShape *parent_delegate);

	virtual bool IsHit(int32_t x, int32_t y, int32_t hit_range, Qt::CursorShape *drag_cursor, int32_t *drag_border, bool shift_down, bool ctrl_down) = 0;
	virtual void Draw(class C4TargetFacet &cgo, float line_width) = 0;

	// Coordinate transform: Add object
	int32_t AbsX(int32_t rel_x=0) const;
	int32_t AbsY(int32_t rel_y=0) const;

	// Start/stop dragging
	virtual bool StartDragging(int32_t border, int32_t x, int32_t y, bool shift_down, bool ctrl_down) { dragging_border = border; return true; }
	virtual void StopDragging();
	virtual void Drag(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t hit_range, Qt::CursorShape *drag_cursor) = 0;
	bool IsDragging() const { return dragging_border != -1; }

	virtual void SetValue(const C4Value &val) = 0;
	virtual C4Value GetValue() const = 0; 	// Return current shape as C4Value to be stored back to property

	const class C4PropertyDelegateShape *GetParentDelegate() const { return parent_delegate; }

signals:
	void ShapeDragged();
};	

// Rectangular shape
class C4ConsoleQtRect : public C4ConsoleQtShape
{
private:
	int32_t left, top, right, bottom;
	bool store_as_proplist;
	bool properties_lowercase;
public:
	C4ConsoleQtRect(class C4Object *for_obj, C4PropList *props, const class C4PropertyDelegateShape *parent_delegate);

	bool IsHit(int32_t x, int32_t y, int32_t hit_range, Qt::CursorShape *drag_cursor, int32_t *drag_border, bool shift_down, bool ctrl_down) override;
	void Draw(class C4TargetFacet &cgo, float line_width) override;
	void Drag(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t hit_range, Qt::CursorShape *drag_cursor) override;

	void SetValue(const C4Value &val) override;
	C4Value GetValue() const override;
};

// Circular shape
class C4ConsoleQtCircle : public C4ConsoleQtShape
{
private:
	int32_t radius;
	int32_t cx, cy;
	bool can_move_center;
public:
	C4ConsoleQtCircle(class C4Object *for_obj, C4PropList *props, const class C4PropertyDelegateShape *parent_delegate);

	bool IsHit(int32_t x, int32_t y, int32_t hit_range, Qt::CursorShape *drag_cursor, int32_t *drag_border, bool shift_down, bool ctrl_down) override;
	void Draw(class C4TargetFacet &cgo, float line_width) override;
	void Drag(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t hit_range, Qt::CursorShape *drag_cursor) override;

	void SetValue(const C4Value &val) override;
	C4Value GetValue() const override;
};

// Single position shape
class C4ConsoleQtPoint : public C4ConsoleQtShape
{
private:
	int32_t cx, cy;
public:
	C4ConsoleQtPoint(class C4Object *for_obj, C4PropList *props, const class C4PropertyDelegateShape *parent_delegate);

	bool IsHit(int32_t x, int32_t y, int32_t hit_range, Qt::CursorShape *drag_cursor, int32_t *drag_border, bool shift_down, bool ctrl_down) override;
	void Draw(class C4TargetFacet &cgo, float line_width) override;
	void Drag(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t hit_range, Qt::CursorShape *drag_cursor) override;

	void SetValue(const C4Value &val) override;
	C4Value GetValue() const override;
};

// Vertices and edges
class C4ConsoleQtGraph : public C4ConsoleQtShape
{
protected:
	struct Vertex
	{
		int32_t x, y;
		uint32_t color; // 0 = default color

		Vertex() : x(0), y(0), color(0u) {}
	};

	struct Edge
	{
		int32_t vertex_indices[2]; // index into vertices array
		uint32_t color; // 0 = default color
		uint32_t line_thickness;

		Edge() : color(0u), line_thickness(1) { vertex_indices[0] = vertex_indices[1] = -1; }
		bool connects_to(int32_t vertex_index) const;
		bool connects_to(int32_t vertex_index, int32_t *idx) const;
	};

	std::vector<Vertex> vertices;
	std::vector<Edge> edges;
	bool store_as_proplist;
	bool properties_lowercase;
	bool allow_edge_selection; // If edges on the graph can be selected

	// Drag snap to other vertices
	int32_t drag_snap_offset_x = 0, drag_snap_offset_y = 0;
	bool drag_snapped = false;
	int32_t drag_snap_vertex = -1, drag_source_vertex_index = -1;

	// Resolve border_index to vertices/edges: Use negative indices for edges and zero/positive indices for vertices
	static bool IsVertexDrag(int32_t border) { return border >= 0; }
	static bool IsEdgeDrag(int32_t border) { return border < -1; }
	static int32_t DragBorderToVertex(int32_t border) { return border; }
	static int32_t DragBorderToEdge(int32_t border) { return -2 - border; }
	static int32_t VertexToDragBorder(int32_t vertex) { return vertex; }
	static int32_t EdgeToDragBorder(int32_t edge) { return -edge - 2; }
public:
	C4ConsoleQtGraph(class C4Object *for_obj, C4PropList *props, const class C4PropertyDelegateShape *parent_delegate);

	bool IsHit(int32_t x, int32_t y, int32_t hit_range, Qt::CursorShape *drag_cursor, int32_t *drag_border, bool shift_down, bool ctrl_down) override;
	void Draw(class C4TargetFacet &cgo, float line_width) override;
	void Drag(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t hit_range, Qt::CursorShape *drag_cursor) override;
	bool StartDragging(int32_t border, int32_t x, int32_t y, bool shift_down, bool ctrl_down) override;
	void StopDragging() override;

	void SetValue(const C4Value &val) override;
	C4Value GetValue() const override;

protected:
	void InsertVertexBefore(int32_t insert_vertex_index, int32_t x, int32_t y);
	void InsertEdgeBefore(int32_t insert_edge_index, int32_t vertex1, int32_t vertex2);
	virtual int32_t InsertVertexOnEdge(int32_t split_edge_index, int32_t x, int32_t y);
	virtual int32_t InsertVertexOnVertex(int32_t target_vertex_index, int32_t x, int32_t y);
	virtual void RemoveEdge(int32_t edge_index);
	virtual void RemoveVertex(int32_t vertex_index, bool create_skip_connection);
	int32_t GetEdgeCountForVertex(int32_t vertex_index) const;

	virtual bool IsPolyline() const { return false; }

	C4ValueArray *GetVerticesValue() const;
	C4ValueArray *GetEdgesValue() const;
	void SetVerticesValue(const C4ValueArray *vvertices);
	void SetEdgesValue(const C4ValueArray *vedges);

	// Check if given vertex/edge can be modified under given shift state
	virtual bool IsVertexHit(int32_t vertex_index, Qt::CursorShape *drag_cursor, bool shift_down, bool ctrl_down);
	virtual bool IsEdgeHit(int32_t edge_index, Qt::CursorShape *drag_cursor, bool shift_down, bool ctrl_down);

};

// Specialization of graph: One line of connected vertices
class C4ConsoleQtPolyline : public C4ConsoleQtGraph
{
public:
	C4ConsoleQtPolyline(class C4Object *for_obj, C4PropList *props, const class C4PropertyDelegateShape *parent_delegate)
		: C4ConsoleQtGraph(for_obj, props, parent_delegate) {}

	void SetValue(const C4Value &val) override;
	C4Value GetValue() const override;

protected:
	int32_t InsertVertexOnEdge(int32_t split_edge_index, int32_t x, int32_t y) override;
	int32_t InsertVertexOnVertex(int32_t target_vertex_index, int32_t x, int32_t y) override;
	void RemoveEdge(int32_t edge_index) override;

	bool IsVertexHit(int32_t vertex_index, Qt::CursorShape *drag_cursor, bool shift_down, bool ctrl_down) override;
	bool IsPolyline() const override { return true; }
};

// Specialization of graph: One closed line of connected vertices
class C4ConsoleQtPolygon : public C4ConsoleQtPolyline
{
public:
	C4ConsoleQtPolygon(class C4Object *for_obj, C4PropList *props, const class C4PropertyDelegateShape *parent_delegate)
		: C4ConsoleQtPolyline(for_obj, props, parent_delegate) {}

	void SetValue(const C4Value &val) override;

protected:
	int32_t InsertVertexOnEdge(int32_t split_edge_index, int32_t x, int32_t y) override;
	int32_t InsertVertexOnVertex(int32_t target_vertex_index, int32_t x, int32_t y) override;
	void RemoveEdge(int32_t edge_index) override;

	bool IsVertexHit(int32_t vertex_index, Qt::CursorShape *drag_cursor, bool shift_down, bool ctrl_down) override;
};

/* List of all current editable Qt shapes */
class C4ConsoleQtShapes
{
	typedef std::list<std::unique_ptr<C4ConsoleQtShape> > ShapeList;
	ShapeList shapes;
	C4ConsoleQtShape *dragging_shape;
	Qt::CursorShape drag_cursor;
	float drag_x, drag_y;
public:
	C4ConsoleQtShapes() : dragging_shape(nullptr), drag_x(0), drag_y(0), drag_cursor(Qt::CursorShape::ArrowCursor) { }

	C4ConsoleQtShape *CreateShape(class C4Object *for_obj, C4PropList *props, const C4Value &val, const class C4PropertyDelegateShape *parent_delegate);
	void AddShape(C4ConsoleQtShape *shape);
	void RemoveShape(C4ConsoleQtShape *shape);
	void ClearShapes();

	// Mouse callbacks from viewports to execute shape dragging
	bool MouseDown(float x, float y, float hit_range, bool shift_down, bool ctrl_down); // return true if a shape was hit
	void MouseMove(float x, float y, bool left_down, float hit_range, bool shift_down, bool ctrl_down); // move move: Execute shape dragging
	void MouseUp(float x, float y, bool shift_down, bool ctrl_down);

	void Draw(C4TargetFacet &cgo);

	// Dragging info
	bool HasDragCursor() const { return drag_cursor != Qt::CursorShape::ArrowCursor; }
	Qt::CursorShape GetDragCursor() const { return drag_cursor; }
};

/* Shape holder class: Handles adding/removal of shape to shapes list */
class C4ConsoleQtShapeHolder
{
	C4ConsoleQtShape *shape;

public:
	C4ConsoleQtShapeHolder() : shape(nullptr) {}
	~C4ConsoleQtShapeHolder() { Clear(); }

	void Clear();
	void Set(C4ConsoleQtShape *new_shape);
	C4ConsoleQtShape *Get() const { return shape; }
};


#endif // WITH_QT_EDITOR
#endif // INC_C4ConsoleQtShapes