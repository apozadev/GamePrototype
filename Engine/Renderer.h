#pragma once

#include <queue>
#include "RenderStateInfo.h"
#include "ResourceLoader.h"
#include "Mailbox.h"

struct CreateRequestBase
{
  ResourceHandle hHandle;
  ResourceType eType;
};

struct MeshCreateRequest : public CreateRequestBase
{  
  const void* pVertexData;
  size_t zVertexDataSize;
  const void* pIndexData;
  size_t zIndexDataSize;
};

struct MaterialCreateRequest : public CreateRequestBase
{
  // m_uMeshConstantSize is filled by the renderer
  RenderStateInfo oRenderStateInfo;
};

typedef uint32_t RenderObjectHandle;

void RenderThreadFnc();

// Call from the main thread, then join the render thread.
void RequestRenderThreadExit();

// Render object API: update thread only, between AcquireMailboxWrite/ReleaseMailboxWrite.
// Changes reach the render thread as mailbox messages.
RenderObjectHandle CreateRenderObject(MailboxHandle _hMailbox);

void DestroyRenderObject(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj);

void SetRenderObjectMesh(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj, ResourceHandle _hMesh);

void SetRenderObjectMaterial(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj, ResourceHandle _hMaterial);

// Sets a shader constant by its name in the shader (e.g. "color"), in every cbuffer of the material
// that declares it. Affects every render object using this material.
// Ignored if the material is not READY or no shader declares the name. Max 64 bytes (float4x4).
void SetMaterialProperty(MailboxHandle _hMailbox, ResourceHandle _hMaterial, const char* _sName, const void* _pData, uint32_t _uSize);

// Safe to call from the window thread; the render thread applies it next frame.
void RequestWindowResize();

void RequestCreateMesh(const MeshCreateRequest& _oRequest);

void RequestCreateMaterial(const MaterialCreateRequest& _oRequest);

MailboxHandle AcquireMailboxWrite();

void ReleaseMailboxWrite(MailboxHandle _hMailbox);
