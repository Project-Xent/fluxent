#ifndef FLUX_DATA_INTERNAL_H
#define FLUX_DATA_INTERNAL_H

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "fluxent/flux_data.h"

#include <ole2.h>

/* Takes over one reference to obj. */
FluxData    *flux_data_adopt(IDataObject *obj);
IDataObject *flux_data_object(FluxData const *data);
/* put takes ownership of h either way; take yields an HGLOBAL medium the caller releases. */
bool         flux_data_put(FluxData *data, CLIPFORMAT cf, HGLOBAL h);
bool         flux_data_take(FluxData const *data, CLIPFORMAT cf, STGMEDIUM *sm);

#endif
