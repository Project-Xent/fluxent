#ifndef FLUX_DCOMP_H
#define FLUX_DCOMP_H

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <unknwn.h>
#include <windef.h>
#include <winbase.h>

DEFINE_GUID(IID_IDCompositionDevice, 0xc37ea93a, 0xe7aa, 0x450d, 0xb1, 0x6f, 0x97, 0x46, 0xcb, 0x04, 0x07, 0xf3);

typedef struct IDCompositionDevice IDCompositionDevice;
typedef struct IDCompositionTarget IDCompositionTarget;
typedef struct IDCompositionVisual IDCompositionVisual;

typedef struct IDCompositionTargetVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(IDCompositionTarget *This, REFIID riid, void **ppv);
	ULONG(STDMETHODCALLTYPE *AddRef)(IDCompositionTarget *This);
	ULONG(STDMETHODCALLTYPE *Release)(IDCompositionTarget *This);
	HRESULT(STDMETHODCALLTYPE *SetRoot)(IDCompositionTarget *This, IDCompositionVisual *visual);
} IDCompositionTargetVtbl;

struct IDCompositionTarget {
	IDCompositionTargetVtbl const *lpVtbl;
};

#define IDCompositionTarget_QueryInterface(p, a, b) (p)->lpVtbl->QueryInterface(p, a, b)
#define IDCompositionTarget_AddRef(p)               (p)->lpVtbl->AddRef(p)
#define IDCompositionTarget_Release(p)              (p)->lpVtbl->Release(p)
#define IDCompositionTarget_SetRoot(p, a)           (p)->lpVtbl->SetRoot(p, a)

typedef struct IDCompositionVisualVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(IDCompositionVisual *This, REFIID riid, void **ppv);
	ULONG(STDMETHODCALLTYPE *AddRef)(IDCompositionVisual *This);
	ULONG(STDMETHODCALLTYPE *Release)(IDCompositionVisual *This);
	HRESULT(STDMETHODCALLTYPE *SetOffsetX_Anim)(IDCompositionVisual *This, void *animation);
	HRESULT(STDMETHODCALLTYPE *SetOffsetX)(IDCompositionVisual *This, float offsetX);
	HRESULT(STDMETHODCALLTYPE *SetOffsetY_Anim)(IDCompositionVisual *This, void *animation);
	HRESULT(STDMETHODCALLTYPE *SetOffsetY)(IDCompositionVisual *This, float offsetY);
	HRESULT(STDMETHODCALLTYPE *SetTransform_Anim)(IDCompositionVisual *This, void *transform);
	HRESULT(STDMETHODCALLTYPE *SetTransform)(IDCompositionVisual *This, void const *matrix);
	HRESULT(STDMETHODCALLTYPE *SetTransformParent)(IDCompositionVisual *This, IDCompositionVisual *visual);
	HRESULT(STDMETHODCALLTYPE *SetEffect)(IDCompositionVisual *This, void *effect);
	HRESULT(STDMETHODCALLTYPE *SetBitmapInterpolationMode)(IDCompositionVisual *This, int mode);
	HRESULT(STDMETHODCALLTYPE *SetBorderMode)(IDCompositionVisual *This, int mode);
	HRESULT(STDMETHODCALLTYPE *SetClip_Anim)(IDCompositionVisual *This, void *clip);
	HRESULT(STDMETHODCALLTYPE *SetClip)(IDCompositionVisual *This, void const *rect);
	HRESULT(STDMETHODCALLTYPE *SetContent)(IDCompositionVisual *This, IUnknown *content);
	HRESULT(STDMETHODCALLTYPE *AddVisual)(
	  IDCompositionVisual *This, IDCompositionVisual *visual, BOOL insert_above, IDCompositionVisual *ref
	);
	HRESULT(STDMETHODCALLTYPE *RemoveVisual)(IDCompositionVisual *This, IDCompositionVisual *visual);
	HRESULT(STDMETHODCALLTYPE *RemoveAllVisuals)(IDCompositionVisual *This);
	HRESULT(STDMETHODCALLTYPE *SetCompositeMode)(IDCompositionVisual *This, int mode);
} IDCompositionVisualVtbl;

struct IDCompositionVisual {
	IDCompositionVisualVtbl const *lpVtbl;
};

#define IDCompositionVisual_QueryInterface(p, a, b) (p)->lpVtbl->QueryInterface(p, a, b)
#define IDCompositionVisual_AddRef(p)               (p)->lpVtbl->AddRef(p)
#define IDCompositionVisual_Release(p)              (p)->lpVtbl->Release(p)
#define IDCompositionVisual_SetContent(p, a)        (p)->lpVtbl->SetContent(p, a)
#define IDCompositionVisual_AddVisual(p, a, b, c)   (p)->lpVtbl->AddVisual(p, a, b, c)
#define IDCompositionVisual_RemoveVisual(p, a)      (p)->lpVtbl->RemoveVisual(p, a)
#define IDCompositionVisual_RemoveAllVisuals(p)     (p)->lpVtbl->RemoveAllVisuals(p)

typedef struct IDCompositionDeviceVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(IDCompositionDevice *This, REFIID riid, void **ppv);
	ULONG(STDMETHODCALLTYPE *AddRef)(IDCompositionDevice *This);
	ULONG(STDMETHODCALLTYPE *Release)(IDCompositionDevice *This);
	HRESULT(STDMETHODCALLTYPE *Commit)(IDCompositionDevice *This);
	HRESULT(STDMETHODCALLTYPE *WaitForCommitCompletion)(IDCompositionDevice *This);
	HRESULT(STDMETHODCALLTYPE *GetFrameStatistics)(IDCompositionDevice *This, void *stats);
	HRESULT(STDMETHODCALLTYPE *CreateTargetForHwnd)(
	  IDCompositionDevice *This, HWND hwnd, BOOL topmost, IDCompositionTarget **target
	);
	HRESULT(STDMETHODCALLTYPE *CreateVisual)(IDCompositionDevice *This, IDCompositionVisual **visual);
} IDCompositionDeviceVtbl;

struct IDCompositionDevice {
	IDCompositionDeviceVtbl const *lpVtbl;
};

#define IDCompositionDevice_QueryInterface(p, a, b)         (p)->lpVtbl->QueryInterface(p, a, b)
#define IDCompositionDevice_AddRef(p)                       (p)->lpVtbl->AddRef(p)
#define IDCompositionDevice_Release(p)                      (p)->lpVtbl->Release(p)
#define IDCompositionDevice_Commit(p)                       (p)->lpVtbl->Commit(p)
#define IDCompositionDevice_WaitForCommitCompletion(p)      (p)->lpVtbl->WaitForCommitCompletion(p)
#define IDCompositionDevice_CreateTargetForHwnd(p, a, b, c) (p)->lpVtbl->CreateTargetForHwnd(p, a, b, c)
#define IDCompositionDevice_CreateVisual(p, a)              (p)->lpVtbl->CreateVisual(p, a)

HRESULT WINAPI DCompositionCreateDevice(IDXGIDevice *dxgiDevice, REFIID iid, void **dcompositionDevice);

#endif
