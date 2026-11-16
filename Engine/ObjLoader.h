#pragma once

#include <cstddef>
#include <cstdint>

struct LinearAllocator;
struct Vertex;

struct ObjMesh
{
  Vertex* pVertices;
  uint32_t uVertexCount;
  uint16_t* pIndices;
  uint32_t uIndexCount;
};

// Builds an indexed triangle list from OBJ text (v, vt, vn and f; polygons are fan triangulated,
// everything else is ignored and all groups end up in one mesh). Texture v is flipped to a top-left origin.
// The result is allocated from _pAllocator. Returns false (logged) on malformed input, more than
// 65536 unique vertices or not enough memory; _pAllocator is left untouched then.
bool ParseObj(const char* _pText, size_t _zSize, LinearAllocator* _pAllocator, ObjMesh& oMesh_);
