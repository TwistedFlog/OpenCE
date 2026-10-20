/*
FIRST_PERSON_LEGS.C

Your own legs in first person (Video Setup > FOV and Viewmodels > Legs,
display.first_person_legs; off, the stock game's: the body not drawn in its
own player's view). The body the game animates for everyone else is drawn
for its own player too, from the waist down: looking down while standing,
running or crouching, the legs move as the body's animation moves them.

Only the drawing changes, and only in that player's first-person view: its
shadow, its reflection, its hit boxes and markers are the game's as ever,
and nothing it carries or wears (its weapons, its children, its widgets) is
drawn with it.

The legs are found from the skeleton, once per biped definition: the pelvis
and the head are the biped's own runtime nodes (the ones its autoaim pill
uses). The pelvis's child on the way up to the head is the spine; the nodes
below the pelvis but not below the spine are the legs. Everything else past
the pelvis's own ancestors is drawn shrunk to nothing
(FIRST_PERSON_LEGS_COLLAPSE) onto one point of the waist, part of the way
from the pelvis's joint to the spine's (FIRST_PERSON_LEGS_LID), so the
upper body's skin closes the waist as a low lid: the vertices blended
between the hips and the spine are pulled in towards that point (onto the
spine's joint, above the waist, they stood up as a dome). The model's
geometry is never read.

The pose is each local player's own (split-screen views draw their own
bodies). It is turned about the hips from the body's facing to the view's:
the game turns a standing body only once the aim is well away from it, and
slowly, and one in the air not at all, so the legs drawn as they face would
lag behind the view and, after a jump's turn, stand in front of it. It is
set back a fixed distance behind the eye along the view's level facing, so
the body stands still on the ground as the view turns up and down: looking
down, it is a little behind, as though the head leaned out over it.
*/

#include "cseries.h"
#include "real_math.h"
#include "camera/director.h"
#include "game/players.h"
#include "models/model_definitions.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"
#include "render/render.h"
#include "render/render_cameras.h"
#include "tag_files/tag_groups.h"
#include "units/biped_definitions.h"

#include "first_person_legs.h"

#include <math.h>

enum
{
	FIRST_PERSON_LEGS_MODELS = 8,
};

/* how far behind the eye the body is drawn, world units (about 49 cm), along
the view's level facing (tuned in-game) */
#define FIRST_PERSON_LEGS_SET_BACK 0.16f
/* where on the way from the pelvis's joint (0) to the spine's (1) the upper
body is drawn, at a field of view of FIRST_PERSON_LEGS_LID_FOV degrees
across at 16:9 (display.fov's measure), and higher in proportion to a wider
view's: lower, the waist sinks into a funnel; higher, it stands up as a
dome (tuned by eye) */
#define FIRST_PERSON_LEGS_LID 0.5f
#define FIRST_PERSON_LEGS_LID_FOV 90.f
/* the scale the upper body is drawn at on the waist: small enough that its
skin is within a millimetre of the lid's point, not zero, which would
leave its vertices' normals without a length to normalize */
#define FIRST_PERSON_LEGS_COLLAPSE 0.001f

/* port/linux/src/port_config.c */
int config_boolean(const char *name);
unsigned long config_changes(void);
/* port/linux/src (halo.log) */
void platform_log(const char *format, ...);

/* ---------- globals */

/* a biped definition's legs, as its skeleton has them */
struct first_person_legs_model
{
	long definition_index; /* NONE: free */
	/* the pelvis's child towards the head; NONE: no legs to draw */
	short spine_node_index;
	/* TRUE: a node of the upper body, drawn onto the spine's joint */
	boolean collapsed[MAXIMUM_NODES_PER_MODEL];
};

static struct first_person_legs_model first_person_legs_models[FIRST_PERSON_LEGS_MODELS];
static short first_person_legs_next_model = 0;
static boolean first_person_legs_models_valid = FALSE;

/* each local player's pose of its legs, rewritten as its view is drawn */
static real_matrix4x3 first_person_legs_matrices[MAXIMUM_LOCAL_PLAYERS][MAXIMUM_NODES_PER_MODEL];

/* ---------- private code */

static boolean first_person_legs_enabled(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled = FALSE;

	if (changes != config_changes())
	{
		changes = config_changes();
		enabled = config_boolean("display.first_person_legs") != 0;
	}
	return enabled;
}

/* whether ancestor is node or above it (a map's parents are followed no
further than there are nodes, so a loop ends) */
static boolean first_person_legs_below(
	struct model const *model,
	short node_count,
	short node_index,
	short ancestor_index)
{
	short steps;

	for (steps = 0; steps <= node_count && node_index >= 0 && node_index < node_count; steps++)
	{
		if (node_index == ancestor_index)
			return TRUE;
		node_index = TAG_BLOCK_GET_ELEMENT(&model->nodes, node_index, struct model_node)->parent_node_index;
	}
	return FALSE;
}

