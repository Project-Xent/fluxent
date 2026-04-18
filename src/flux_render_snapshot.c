#include "fluxent/flux_render_snapshot.h"

#include <string.h>

void flux_snapshot_build(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node, FluxNodeData const *nd) {
	memset(snap, 0, sizeof(*snap));
	snap->id      = ( uint64_t ) node;
	snap->type    = xent_get_control_type(ctx, node);
	snap->enabled = xent_get_semantic_enabled(ctx, node);

	if (!nd) return;

	snap->background    = nd->visuals.background;
	snap->border_color  = nd->visuals.border_color;
	snap->corner_radius = nd->visuals.corner_radius;
	snap->border_width  = nd->visuals.border_width;
	snap->opacity       = nd->visuals.opacity;

	/* Sub-element hover tracking */
	snap->hover_local_x = nd->hover_local_x;

	if (!nd->component_data) return;

		switch (snap->type) {
			case XENT_CONTROL_TEXT : {
				FluxTextData const *t     = ( FluxTextData const * ) nd->component_data;
				snap->text_content        = t->content;
				snap->font_family         = t->font_family;
				snap->text_color          = t->text_color;
				snap->font_size           = t->font_size;
				snap->font_weight         = t->font_weight;
				snap->text_alignment      = t->alignment;
				snap->text_vert_alignment = t->vertical_alignment;
				snap->max_lines           = t->max_lines;
				snap->word_wrap           = t->wrap;
				break;
			}
		case XENT_CONTROL_BUTTON :
			case XENT_CONTROL_TOGGLE_BUTTON : {
				FluxButtonData const *b = ( FluxButtonData const * ) nd->component_data;
				snap->label             = b->label;
				snap->icon_name         = b->icon_name;
				snap->text_color        = b->label_color;
				snap->font_size         = b->font_size;
				snap->button_style      = b->style;
				snap->is_checked        = b->is_checked;
				snap->enabled           = b->enabled;
				break;
			}
		case XENT_CONTROL_CHECKBOX :
		case XENT_CONTROL_RADIO :
			case XENT_CONTROL_SWITCH : {
				FluxCheckboxData const *c = ( FluxCheckboxData const * ) nd->component_data;
				snap->label               = c->label;
				snap->check_state         = c->state;
				snap->enabled             = c->enabled;
				break;
			}
			case XENT_CONTROL_SLIDER : {
				FluxSliderData const *s = ( FluxSliderData const * ) nd->component_data;
				snap->min_value         = s->min_value;
				snap->max_value         = s->max_value;
				snap->current_value     = s->current_value;
				snap->step              = s->step;
				snap->enabled           = s->enabled;
				break;
			}
			case XENT_CONTROL_TEXT_INPUT : {
				FluxTextBoxData const *tb = ( FluxTextBoxData const * ) nd->component_data;
				snap->text_content        = tb->content;
				snap->placeholder         = tb->placeholder;
				snap->font_family         = tb->font_family;
				snap->font_size           = tb->font_size;
				snap->text_color          = tb->text_color;
				snap->cursor_position     = tb->cursor_position;
				snap->selection_start     = tb->selection_start;
				snap->selection_end       = tb->selection_end;
				snap->scroll_offset_x     = tb->scroll_offset_x;
				snap->enabled             = tb->enabled;
				snap->composition_text    = tb->composition_text;
				snap->composition_length  = tb->composition_length;
				snap->composition_cursor  = tb->composition_cursor;
				snap->selection_color     = tb->selection_color;
				snap->readonly            = tb->readonly;
				break;
			}
			case XENT_CONTROL_SCROLL : {
				FluxScrollData const *sc        = ( FluxScrollData const * ) nd->component_data;
				snap->scroll_x                  = sc->scroll_x;
				snap->scroll_y                  = sc->scroll_y;
				snap->scroll_content_w          = sc->content_w;
				snap->scroll_content_h          = sc->content_h;
				snap->scroll_h_vis              = sc->h_vis;
				snap->scroll_v_vis              = sc->v_vis;
				snap->scroll_mouse_over         = sc->mouse_over;
				snap->scroll_mouse_local_x      = sc->mouse_local_x;
				snap->scroll_mouse_local_y      = sc->mouse_local_y;
				snap->scroll_drag_axis          = sc->drag_axis;
				snap->scroll_v_up_pressed       = sc->v_up_pressed;
				snap->scroll_v_dn_pressed       = sc->v_dn_pressed;
				snap->scroll_h_lf_pressed       = sc->h_lf_pressed;
				snap->scroll_h_rg_pressed       = sc->h_rg_pressed;
				snap->scroll_last_activity_time = sc->last_activity_time;
				break;
			}
			case XENT_CONTROL_PROGRESS : {
				FluxProgressData const *p = ( FluxProgressData const * ) nd->component_data;
				snap->current_value       = p->value;
				snap->max_value           = p->max_value;
				snap->indeterminate       = p->indeterminate;
				break;
			}
			case XENT_CONTROL_PASSWORD_BOX : {
				FluxTextBoxData const *tb = ( FluxTextBoxData const * ) nd->component_data;
				snap->text_content        = tb->content;
				snap->placeholder         = tb->placeholder;
				snap->font_family         = tb->font_family;
				snap->font_size           = tb->font_size;
				snap->text_color          = tb->text_color;
				snap->cursor_position     = tb->cursor_position;
				snap->selection_start     = tb->selection_start;
				snap->selection_end       = tb->selection_end;
				snap->scroll_offset_x     = tb->scroll_offset_x;
				snap->enabled             = tb->enabled;
				snap->composition_text    = tb->composition_text;
				snap->composition_length  = tb->composition_length;
				snap->composition_cursor  = tb->composition_cursor;
				snap->selection_color     = tb->selection_color;
				snap->readonly            = tb->readonly;
				/* Reveal state: semantic_checked == 1 means show plain text */
				snap->is_checked          = xent_get_semantic_checked(ctx, node) != 0;
				break;
			}
			case XENT_CONTROL_NUMBER_BOX : {
				FluxTextBoxData const *tb = ( FluxTextBoxData const * ) nd->component_data;
				snap->text_content        = tb->content;
				snap->placeholder         = tb->placeholder;
				snap->font_family         = tb->font_family;
				snap->font_size           = tb->font_size;
				snap->text_color          = tb->text_color;
				snap->cursor_position     = tb->cursor_position;
				snap->selection_start     = tb->selection_start;
				snap->selection_end       = tb->selection_end;
				snap->scroll_offset_x     = tb->scroll_offset_x;
				snap->enabled             = tb->enabled;
				snap->composition_text    = tb->composition_text;
				snap->composition_length  = tb->composition_length;
				snap->composition_cursor  = tb->composition_cursor;
				snap->selection_color     = tb->selection_color;
				snap->readonly            = tb->readonly;
				/* NumberBox spin state from semantic properties */
				snap->nb_spin_placement   = ( uint8_t ) (xent_get_semantic_expanded(ctx, node) ? 2 : 0);
				{
					float sv = 0, smin = 0, smax = 0;
						if (xent_get_semantic_value(ctx, node, &sv, &smin, &smax)) {
							snap->nb_up_enabled   = (sv < smax) || (smin == smax);
							snap->nb_down_enabled = (sv > smin) || (smin == smax);
						}
						else {
							snap->nb_up_enabled   = true;
							snap->nb_down_enabled = true;
						}
				}
				break;
			}
			case XENT_CONTROL_HYPERLINK : {
				FluxHyperlinkData const *hl = ( FluxHyperlinkData const * ) nd->component_data;
				snap->label                 = hl->label;
				snap->icon_name             = hl->icon_name;
				snap->text_color            = hl->label_color;
				snap->font_size             = hl->font_size;
				snap->button_style          = FLUX_BUTTON_TEXT;
				snap->enabled               = hl->enabled;

				break;
			}
			case XENT_CONTROL_REPEAT_BUTTON : {
				FluxRepeatButtonData const *rb = ( FluxRepeatButtonData const * ) nd->component_data;
				snap->label                    = rb->label;
				snap->icon_name                = rb->icon_name;
				snap->text_color               = rb->label_color;
				snap->font_size                = rb->font_size;
				snap->button_style             = rb->style;
				snap->enabled                  = rb->enabled;
				break;
			}
			case XENT_CONTROL_PROGRESS_RING : {
				FluxProgressRingData const *pr = ( FluxProgressRingData const * ) nd->component_data;
				snap->current_value            = pr->value;
				snap->max_value                = pr->max_value;
				snap->indeterminate            = pr->indeterminate;
				break;
			}
			case XENT_CONTROL_INFO_BADGE : {
				FluxInfoBadgeData const *ib = ( FluxInfoBadgeData const * ) nd->component_data;
				snap->current_value         = ( float ) ib->value;
				snap->icon_name             = ib->icon_name;
				snap->background            = ib->background;
				snap->is_checked            = (ib->mode == FLUX_BADGE_DOT);
				snap->indeterminate         = (ib->mode == FLUX_BADGE_ICON);
				break;
			}
		default : break;
		}
}
