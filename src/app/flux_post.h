#ifndef FLUX_APP_POST_H
#define FLUX_APP_POST_H

#include <xtk/xtk_reconcile.h>

#include <fluxent/flux_app.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxPostSink FluxPostSink;

FluxPostSink               *flux_postsink_create(XtkRt *rt);
void                        flux_postsink_destroy(FluxPostSink *sink);
XtkPostStatus flux_postsink_send(FluxPostSink *sink, XtkMsg msg, char const *data, uint32_t len, bool text);

bool          flux_app_bindruntime(FluxApp *app, XtkRt *rt);

#endif
