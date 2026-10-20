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
the pelvis's own ancestors is drawn onto the spine's joint, shrunk to
nothing (FIRST_PERSON_LEGS_COLLAPSE), so the upper body's skin closes the
waist as the vertices blended between the hips and the spine are pulled in
towards the joint. The model's geometry is never read.

The pose is each local player's own (split-screen views draw their own
bodies). The waist's top, where the upper body closes, is kept out of the
view as Halo 2 keeps its first-person body's: the body is set back behind
the eye, along its own level facing, a little looking ahead, and as far as
keeps the waist below the bottom of the view looking down (up to
FIRST_PERSON_LEGS_SET_BACK_MAXIMUM). The waist is taken from the skeleton
too: spheres around the pelvis's and the spine's joints, as wide as the
hips (FIRST_PERSON_LEGS_WAIST times the hip joints' distance from the
pelvis's).
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

enum
{
	FIRST_PERSON_LEGS_MODELS = 8,
	/* render_cameras.c's _render_frustum_plane_bottom (its enum is its own) */
	FIRST_PERSON_LEGS_BOTTOM_PLANE = 2,
};

/* how far behind the eye the body is drawn at least, world units (about
9 cm), and at most (about 1 m), along its own level facing (tuned by eye) */
#define FIRST_PERSON_LEGS_SET_BACK 0.03f
#define FIRST_PERSON_LEGS_SET_BACK_MAXIMUM 0.33f
/* how wide the waist is, as a multiple of the hip joints' distance from the
pelvis's: the skin around the hips reaches past their joints (tuned by eye) */
#define FIRST_PERSON_LEGS_WAIST 2.f
/* how little the set-back moves the waist's distance from the bottom of the
view, per unit, before setting it back is no use (looking up) */
#define FIRST_PERSON_LEGS_TOWARD_MINIMUM 0.05f
/* the scale the upper body is drawn at on the spine's joint: small enough
that its skin is within a millimetre of the joint, not zero, which would
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

/* how far back the body is drawn: at least FIRST_PERSON_LEGS_SET_BACK, and as
far as keeps the waist below the bottom of the view (the bottom plane, whose
distances are positive outside the view), at most
FIRST_PERSON_LEGS_SET_BACK_MAXIMUM */
static real first_person_legs_set_back(
	struct model const *model,
	short node_count,
	short spine_index,
	real_matrix4x3 const *nodes,
	real_vector3d const *back,
	real_plane3d const *bottom)
{
	short pelvis_index = TAG_BLOCK_GET_ELEMENT(&model->nodes, spine_index, struct model_node)->parent_node_index;
	real toward = dot_product3d(&bottom->n, back);
	real radius = 0.f;
	real set_back = FIRST_PERSON_LEGS_SET_BACK;
	short node_index;

	/* (moved back, the waist barely leaves the view, or comes into it) */
	if (toward < FIRST_PERSON_LEGS_TOWARD_MINIMUM)
		return set_back;
	/* (the hip joints: the pelvis's children but the spine) */
	for (node_index = 0; node_index < node_count; node_index++)
	{
		if (node_index != spine_index &&
			TAG_BLOCK_GET_ELEMENT(&model->nodes, node_index, struct model_node)->parent_node_index == pelvis_index)
		{
			real distance = distance3d(&nodes[node_index].position, &nodes[pelvis_index].position);

			if (distance > radius)
				radius = distance;
		}
	}
	radius *= FIRST_PERSON_LEGS_WAIST;
	{
		real pelvis_needed = (radius - plane3d_distance_to_point(bottom, &nodes[pelvis_index].position)) / toward;
		real spine_needed = (radius - plane3d_distance_to_point(bottom, &nodes[spine_index].position)) / toward;

		set_back = MAX(set_back, MAX(pelvis_needed, spine_needed));
	}
	if (!(set_back <= FIRST_PERSON_LEGS_SET_BACK_MAXIMUM))
		set_back = FIRST_PERSON_LEGS_SET_BACK_MAXIMUM;
	return set_back;
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
	real_vector3d back;
	real set_back;
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
	/* (back from the body's own facing, level) */
	back.i = -nodes[0].forward.i;
	back.j = -nodes[0].forward.j;
	back.k = 0.f;
	if (normalize3d(&back) <= 0.f)
		back = *global_zero_vector3d;
	set_back = first_person_legs_set_back(model, node_count, legs->spine_node_index, nodes, &back,
		&render.frustum.world_planes[FIRST_PERSON_LEGS_BOTTOM_PLANE]);
	for (index = 0; index < node_count; index++)
	{
		if (legs->collapsed[index])
		{
			pose[index] = nodes[legs->spine_node_index];
			pose[index].scale *= FIRST_PERSON_LEGS_COLLAPSE;
		}
		else
			pose[index] = nodes[index];
		pose[index].position.x += back.i * set_back;
		pose[index].position.y += back.j * set_back;
	}
	return pose;
}
