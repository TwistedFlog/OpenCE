/* The local player's own legs in first person (port/linux/game/first_person_legs.c) */
#include "harness.h"
#include <math.h>
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAXIMUM_LOCAL_PLAYERS 4
#define MAXIMUM_NODES_PER_MODEL 64
#define TAG_BLOCK_GET_ELEMENT(block, index, type) (&((type *)(block)->address)[index])
typedef unsigned short word;
typedef struct { real scale; real_vector3d forward, left, up; real_point3d position; } real_matrix4x3;
struct tag_block { long count; void *address; };
struct model_node { short parent_node_index; };
struct model { struct tag_block nodes; };
struct biped_definition
{
	struct { struct { long index; } model; } object;
	struct { short runtime_pelvis_node_index, runtime_head_node_index; } biped;
};
struct object_datum
{
	long definition_index;
	struct
	{
		unsigned long flags;
		real_vector3d forward;
		short type;
		word damage_flags;
		long parent_object_index;
		struct { short size, offset; } node_matrices;
	} object;
};
enum { _object_type_biped = 0, _object_type_vehicle = 1 };
enum { _object_invisible_bit = 0 };
enum { _object_dead_bit = 2 };
enum { _director_perspective_first_person = 0, _director_perspective_third_person = 1 };

static real_vector3d const zero_vector = {0, 0, 0};
static real_vector3d const *global_zero_vector3d = &zero_vector;
static real normalize3d(real_vector3d *v)
{
	real length = sqrtf(v->i * v->i + v->j * v->j + v->k * v->k);
	if (length > 0) { v->i /= length; v->j /= length; v->k /= length; }
	return length;
}

static int setting = 1;
static int config_boolean(const char *name) { (void)name; return setting; }
static unsigned long config_changes(void) { static unsigned long changes; return ++changes; }
static int logs;
static void platform_log(const char *format, ...) { (void)format; logs++; }
static char const *tag_get_name(long index) { (void)index; return "characters\\cyborg\\cyborg"; }

/* two local players, each with a biped (objects 0 and 1, definitions 10 and 11) */
static short perspective[MAXIMUM_LOCAL_PLAYERS];
static struct { long unit_index; } players[2] = {{0}, {1}};
static struct object_datum objects[2];
static struct biped_definition definitions[2];
static struct model model;
static struct model_node skeleton[16];
static real_matrix4x3 node_matrices[2][16];
static short director_get_perspective(short local) { return perspective[local]; }
static long local_player_get_player_index(short local) { return local < 2 ? local : NONE; }
#define player_get(index) (&players[index])
static struct object_datum *object_try_and_get(long index) { return index >= 0 && index < 2 ? &objects[index] : NULL; }
#define biped_definition_get(index) (&definitions[(index) - 10])
static struct model *model_definition_get(long index) { (void)index; return &model; }
static real_matrix4x3 *object_get_node_matrices(long index) { return node_matrices[index]; }
#include "under_test.inc"

/* 0 frame; 1 pelvis; 2 spine, 3 spine1, 4 head, 9 clavicle; 5 l thigh, 6 l calf; 7 r thigh, 8 r calf; 10 a node
on the frame (a gun's) */
enum { FRAME, PELVIS, SPINE, SPINE1, HEAD, L_THIGH, L_CALF, R_THIGH, R_CALF, CLAVICLE, GUN, NODES };
static short const parents[NODES] = {NONE, FRAME, PELVIS, SPINE, SPINE1, PELVIS, L_THIGH, PELVIS, R_THIGH, SPINE1, FRAME};

