#pragma once

#include "RenderStateInfo.h"
#include "ResourceLoader.h"
#include "Mailbox.h"

struct CreateRequestBase
{
  ResourceHandle hHandle;
  ResourceType eType;
  bool bLoadFailed; // Set by RequestResourceFailed: nothing to create
};

// RequestCreateMesh copies the data it points to: it only has to be valid during the call.
struct MeshCreateRequest : public CreateRequestBase
{
  const void* pVertexData;
  size_t zVertexDataSize;
  const void* pIndexData;
  size_t zIndexDataSize; // 16-bit indices, so the index count is zIndexDataSize / sizeof(uint16_t)
};

// RequestCreateMaterial copies the shader filenames (null-terminated copies, so the
// backends can use them as C strings): they only have to be valid during the call.
struct MaterialCreateRequest : public CreateRequestBase
{
  // m_uMeshConstantSize is filled by the renderer
  RenderStateInfo oRenderStateInfo;
};

typedef uint32_t RenderObjectHandle;

struct LinearAllocator;

// Allocates the create request ring from _pAllocator, which must outlive the render thread.
// Call once from the main thread before starting the render or loader threads.
void InitRenderer(LinearAllocator* _pAllocator);

void RenderThreadFnc();

// Call from the main thread, then join the render thread.
void RequestRenderThreadExit();

// Render object API: update thread only, between AcquireMailboxWrite/ReleaseMailboxWrite.
// Changes reach the render thread as mailbox messages.
// Meshes and materials can be set while still loading: the object is drawn once both are
// created, with the error mesh/material in place of a FAILED one.

// Returns 0 if the mailbox is full (the object is not created; try again next frame).
RenderObjectHandle CreateRenderObject(MailboxHandle _hMailbox);

void DestroyRenderObject(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj);

void SetRenderObjectMesh(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj, ResourceHandle _hMesh);

void SetRenderObjectMaterial(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj, ResourceHandle _hMaterial);

// Sets a shader constant by its name in the shader (e.g. "color"), in every cbuffer of the material
// that declares it. Affects every render object using this material.
// Set while the material is LOADING, it is applied once it is created. Ignored if the material
// FAILED or no shader declares the name. Max 64 bytes (float4x4).
void SetMaterialProperty(MailboxHandle _hMailbox, ResourceHandle _hMaterial, const char* _sName, const void* _pData, uint32_t _uSize);

struct ImDrawData;

// Update thread, between AcquireMailboxWrite/ReleaseMailboxWrite, after ImGui::Render().
// Copies the draw data (the next ImGui::NewFrame overwrites the original); the render
// thread draws the copy over that frame's scene. Call at most once per mailbox.
// If the copy doesn't fit, it logs and that frame has no ImGui.
void SubmitImGuiDrawData(MailboxHandle _hMailbox, const ImDrawData* _pDrawData);

// Safe to call from the window thread; the render thread applies it next frame.
void RequestWindowResize();

// Loader thread only (the create request ring has a single producer).
// Copy the request and its data into the create request ring, blocking while it is full.
// Returns false (logged) if the request is too big to ever fit in the ring (increase CREATE_REQUEST_RING_SIZE).
bool RequestCreateMesh(const MeshCreateRequest& _oRequest);

bool RequestCreateMaterial(const MaterialCreateRequest& _oRequest);

// Loader thread only. The resource couldn't be loaded: the render thread moves it to FAILED
// and draws whatever uses it with the error mesh/material.
void RequestResourceFailed(ResourceHandle _hHandle, ResourceType _eType);

MailboxHandle AcquireMailboxWrite();

void ReleaseMailboxWrite(MailboxHandle _hMailbox);
