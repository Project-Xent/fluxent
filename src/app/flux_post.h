#ifndef FLUX_APP_POST_H
#define FLUX_APP_POST_H

#include <xtk/xtk_reconcile.h>

#include <fluxent/flux_app.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxPostSink FluxPostSink;

FluxPostSink               *flux_postsink_create(void);
void                        flux_postsink_destroy(FluxPostSink *sink);
bool                        flux_postsink_invoke(FluxPostSink *sink, void (*fn)(void *ctx), void *ctx);
XtkPostStatus flux_postsink_send(FluxPostSink *sink, XtkRt *rt, XtkMsg msg, char const *data, uint32_t len, bool text);

bool          flux_app_bindruntime(FluxApp *app, XtkRt *rt);

#endif
