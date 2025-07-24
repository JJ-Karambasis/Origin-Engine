#ifndef MESH_H
#define MESH_H

struct vtx_attrib {
	v3 Normal;
	v2 UV;
};

struct mesh_part {
	u32 VertexAt;
	u32 VertexCount;
	u32 IndexAt;
	u32 IndexCount;
};

struct editable_mesh {
	memory_reserve BaseReserve;
	memory_reserve PositionReserve;
	memory_reserve NormalReserve;
	memory_reserve UVReserve;
	memory_reserve IndicesReserve;
	memory_reserve PartReserve;
	v3*            Positions;
	v3*            Normals;
	v2*            UVs;
	mesh_part*     Parts;
	u32*           Indices;
	u32 		   VertexCount;
	u32 		   IndexCount;
	u32 		   PartCount;
};

struct mesh_edge {
	u32 Idx0;
	u32 Idx1;
};

struct mesh_edge_table_slot_entry {
	u32 	  Hash;
	mesh_edge Key;
	u32       Value;
	mesh_edge_table_slot_entry* Next;
};

struct mesh_edge_table_slot {
	mesh_edge_table_slot_entry* First;
	mesh_edge_table_slot_entry* Last;
};

struct mesh_edge_table {
	arena*          	  Arena;
	u32 				  SlotCapacity;
	mesh_edge_table_slot* Slots;
};


#endif