static void world(void)
{
	short i, o;
	for (i = 0; i < NODES; i++) skeleton[i].parent_node_index = parents[i];
	model.nodes.count = NODES;
	model.nodes.address = skeleton;
	for (o = 0; o < 2; o++) {
		memset(&objects[o], 0, sizeof(objects[o]));
		objects[o].definition_index = 10 + o;
		objects[o].object.type = _object_type_biped;
		objects[o].object.parent_object_index = NONE;
		objects[o].object.forward.i = 1;
		objects[o].object.node_matrices.size = (short)(NODES * sizeof(real_matrix4x3));
		definitions[o].object.model.index = 5;
		definitions[o].biped.runtime_pelvis_node_index = PELVIS;
		definitions[o].biped.runtime_head_node_index = HEAD;
		for (i = 0; i < NODES; i++) {
			memset(&node_matrices[o][i], 0, sizeof(real_matrix4x3));
			node_matrices[o][i].scale = 1;
			node_matrices[o][i].forward.i = 1;
			node_matrices[o][i].position.x = 10.f * o + i;
			node_matrices[o][i].position.z = (real)i;
		}
	}
	perspective[0] = perspective[1] = _director_perspective_first_person;
	setting = 1;
	logs = 0;
	first_person_legs_reset();
}

static int same_place(real_matrix4x3 const *a, real_matrix4x3 const *b, real dx)
{
	return fabsf(a->position.x - (b->position.x + dx)) < 1e-5f && fabsf(a->position.y - b->position.y) < 1e-5f &&
		fabsf(a->position.z - b->position.z) < 1e-5f;
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	real const back = -FIRST_PERSON_LEGS_SET_BACK;
	world();
	CASE("off") {
		setting = 0;
		CHECK(!first_person_legs_node_matrices(0, 0), "off, the body is not drawn in its own view");
	}
	CASE("legs") {
		real_matrix4x3 const *pose = first_person_legs_node_matrices(0, 0);
		real_matrix4x3 lid = node_matrices[0][PELVIS];
		short i;
		/* (part of the way from the pelvis's joint to the spine's) */
		lid.position.x += (node_matrices[0][SPINE].position.x - lid.position.x) * FIRST_PERSON_LEGS_LID;
		lid.position.z += (node_matrices[0][SPINE].position.z - lid.position.z) * FIRST_PERSON_LEGS_LID;
		CHECK(FIRST_PERSON_LEGS_LID > 0 && FIRST_PERSON_LEGS_LID < 1, "the lid between the two joints");
		CHECK(pose != NULL, "the player's own biped, on foot");
		for (i = 0; i < NODES; i++) {
			if (i == FRAME || i == PELVIS || i == L_THIGH || i == L_CALF || i == R_THIGH || i == R_CALF) {
				CHECK(same_place(&pose[i], &node_matrices[0][i], back), "node %d kept, set back", i);
				CHECK(pose[i].scale == 1, "node %d at its own scale", i);
			} else {
				CHECK(same_place(&pose[i], &lid, back), "node %d onto the waist's lid", i);
				CHECK(pose[i].scale > 0 && pose[i].scale <= 0.001f, "node %d shrunk, not to nothing", i);
			}
		}
		CHECK(logs == 0, "nothing logged");
	}
	CASE("setback") {
		real_matrix4x3 const *pose;
		/* (the first node's axis up the spine, as a pelvis's is: not the body's facing) */
		node_matrices[0][FRAME].forward.i = 0.1f;
		node_matrices[0][FRAME].forward.j = 0.3f;
		node_matrices[0][FRAME].forward.k = 0.95f;
		pose = first_person_legs_node_matrices(0, 0);
		CHECK(pose && same_place(&pose[L_CALF], &node_matrices[0][L_CALF], back),
			"back from the object's facing, not its first node's");
		objects[0].object.forward.i = 0.6f;
		objects[0].object.forward.k = 0.8f; /* (a body tilted: the set-back stays level) */
		pose = first_person_legs_node_matrices(0, 0);
		CHECK(pose && same_place(&pose[L_CALF], &node_matrices[0][L_CALF], back), "level, the whole set-back");
		objects[0].object.forward.i = 0;
		objects[0].object.forward.k = 1;
		pose = first_person_legs_node_matrices(0, 0);
		CHECK(pose && same_place(&pose[L_CALF], &node_matrices[0][L_CALF], 0), "no level facing, no set-back");
	}
	CASE("who") {
		CHECK(!first_person_legs_node_matrices(0, 1), "another player's body");
		CHECK(!first_person_legs_node_matrices(-1, 0) && !first_person_legs_node_matrices(MAXIMUM_LOCAL_PLAYERS, 0),
			"no such local player");
		CHECK(!first_person_legs_node_matrices(2, 0), "a local player without a player");
		perspective[0] = _director_perspective_third_person;
		CHECK(!first_person_legs_node_matrices(0, 0), "third person");
		perspective[0] = _director_perspective_first_person;
		objects[0].object.damage_flags = FLAG(_object_dead_bit);
		CHECK(!first_person_legs_node_matrices(0, 0), "dead");
		objects[0].object.damage_flags = 0;
		objects[0].object.flags = FLAG(_object_invisible_bit);
		CHECK(!first_person_legs_node_matrices(0, 0), "hidden");
		objects[0].object.flags = 0;
		objects[0].object.parent_object_index = 1;
		CHECK(!first_person_legs_node_matrices(0, 0), "riding something");
		objects[0].object.parent_object_index = NONE;
		objects[0].object.type = _object_type_vehicle;
		CHECK(!first_person_legs_node_matrices(0, 0), "not a biped");
		objects[0].object.type = _object_type_biped;
		objects[0].object.node_matrices.size = (short)((NODES - 1) * sizeof(real_matrix4x3));
		CHECK(!first_person_legs_node_matrices(0, 0), "fewer matrices than the model's nodes");
		objects[0].object.node_matrices.size = (short)(NODES * sizeof(real_matrix4x3));
		CHECK(first_person_legs_node_matrices(0, 0) != NULL, "and drawn again");
	}
	CASE("unusable") {
		definitions[0].biped.runtime_pelvis_node_index = NONE;
		CHECK(!first_person_legs_node_matrices(0, 0) && !first_person_legs_node_matrices(0, 0), "no pelvis");
		CHECK(logs == 1, "said once (%d)", logs);
		definitions[1].biped.runtime_head_node_index = L_CALF; /* (a head not below the spine: under a leg) */
		skeleton[L_THIGH].parent_node_index = FRAME;
		CHECK(!first_person_legs_node_matrices(1, 1), "a head not below the pelvis");
		CHECK(logs == 2, "said once more (%d)", logs);
		world();
		skeleton[L_THIGH].parent_node_index = SPINE;
		skeleton[R_THIGH].parent_node_index = SPINE;
		CHECK(!first_person_legs_node_matrices(0, 0), "no legs below the pelvis but the spine");
		CHECK(logs == 1, "said (%d)", logs);
	}
	CASE("loops") {
		skeleton[SPINE].parent_node_index = SPINE1; /* (spine and spine1 each other's parent) */
		CHECK(!first_person_legs_node_matrices(0, 0), "a loop in a map's parents ends");
		world();
		skeleton[PELVIS].parent_node_index = 40; /* (a parent past the nodes) */
		CHECK(first_person_legs_node_matrices(0, 0) != NULL, "a parent past the nodes is no node");
	}
	CASE("split") {
		real_matrix4x3 const *first = first_person_legs_node_matrices(0, 0);
		real_matrix4x3 const *second = first_person_legs_node_matrices(1, 1);
		CHECK(first && second && first != second, "each view's own pose");
		CHECK(same_place(&first[L_CALF], &node_matrices[0][L_CALF], back), "the first view keeps its body's pose");
		CHECK(same_place(&second[L_CALF], &node_matrices[1][L_CALF], back), "the second its own");
	}
	CASE("reset") {
		definitions[0].biped.runtime_pelvis_node_index = NONE;
		CHECK(!first_person_legs_node_matrices(0, 0), "a map whose biped has no legs");
		definitions[0].biped.runtime_pelvis_node_index = PELVIS; /* (the next map's, at the same index) */
		CHECK(!first_person_legs_node_matrices(0, 0), "the same map: as found");
		first_person_legs_reset();
		CHECK(first_person_legs_node_matrices(0, 0) != NULL, "a new map: found again");
	}
	return 0;
}
