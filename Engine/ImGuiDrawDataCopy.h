#pragma once

// Hands an ImGui frame from the update thread to the render thread.
//
// ImGui::GetDrawData() points into buffers owned by the ImGui context, which the next
// ImGui::NewFrame on the update thread overwrites, possibly while the render thread is
// still drawing. So the draw data is deep-copied (draw lists, commands, vertices,
// indices) into memory the render thread can keep for the whole frame.

struct ImDrawData;
struct LinearAllocator;

// Copies _pSrc into _pAllocator. Returns nullptr if it doesn't fit (nothing is
// rolled back: reset the allocator to reclaim the partial copy).
//
// The copy is plain arena data: its ImDrawList/ImVector destructors must never run (they
// would free arena memory with IM_FREE), so never delete it, just reset its allocator.
// Command user callbacks are kept, so they run on the render thread.
ImDrawData* CopyImDrawData(LinearAllocator* _pAllocator, const ImDrawData* _pSrc);
