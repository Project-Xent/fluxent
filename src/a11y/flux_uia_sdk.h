#ifndef FLUX_UIA_SDK_H
#define FLUX_UIA_SDK_H

#ifndef COBJMACROS
  #define COBJMACROS
#endif
#ifndef CINTERFACE
  #define CINTERFACE
#endif

#if defined(_MSC_VER) && !defined(__UIA_OtherConstants_MODULE_DEFINED__)
  // The Windows SDK gives UIA_ScrollPatternNoScroll external linkage in C.
  #define __UIA_OtherConstants_MODULE_DEFINED__
#endif

#include <windows.h>
#include <oleauto.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>

#endif
