/* SHIELD LOOK: optional render-only profiles; CLASSIC preserves map-authored shields. */
#ifndef SHIELD_LOOK_H
#define SHIELD_LOOK_H

struct shader;
struct model;
struct render_model_effect;
union real_rgb_color;
struct real_matrix4x3;

boolean shield_look_colors(struct shader const *shader, union real_rgb_color const *colors,
	union real_rgb_color *perpendicular, union real_rgb_color *parallel);
void shield_arcs_unload(void);
void shield_arcs_draw(long object_index, long model_index, struct real_matrix4x3 const *nodes,
	struct render_model_effect const *effect, boolean arms);
void shield_look_reset(void);
boolean shield_look_skip_shell(struct model const *model, struct shader const *surface);
struct shader *shield_look_body(long object_index, struct shader *original);
boolean shield_look_arms(short local_player_index, long unit_index, long hands_model_index,
	struct render_model_effect *effect);

#endif
