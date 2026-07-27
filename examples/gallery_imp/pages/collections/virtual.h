#ifndef GALLERY_IMP_COLLECTIONS_VIRTUAL_H
#define GALLERY_IMP_COLLECTIONS_VIRTUAL_H

#include "app.h"

typedef struct ListDemo {
	GalleryApp *g;
	XentNodeId  list;
	int         count;
	float       item_h;
	float       item_w;
	float       viewport_h;
	int         cols;
	int         selected;
	XtkListKind kind;
	void        (*fill)(GalleryApp *g, XentNodeId item, int index);
} ListDemo;

void       gallery_list_select(void *ud, int idx);
void       gallery_list_invoke(void *ud, int idx);
void       gallery_list_clear(GalleryApp *g, XentNodeId host);
void       gallery_list_realize(ListDemo *demo);
XentNodeId gallery_list_create(
  GalleryApp *g, XentNodeId parent, XtkListKind kind, int count, float item_h, float item_w, int cols, int selected,
  XtkListSelMode mode, int tag, void (*fill)(GalleryApp *, XentNodeId, int), float height
);

#endif
