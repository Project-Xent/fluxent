/**
 * @file flux_interop.h
 * @brief Hand-declared Windows.UI.Composition interop COM interfaces.
 *
 * cwinrt projects the WinRT runtime classes but not the classic-COM interop
 * interfaces that bridge Composition to Direct3D/Direct2D and Win32 HWNDs.
 * These are declared here, in C, with vtable layouts and IIDs taken verbatim
 * from the Windows SDK header `winrt/windows.ui.composition.interop.h`
 * (10.0.26100.0). The method order is ABI-critical: it must match the SDK
 * exactly or calls dispatch to the wrong slot.
 *
 * Return parameters that the SDK types as projected Composition interfaces
 * (ICompositionSurface, ICompositionGraphicsDevice, IDesktopWindowTarget) are
 * declared `void**` here; callers QI the result to the cwinrt `WUC_*` type via
 * cwinrt_query.
 */
#ifndef FLUX_COMPOSE_INTEROP_H
#define FLUX_COMPOSE_INTEROP_H

#include <windows.h>
#include <unknwn.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- ICompositorInterop {25297D5C-3AD4-4C9C-B5CF-E36A38512330} ---- */
typedef struct FluxICompositorInterop FluxICompositorInterop;

typedef struct FluxICompositorInteropVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(FluxICompositorInterop *, REFIID, void **);
	ULONG(STDMETHODCALLTYPE *AddRef)(FluxICompositorInterop *);
	ULONG(STDMETHODCALLTYPE *Release)(FluxICompositorInterop *);
	HRESULT(STDMETHODCALLTYPE *CreateCompositionSurfaceForHandle)(FluxICompositorInterop *, HANDLE, void **);
	HRESULT(STDMETHODCALLTYPE *CreateCompositionSurfaceForSwapChain)(FluxICompositorInterop *, IUnknown *, void **);
	HRESULT(STDMETHODCALLTYPE *CreateGraphicsDevice)(FluxICompositorInterop *, IUnknown *, void **);
} FluxICompositorInteropVtbl;

struct FluxICompositorInterop {
	FluxICompositorInteropVtbl const *lpVtbl;
};

static const GUID FluxIID_ICompositorInterop = {
  0x25297d5c, 0x3ad4, 0x4c9c, {0xb5, 0xcf, 0xe3, 0x6a, 0x38, 0x51, 0x23, 0x30}
};

/* ---- ICompositionGraphicsDeviceInterop {A116FF71-F8BF-4C8A-9C98-70779A32A9C8} ---- */
typedef struct FluxICompositionGraphicsDeviceInterop FluxICompositionGraphicsDeviceInterop;

typedef struct FluxICompositionGraphicsDeviceInteropVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(FluxICompositionGraphicsDeviceInterop *, REFIID, void **);
	ULONG(STDMETHODCALLTYPE *AddRef)(FluxICompositionGraphicsDeviceInterop *);
	ULONG(STDMETHODCALLTYPE *Release)(FluxICompositionGraphicsDeviceInterop *);
	HRESULT(STDMETHODCALLTYPE *GetRenderingDevice)(FluxICompositionGraphicsDeviceInterop *, IUnknown **);
	HRESULT(STDMETHODCALLTYPE *SetRenderingDevice)(FluxICompositionGraphicsDeviceInterop *, IUnknown *);
} FluxICompositionGraphicsDeviceInteropVtbl;

struct FluxICompositionGraphicsDeviceInterop {
	FluxICompositionGraphicsDeviceInteropVtbl const *lpVtbl;
};

static const GUID FluxIID_ICompositionGraphicsDeviceInterop = {
  0xa116ff71, 0xf8bf, 0x4c8a, {0x9c, 0x98, 0x70, 0x77, 0x9a, 0x32, 0xa9, 0xc8}
};