/* a skeleton's legs, from its pelvis and head: the spine node, or NONE (and
why not, for the log) */
static short first_person_legs_find(
	struct model const *model,
	short node_count,
	short pelvis_index,
	short head_index,
	boolean *collapsed,
	char const **reason)
{
	short spine_index = NONE;
	short leg_count = 0;
	short node_index;
	short steps;

	if (pelvis_index < 0 || pelvis_index >= node_count || head_index < 0 || head_index >= node_count ||
		head_index == pelvis_index)
	{
		*reason = "no pelvis and head nodes";
		return NONE;
	}
	/* (the spine: the head's ancestor whose parent is the pelvis) */
	node_index = head_index;
	for (steps = 0; steps <= node_count && node_index >= 0 && node_index < node_count; steps++)
	{
		short parent_index = TAG_BLOCK_GET_ELEMENT(&model->nodes, node_index, struct model_node)->parent_node_index;

		if (parent_index == pelvis_index)
		{
			spine_index = node_index;
			break;
		}
		node_index = parent_index;
	}
	if (spine_index == NONE)
	{
		*reason = "its head is not below its pelvis";
		return NONE;
	}
	for (node_index = 0; node_index < node_count; node_index++)
	{
		/* (the pelvis and the frames above it are the body's place, kept) */
		if (first_person_legs_below(model, node_count, pelvis_index, node_index))
			collapsed[node_index] = FALSE;
		else if (first_person_legs_below(model, node_count, node_index, pelvis_index) &&
			!first_person_legs_below(model, node_count, node_index, spine_index))
		{
			collapsed[node_index] = FALSE;
			leg_count++;
		}
		else
			collapsed[node_index] = TRUE;
	}
	if (leg_count == 0)
	{
		*reason = "no nodes below its pelvis but its spine";
		return NONE;
	}
	return spine_index;
}

/* a biped definition's legs, found once (and, when it has none, said once) */
static struct first_person_legs_model const *first_person_legs_model_get(
	long definition_index,
	struct model const *model,
	short node_count,
	short pelvis_index,
	short head_index)
{
	struct first_person_legs_model *entry;
	char const *reason = "";
	short index;

	if (!first_person_legs_models_valid)
		first_person_legs_reset();
	for (index = 0; index < FIRST_PERSON_LEGS_MODELS; index++)
	{
		if (first_person_legs_models[index].definition_index == definition_index)
			return &first_person_legs_models[index];
	}
	entry = &first_person_legs_models[first_person_legs_next_model];
	first_person_legs_next_model = (short)((first_person_legs_next_model + 1) % FIRST_PERSON_LEGS_MODELS);
	entry->definition_index = definition_index;
	entry->spine_node_index = first_person_legs_find(model, node_count, pelvis_index, head_index,
		entry->collapsed, &reason);
	if (entry->spine_node_index == NONE)
		platform_log("first-person legs: %s has %s; its legs are not drawn\n", tag_get_name(definition_index), reason);
	return entry;
}

/* the way the view looks, level: its forward's level part, with its up's
(which points the way the forward did) so that looking straight down or up
it still has one; FALSE for none */
static boolean first_person_legs_view_heading(
	real_vector3d const *forward,
	real_vector3d const *up,
	real_vector3d *heading)
{
	heading->i = forward->i - forward->k * up->i;
	heading->j = forward->j - forward->k * up->j;
	heading->k = 0.f;
	return normalize3d(heading) > 0.f;
}

/* the way the body faces, level, in the pose drawn: the object's facing (as
the last tick turned it), taken into its first node's frame as that tick
left it, and out of that node's frame as drawn (between ticks), so the body
turns as smoothly as its pose; FALSE for none */
static boolean first_person_legs_body_heading(
	real_vector3d const *facing,
	real_matrix4x3 const *tick_root,
	real_matrix4x3 const *drawn_root,
	real_vector3d *heading)
{
	real forward = dot_product3d(facing, &tick_root->forward);
	real left = dot_product3d(facing, &tick_root->left);
	real up = dot_product3d(facing, &tick_root->up);

	heading->i = forward * drawn_root->forward.i + left * drawn_root->left.i + up * drawn_root->up.i;
	heading->j = forward * drawn_root->forward.j + left * drawn_root->left.j + up * drawn_root->up.j;
	heading->k = 0.f;
	return normalize3d(heading) > 0.f;
}

/* where between the pelvis's joint and the spine's the upper body is drawn,
for a view drawn at this vertical angle (radians) */
static real first_person_legs_lid(
	real vertical_field_of_view)
{
	/* (the view's angle across at 16:9, in degrees, as display.fov has it) */
	real degrees = (real)(360.0 / _pi * atan(tan(0.5 * (double)vertical_field_of_view) * (16.0 / 9.0)));
	real lid = FIRST_PERSON_LEGS_LID * degrees / FIRST_PERSON_LEGS_LID_FOV;

	return PIN(lid, 0.f, 1.f);
}

