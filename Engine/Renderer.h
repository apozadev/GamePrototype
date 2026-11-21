#pragma once

#include "RenderStateInfo.h"
#include "ResourceLoader.h"
#include "Mailbox.h"

struct LinearAllocator;

struct ImDrawData;

namespace render
{  

  typedef uint32_t ObjectHandle;  

  void InitRenderer(LinearAllocator* _pAllocator);

  void RenderThreadFnc();

  void RequestRenderThreadExit();

  ObjectHandle CreateRenderObject(MailboxHandle _hMailbox);

  void DestroyRenderObject(MailboxHandle _hMailbox, ObjectHandle _hRenderObj);

  void SetRenderObjectMesh(MailboxHandle _hMailbox, ObjectHandle _hRenderObj, ResourceHandle _hMesh);

  void SetRenderObjectMaterial(MailboxHandle _hMailbox, ObjectHandle _hRenderObj, ResourceHandle _hMaterial);

  void SetMaterialProperty(MailboxHandle _hMailbox, ResourceHandle _hMaterial, const char* _sName, const void* _pData, uint32_t _uSize);  

  void SubmitImGuiDrawData(MailboxHandle _hMailbox, const ImDrawData* _pDrawData);

  void RequestWindowResize();

  bool RequestCreateMesh(ResourceHandle _hHandle, const void* pVertexData, size_t zVertexDataSize, const void* pIndexData, size_t zIndexDataSize);

  bool RequestCreateMaterial(ResourceHandle _hHandle, const RenderStateInfo& _oRenderStateInfo);

  void RequestResourceFailed(ResourceHandle _hHandle, ResourceType _eType);

  void RequestUnloadScene();

  MailboxHandle AcquireMailboxWrite();

  void ReleaseMailboxWrite(MailboxHandle _hMailbox);

} // namespace render