/* ---- ICompositionDrawingSurfaceInterop {FD04E6E3-FE0C-4C3C-AB19-A07601A576EE} ----
 * Method order per SDK: BeginDraw, EndDraw, Resize, Scroll, ResumeDraw, SuspendDraw. */
typedef struct FluxICompositionDrawingSurfaceInterop FluxICompositionDrawingSurfaceInterop;

typedef struct FluxICompositionDrawingSurfaceInteropVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(FluxICompositionDrawingSurfaceInterop *, REFIID, void **);
	ULONG(STDMETHODCALLTYPE *AddRef)(FluxICompositionDrawingSurfaceInterop *);
	ULONG(STDMETHODCALLTYPE *Release)(FluxICompositionDrawingSurfaceInterop *);
	HRESULT(STDMETHODCALLTYPE *BeginDraw)(
	  FluxICompositionDrawingSurfaceInterop *, const RECT *, REFIID, void **, POINT *
	);
	HRESULT(STDMETHODCALLTYPE *EndDraw)(FluxICompositionDrawingSurfaceInterop *);
	HRESULT(STDMETHODCALLTYPE *Resize)(FluxICompositionDrawingSurfaceInterop *, SIZE);
	HRESULT(STDMETHODCALLTYPE *Scroll)(FluxICompositionDrawingSurfaceInterop *, const RECT *, const RECT *, int, int);
	HRESULT(STDMETHODCALLTYPE *ResumeDraw)(FluxICompositionDrawingSurfaceInterop *);
	HRESULT(STDMETHODCALLTYPE *SuspendDraw)(FluxICompositionDrawingSurfaceInterop *);
} FluxICompositionDrawingSurfaceInteropVtbl;

struct FluxICompositionDrawingSurfaceInterop {
	FluxICompositionDrawingSurfaceInteropVtbl const *lpVtbl;
};

static const GUID FluxIID_ICompositionDrawingSurfaceInterop = {
  0xfd04e6e3, 0xfe0c, 0x4c3c, {0xab, 0x19, 0xa0, 0x76, 0x01, 0xa5, 0x76, 0xee}
};

/* ---- ICompositorDesktopInterop {29E691FA-4567-4DCA-B319-D0F207EB6807} ---- */
typedef struct FluxICompositorDesktopInterop FluxICompositorDesktopInterop;

typedef struct FluxICompositorDesktopInteropVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(FluxICompositorDesktopInterop *, REFIID, void **);
	ULONG(STDMETHODCALLTYPE *AddRef)(FluxICompositorDesktopInterop *);
	ULONG(STDMETHODCALLTYPE *Release)(FluxICompositorDesktopInterop *);
	HRESULT(STDMETHODCALLTYPE *CreateDesktopWindowTarget)(FluxICompositorDesktopInterop *, HWND, BOOL, void **);
	HRESULT(STDMETHODCALLTYPE *EnsureOnThread)(FluxICompositorDesktopInterop *, DWORD);
} FluxICompositorDesktopInteropVtbl;

struct FluxICompositorDesktopInterop {
	FluxICompositorDesktopInteropVtbl const *lpVtbl;
};

static const GUID FluxIID_ICompositorDesktopInterop = {
  0x29e691fa, 0x4567, 0x4dca, {0xb3, 0x19, 0xd0, 0xf2, 0x07, 0xeb, 0x68, 0x07}
};

/* ---- IGraphicsEffectD2D1Interop {2FC57384-A068-44D7-A331-30982FCF7177} ----
 * Implemented (not consumed) by Fluxent's effect-description objects so the WUC
 * effect factory can read the D2D effect id, properties, and sources. */
