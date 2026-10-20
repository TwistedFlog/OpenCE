/* SHIELD LOOK: optional render-only profiles; map-authored shields win. */
#ifndef SHIELD_LOOK_H
#define SHIELD_LOOK_H

struct shader;
struct render_model_effect;

void shield_look_reset(void);
struct shader *shield_look_body(long object_index, struct shader *original);
boolean shield_look_arms(short local_player_index, long unit_index, long hands_model_index,
	struct render_model_effect *effect);

#endif
