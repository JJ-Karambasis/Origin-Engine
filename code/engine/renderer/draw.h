#ifndef DRAW_H
#define DRAW_H

enum {
	DRAW_FLAG_NONE,
	DRAW_FLAG_LINE,
};
typedef u32 draw_flags;

struct draw_primitive {
	gfx_mesh* Mesh;
	m4_affine Transform;
	v4 		  Color;
};

Dynamic_Array_Define_Type(draw_primitive);

struct draw_primitives {
	gfx_texture_id DepthBuffer;

	gfx_mesh* BoxLineMesh;
	gfx_mesh* SphereLineMesh;

	gfx_mesh* SphereTriangleMesh;
	gfx_mesh* ConeTriangleMesh;
	gfx_mesh* CylinderTriangleMesh;

	dynamic_draw_primitive_array OpaqueDraws;
	dynamic_draw_primitive_array LineDraws;

	v2 LastDim;
};

#endif