typedef enum FluxGraphicsEffectPropertyMapping
{
	FLUX_GFX_EFFECT_PROP_UNKNOWN = 0,
	FLUX_GFX_EFFECT_PROP_DIRECT,
	FLUX_GFX_EFFECT_PROP_VECTORX,
	FLUX_GFX_EFFECT_PROP_VECTORY,
	FLUX_GFX_EFFECT_PROP_VECTORZ,
	FLUX_GFX_EFFECT_PROP_VECTORW,
	FLUX_GFX_EFFECT_PROP_RECT_TO_VECTOR4,
	FLUX_GFX_EFFECT_PROP_RADIANS_TO_DEGREES,
	FLUX_GFX_EFFECT_PROP_COLORMATRIX_ALPHA_MODE,
	FLUX_GFX_EFFECT_PROP_COLOR_TO_VECTOR3,
	FLUX_GFX_EFFECT_PROP_COLOR_TO_VECTOR4,
} FluxGraphicsEffectPropertyMapping;

typedef struct FluxIGraphicsEffectD2D1Interop FluxIGraphicsEffectD2D1Interop;

typedef struct FluxIGraphicsEffectD2D1InteropVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(FluxIGraphicsEffectD2D1Interop *, REFIID, void **);
	ULONG(STDMETHODCALLTYPE *AddRef)(FluxIGraphicsEffectD2D1Interop *);
	ULONG(STDMETHODCALLTYPE *Release)(FluxIGraphicsEffectD2D1Interop *);
	HRESULT(STDMETHODCALLTYPE *GetEffectId)(FluxIGraphicsEffectD2D1Interop *, GUID *);
	HRESULT(STDMETHODCALLTYPE *GetNamedPropertyMapping)(
	  FluxIGraphicsEffectD2D1Interop *, LPCWSTR, UINT *, FluxGraphicsEffectPropertyMapping *
	);
	HRESULT(STDMETHODCALLTYPE *GetPropertyCount)(FluxIGraphicsEffectD2D1Interop *, UINT *);
	HRESULT(STDMETHODCALLTYPE *GetProperty)(FluxIGraphicsEffectD2D1Interop *, UINT, void **);
	HRESULT(STDMETHODCALLTYPE *GetSource)(FluxIGraphicsEffectD2D1Interop *, UINT, void **);
	HRESULT(STDMETHODCALLTYPE *GetSourceCount)(FluxIGraphicsEffectD2D1Interop *, UINT *);
} FluxIGraphicsEffectD2D1InteropVtbl;

struct FluxIGraphicsEffectD2D1Interop {
	FluxIGraphicsEffectD2D1InteropVtbl const *lpVtbl;
};

static const GUID FluxIID_IGraphicsEffectD2D1Interop = {
  0x2fc57384, 0xa068, 0x44d7, {0xa3, 0x31, 0x30, 0x98, 0x2f, 0xcf, 0x71, 0x77}
};

/* ---- IVisualInteractionSourceInterop {11F62CD1-2F9D-42D3-B05F-D6790D9E9F8E} ----
 * Used in Phase D for off-thread scrolling: redirects pointer input to the tracker. */
typedef struct FluxIVisualInteractionSourceInterop FluxIVisualInteractionSourceInterop;

typedef struct FluxIVisualInteractionSourceInteropVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(FluxIVisualInteractionSourceInterop *, REFIID, void **);
	ULONG(STDMETHODCALLTYPE *AddRef)(FluxIVisualInteractionSourceInterop *);
	ULONG(STDMETHODCALLTYPE *Release)(FluxIVisualInteractionSourceInterop *);
	HRESULT(STDMETHODCALLTYPE *TryRedirectForManipulation)(FluxIVisualInteractionSourceInterop *, const POINTER_INFO *);
} FluxIVisualInteractionSourceInteropVtbl;

struct FluxIVisualInteractionSourceInterop {
	FluxIVisualInteractionSourceInteropVtbl const *lpVtbl;
};

static const GUID FluxIID_IVisualInteractionSourceInterop = {
  0x11f62cd1, 0x2f9d, 0x42d3, {0xb0, 0x5f, 0xd6, 0x79, 0x0d, 0x9e, 0x9f, 0x8e}
};

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_INTEROP_H */
