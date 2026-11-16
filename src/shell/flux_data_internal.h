#ifndef FLUX_DATA_INTERNAL_H
#define FLUX_DATA_INTERNAL_H

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "fluxent/flux_data.h"

#include <ole2.h>

FluxData    *flux_data_adopt(IDataObject *obj);
IDataObject *flux_data_object(FluxData const *data);
bool         flux_data_put(FluxData *data, CLIPFORMAT cf, HGLOBAL h);
bool         flux_data_take(FluxData const *data, CLIPFORMAT cf, STGMEDIUM *sm);

#endif
