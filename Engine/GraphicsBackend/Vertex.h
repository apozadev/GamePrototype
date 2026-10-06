#pragma once

struct VtxVec3
{
  float x, y, z;
};

struct Vertex
{
  VtxVec3 m_vPos;  
  VtxVec3 m_vNormal;  
  VtxVec3 m_vTangent;  
  VtxVec3 m_vColor;
  VtxVec3 m_vUv;
};