/* a vector turned about the vertical by the angle whose cosine and sine
these are */
static void first_person_legs_turn(
	real_vector3d *vector,
	real cosine,
	real sine)
{
	real i = vector->i;

	vector->i = cosine * i - sine * vector->j;
	vector->j = sine * i + cosine * vector->j;
}

/* ---------- public code */

void first_person_legs_reset(
	void)
{
	short index;

	for (index = 0; index < FIRST_PERSON_LEGS_MODELS; index++)
		first_person_legs_models[index].definition_index = NONE;
	first_person_legs_next_model = 0;
	first_person_legs_models_valid = TRUE;
}

real_matrix4x3 const *first_person_legs_node_matrices(
	short local_player_index,
	long object_index)
{
	struct object_datum *object;
	struct biped_definition *definition;
	struct model *model;
	struct first_person_legs_model const *legs;
	real_matrix4x3 const *nodes;
	real_matrix4x3 *pose;
	real_vector3d view;
	real_vector3d body;
	real_vector3d back;
	real_point3d lid;
	real lid_fraction;
	real_point3d pivot;
	real cosine = 1.f;
	real sine = 0.f;
	long player_index;
	short node_count;
	short index;

	if (!first_person_legs_enabled() || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS ||
		director_get_perspective(local_player_index) != _director_perspective_first_person)
	{
		return NULL;
	}
	/* (the player's own living, visible biped, on foot) */
	player_index = local_player_get_player_index(local_player_index);
	object = object_try_and_get(object_index);
	if (player_index == NONE || player_get(player_index)->unit_index != object_index || !object ||
		object->object.type != _object_type_biped || object->object.parent_object_index != NONE ||
		TEST_FLAG(object->object.damage_flags, _object_dead_bit) ||
		TEST_FLAG(object->object.flags, _object_invisible_bit))
	{
		return NULL;
	}
	definition = biped_definition_get(object->definition_index);
	if (!definition || definition->object.model.index == NONE)
		return NULL;
	model = model_definition_get(definition->object.model.index);
	node_count = (short)MIN(model->nodes.count, MAXIMUM_NODES_PER_MODEL);
	/* (the object's own matrices, as many as the model's nodes) */
	if (node_count <= 0 || object->object.node_matrices.size < node_count * (long)sizeof(real_matrix4x3))
		return NULL;
	legs = first_person_legs_model_get(object->definition_index, model, node_count,
		definition->biped.runtime_pelvis_node_index, definition->biped.runtime_head_node_index);
	if (legs->spine_node_index == NONE)
		return NULL;

	nodes = object_get_node_matrices(object_index);
	pose = first_person_legs_matrices[local_player_index];
	/* (turned from the body's facing, the object's, to the view's; not
	from its first node's, which in the game's characters is the pelvis,
	its axis up the spine) */
	if (!first_person_legs_view_heading(&render.camera.forward, &render.camera.up, &view))
		view = *global_zero_vector3d;
	else if (first_person_legs_body_heading(&object->object.forward,
		(real_matrix4x3 const *)object_header_block_get(object_index, &object->object.node_matrices),
		&nodes[0], &body))
	{
		cosine = dot_product3d(&body, &view);
		sine = body.i * view.j - body.j * view.i;
	}
	back.i = -view.i;
	back.j = -view.j;
	back.k = 0.f;
	/* A model may have a separate root above the pelvis. Turn about the
	drawn hips so that an offset root does not move them as the view turns. */
	pivot = nodes[definition->biped.runtime_pelvis_node_index].position;
	{
		real_point3d const *spine = &nodes[legs->spine_node_index].position;
		real_point3d const *pelvis = &nodes[definition->biped.runtime_pelvis_node_index].position;

		lid_fraction = first_person_legs_lid(render.camera.vertical_field_of_view);
		lid.x = pelvis->x + (spine->x - pelvis->x) * lid_fraction;
		lid.y = pelvis->y + (spine->y - pelvis->y) * lid_fraction;
		lid.z = pelvis->z + (spine->z - pelvis->z) * lid_fraction;
	}
	for (index = 0; index < node_count; index++)
	{
		if (legs->collapsed[index])
		{
			pose[index] = nodes[legs->spine_node_index];
			pose[index].scale *= FIRST_PERSON_LEGS_COLLAPSE;
			pose[index].position = lid;
		}
		else
			pose[index] = nodes[index];
		{
			real x = pose[index].position.x - pivot.x;
			real y = pose[index].position.y - pivot.y;

			pose[index].position.x = pivot.x + cosine * x - sine * y + back.i * FIRST_PERSON_LEGS_SET_BACK;
			pose[index].position.y = pivot.y + sine * x + cosine * y + back.j * FIRST_PERSON_LEGS_SET_BACK;
		}
		first_person_legs_turn(&pose[index].forward, cosine, sine);
		first_person_legs_turn(&pose[index].left, cosine, sine);
		first_person_legs_turn(&pose[index].up, cosine, sine);
	}
	return pose;
}

