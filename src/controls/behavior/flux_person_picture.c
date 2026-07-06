/**
 * @file flux_person_picture.c
 * @brief PersonPicture behavior: a non-interactive circular avatar leaf.
 *
 * Owns its strings and a resolved-initials buffer. Initials come from an
 * explicit override or are derived from the display name (first + last word,
 * ASCII-uppercased, first Unicode codepoint of each). Badge glyphs are resolved
 * from a Segoe Fluent Icons name to UTF-8 at set time so the renderer stays
 * pure data.
 */
#include "controls/factory/flux_factory.h"
#include "render/flux_icon.h"
#include "runtime/flux_str.h"
#include "fluxent/fluxent.h"

#include <stdlib.h>
#include <string.h>

static FluxPersonPictureData *pp_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_PERSON_PICTURE || !nd->component_data) return NULL;
	return ( FluxPersonPictureData * ) nd->component_data;
}

/* Byte length of the first UTF-8 codepoint at s (defensive; assumes valid UTF-8). */
static int pp_cp_len(char const *s) {
	unsigned char c = ( unsigned char ) s [0];
	if (c < 0x80) return 1;
	if ((c & 0xE0) == 0xC0) return 2;
	if ((c & 0xF0) == 0xE0) return 3;
	if ((c & 0xF8) == 0xF0) return 4;
	return 1;
}

static char const *pp_skip_spaces(char const *s) {
	while (*s == ' ' || *s == '\t') s++;
	return s;
}

/* Append the first codepoint of the word at @p word, ASCII-uppercased, into
 * out[*len..cap). */
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

/* Resolve the initials actually drawn: explicit override wins; otherwise take
 * the first codepoint of the first and last space-separated words. */
static void pp_resolve_initials(FluxPersonPictureData *d) {
	d->resolved [0] = '\0';
	if (d->initials && d->initials [0]) {
		size_t n = strlen(d->initials);
		if (n >= sizeof(d->resolved)) n = sizeof(d->resolved) - 1;
		memcpy(d->resolved, d->initials, n);
		d->resolved [n] = '\0';
		return;
	}
	if (!d->display_name || !d->display_name [0]) return;

	char const *first = pp_skip_spaces(d->display_name);
	if (!first [0]) return;

	/* Locate the last word: scan to the final non-space run. */
	char const *last  = first;
	char const *p     = first;
	while (*p) {
		p = pp_skip_spaces(p);
		if (!*p) break;
		last = p;
		while (*p && *p != ' ' && *p != '\t') p++;
	}

	int len = 0;
	pp_append_initial(d->resolved, &len, ( int ) sizeof(d->resolved), first);
	if (last != first) pp_append_initial(d->resolved, &len, ( int ) sizeof(d->resolved), last);
	d->resolved [len] = '\0';
}

/* Resolve a badge icon name to an owned UTF-8 glyph string (NULL clears). */
static void pp_set_badge_glyph(FluxPersonPictureData *d, char const *name) {
	flux_str_free(d->badge_glyph);
	d->badge_glyph = NULL;
	if (!name || !name [0]) return;
	wchar_t const *cp = flux_icon_lookup(name);
	char           utf8 [8];
	if (cp && flux_icon_to_utf8(cp, utf8, sizeof(utf8)) > 0) d->badge_glyph = flux_str_dup(utf8);
	else d->badge_glyph = flux_str_dup(name); /* allow a literal glyph string */
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

XentNodeId flux_create_person_picture(FluxPersonPictureCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_PERSON_PICTURE);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData          *nd = flux_node_store_get(info->store, node);
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

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_IMAGE);
	xent_set_size(info->ctx, node, (XentSize) {d->diameter, d->diameter});
	return node;
}

void flux_person_picture_set_display_name(FluxNodeStore *store, XentNodeId pp, char const *display_name) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (!d) return;
	flux_str_free(d->display_name);
	d->display_name = display_name ? flux_str_dup(display_name) : NULL;
	pp_resolve_initials(d);
}

void flux_person_picture_set_initials(FluxNodeStore *store, XentNodeId pp, char const *initials) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (!d) return;
	flux_str_free(d->initials);
	d->initials = (initials && initials [0]) ? flux_str_dup(initials) : NULL;
	pp_resolve_initials(d);
}

void flux_person_picture_set_image(FluxNodeStore *store, XentNodeId pp, char const *image_path) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (!d) return;
	flux_str_free(d->image_path);
	d->image_path = (image_path && image_path [0]) ? flux_str_dup(image_path) : NULL;
}

void flux_person_picture_set_badge(FluxNodeStore *store, XentNodeId pp, char const *badge_glyph, int badge_number) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (!d) return;
	pp_set_badge_glyph(d, badge_glyph);
	d->badge_number = badge_number;
}

void flux_person_picture_set_group(FluxNodeStore *store, XentNodeId pp, bool is_group) {
	FluxPersonPictureData *d = pp_data(store, pp);
	if (d) d->is_group = is_group;
}
