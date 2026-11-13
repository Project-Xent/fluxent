#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "render/flux_icon.h"
#include "runtime/flux_str.h"
#include "controls/factory/flux_init_media.h"

#include <stdlib.h>
#include <string.h>

static FluxPersonPictureData *pp_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_PERSON_PICTURE || !nd->component_data) return NULL;
	return ( FluxPersonPictureData * ) nd->component_data;
}

static int pp_cp_len(char const *s) {
	unsigned char c = ( unsigned char ) s [0];
	if (c < 0x80) return 1;
	if ((c & 0xe0) == 0xc0) return 2;
	if ((c & 0xf0) == 0xe0) return 3;
	if ((c & 0xf8) == 0xf0) return 4;
	return 1;
}

static char const *pp_skip_spaces(char const *s) {
	while (*s == ' ' || *s == '\t') s++;
	return s;
}

static void pp_append_initial(char *out, int *len, int cap, char const *word) {
	if (!word || !word [0]) return;
	int n = pp_cp_len(word);
	if (*len + n >= cap) return;
	for (int i = 0; i < n; i++) {
		char ch = word [i];
		if (n == 1 && ch >= 'a' && ch <= 'z') ch = ( char ) (ch - 'a' + 'A');
		out [(*len)++] = ch;
	}
}

static void pp_copy_initials(FluxPersonPictureData *d) {
	size_t n = strlen(d->initials);
	if (n >= sizeof(d->resolved)) n = sizeof(d->resolved) - 1;
	memcpy(d->resolved, d->initials, n);
	d->resolved [n] = '\0';
}

static char const *pp_last_word(char const *first) {
	char const *last = first;
	char const *p    = first;
	while (*p) {
		p = pp_skip_spaces(p);
		if (!*p) break;
		last = p;
		while (*p && *p != ' ' && *p != '\t') p++;
	}
	return last;
}

static void pp_resolve_initials(FluxPersonPictureData *d) {
	d->resolved [0] = '\0';
	if (d->initials && d->initials [0]) {
		pp_copy_initials(d);
		return;
	}
	if (!d->display_name || !d->display_name [0]) return;

	char const *first = pp_skip_spaces(d->display_name);
	if (!first [0]) return;

	int len = 0;
	pp_append_initial(d->resolved, &len, ( int ) sizeof(d->resolved), first);
	char const *last = pp_last_word(first);
	if (last != first) pp_append_initial(d->resolved, &len, ( int ) sizeof(d->resolved), last);
	d->resolved [len] = '\0';
}

static void pp_set_badge_glyph(FluxPersonPictureData *d, char const *name) {
	flux_str_free(d->badge_glyph);
	d->badge_glyph = NULL;
	if (!name || !name [0]) return;
	wchar_t const *cp = flux_icon_lookup(name);
	char           utf8 [8];
	if (cp && flux_icon_to_utf8(cp, utf8, sizeof(utf8)) > 0) d->badge_glyph = flux_str_dup(utf8);
	else d->badge_glyph = flux_str_dup(name);
}

static void pp_destroy(void *component_data) {
	FluxPersonPictureData *d = ( FluxPersonPictureData * ) component_data;
	if (!d) return;
	flux_str_free(d->display_name);
	flux_str_free(d->initials);
	flux_str_free(d->image_path);
	flux_str_free(d->badge_glyph);
	free(d);
}

XentNodeId flux_avatar_init(FluxPersonPictureDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_PERSON_PICTURE);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData          *nd = flux_store_find(info->store, node);
	FluxPersonPictureData *d  = nd ? ( FluxPersonPictureData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	d->ctx          = info->ctx;
	d->store        = info->store;
	d->node         = node;
	d->diameter     = info->diameter > 0.0f ? info->diameter : FLUX_PP_DEFAULT_SIZE;
	d->display_name = info->display_name ? flux_str_dup(info->display_name) : NULL;
	d->initials     = info->initials ? flux_str_dup(info->initials) : NULL;
	d->image_path   = info->image_path ? flux_str_dup(info->image_path) : NULL;
	d->badge_number = info->badge_number;
	d->is_group     = info->is_group;
	pp_set_badge_glyph(d, info->badge_glyph);
	pp_resolve_initials(d);

	nd->component_data         = d;
	nd->destroy_component_data = pp_destroy;

	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_IMAGE);
	xent_setsize(info->ctx, node, (XentSize) {d->diameter, d->diameter});
	return node;
}

void flux_avatar_setname(FluxStore *store, XentNodeId pp, char const *display_name) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (!d) return;
	flux_str_free(d->display_name);
	d->display_name = display_name ? flux_str_dup(display_name) : NULL;
	pp_resolve_initials(d);
}

void flux_avatar_set_initials(FluxStore *store, XentNodeId pp, char const *initials) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (!d) return;
	flux_str_free(d->initials);
	d->initials = (initials && initials [0]) ? flux_str_dup(initials) : NULL;
	pp_resolve_initials(d);
}

void flux_avatar_set_image(FluxStore *store, XentNodeId pp, char const *image_path) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (!d) return;
	flux_str_free(d->image_path);
	d->image_path = (image_path && image_path [0]) ? flux_str_dup(image_path) : NULL;
}

void flux_avatar_set_badge(FluxStore *store, XentNodeId pp, char const *badge_glyph, int badge_number) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (!d) return;
	pp_set_badge_glyph(d, badge_glyph);
	d->badge_number = badge_number;
}

void flux_avatar_set_group(FluxStore *store, XentNodeId pp, bool is_group) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (d) d->is_group = is_group;
}
