/*
SHIELD_LOOK.C

Optional CMT Snow Grove plasma profiles on positively identified stock
Spartan shields. The map owns its tags and pixels; this module owns only
the draw-time shader copies. See docs/shield_look.md for the inspected
data, conservative eligibility policy and shared-engine hooks.
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "real_math.h"
#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "models/model_definitions.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "rasterizer/rasterizer_model_types.h"
#include "scenario/scenario.h"
#include "shaders/shader_definitions.h"
#include "units/units.h"
#include "shield_look.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *config_string(const char *name);
boolean overshield_effect_arms_values(long unit_index, real const *values, real *arms);

/* Plasma tag layout, shared by the validated Xbox, CE and MCC runtime
tags. Keep the references intact: each map's own texture cache serves them. */
struct shield_look_plasma
{
	struct shader shader;
	byte reserved28[4];
	short intensity_source;
	short pad2E;
	real intensity_exponent;
	short offset_source;
	short pad36;
	real offset_amount;
	real offset_exponent;
	byte reserved40[0x20];
	real perpendicular_alpha;
	real_rgb_color perpendicular_color;
	real parallel_alpha;
	real_rgb_color parallel_color;
	short color_source;
	byte reserved82[0x3E];
	real primary_period;
	real_vector3d primary_direction;
	real primary_scale;
	struct tag_reference primary_map;
	byte reservedE4[0x24];
	real secondary_period;
	real_vector3d secondary_direction;
	real secondary_scale;
	struct tag_reference secondary_map;
	byte reserved12C[0x20];
};

typedef char shield_look_plasma_size_check[sizeof(struct shield_look_plasma) == 0x14C ? 1 : -1];
typedef char shield_look_primary_map_check[offsetof(struct shield_look_plasma, primary_map) == 0xD4 ? 1 : -1];
typedef char shield_look_secondary_map_check[offsetof(struct shield_look_plasma, secondary_map) == 0x11C ? 1 : -1];

struct shield_look_model_shader
{
	struct tag_reference shader;
	short permutation;
	short pad;
	long unused[3];
};

enum { SHIELD_LOOK_MAXIMUM = 64 };
struct shield_look_entry
{
	long definition_index;
	long hands_index;
	boolean eligible;
	struct shield_look_plasma body;
	struct shield_look_plasma outer_body;
	struct shield_look_plasma arms;
};

/* Never recycled during a map: transparent draw packets retain pointers
until they are submitted, including each split-screen view. */
static struct shield_look_entry shield_looks[SHIELD_LOOK_MAXIMUM];
static short shield_look_count;
static real shield_look_arm_values[MAXIMUM_LOCAL_PLAYERS][NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS];

static boolean shield_look_enabled(void)
{
	char const *value = config_string("display.shield_look");
	return value && (!strcmp(value, "enhanced") || !strcmp(value, "enhanced2"));
}

static uint32_t shield_look_hash(uint32_t hash, void const *data, size_t size)
{
	byte const *bytes = data;
	size_t index;

	for (index = 0; index < size; index++)
		hash = (hash ^ bytes[index]) * UINT32_C(16777619);
	return hash;
}

static boolean shield_look_stock_shader(struct shield_look_plasma const *plasma)
{
	struct shield_look_plasma copy;

	if (!cache_file_tag_cache_contains(plasma, sizeof(*plasma)) || plasma->shader.base.type != 10)
		return FALSE;
	copy = *plasma;
	/* Runtime type/padding and relocated tag references are not appearance. */
	copy.shader.base.type = 0;
	copy.shader.base.pad = 0;
	memset(&copy.primary_map, 0, sizeof(copy.primary_map));
	memset(&copy.secondary_map, 0, sizeof(copy.secondary_map));
	return shield_look_hash(UINT32_C(2166136261), &copy, sizeof(copy)) == UINT32_C(0x762FCC1F);
}

static boolean shield_look_stock_noise(long tag_index)
{
	struct bitmap_group *group;
	struct bitmap_data *bitmap;
	byte *pixels;
	long size;
	uint32_t hash;
	boolean complete = FALSE;

	if (tag_index == NONE || tag_get_group_tag(tag_index) != 'bitm')
		return FALSE;
	group = bitmap_group_get(tag_index);
	if (!cache_file_tag_cache_contains(group, sizeof(*group)) || group->bitmaps.count != 1 ||
		!cache_file_tag_cache_contains(group->bitmaps.address, sizeof(struct bitmap_data)))
		return FALSE;
	bitmap = group->bitmaps.address;
	if (bitmap->width != 32 || bitmap->height != 32 || bitmap->depth != 32 ||
		bitmap->type != 1 || bitmap->mipmap_count != 5 || (bitmap->format != 3 && bitmap->format != 11))
		return FALSE;
	/* Compression, palettes and linear addressing would interpret the
	stored bytes differently. Only the inspected volume layouts qualify. */
	if ((bitmap->flags & 0x1E) != 0 && (bitmap->flags & 0x1E) != 8)
		return FALSE;
	/* Full mip chain, excluding hardware padding. The cache rounds Xbox
	reads up to a sector; the allocation includes that tail. */
	size = bitmap->format == 3 ? 74898 : 149796;
	if (bitmap->pixels_size < size || bitmap->pixels_offset < 0)
		return FALSE;
	pixels = malloc((size + 4095) & ~4095);
	if (!pixels)
		return FALSE;
	memset(pixels, 0, (size + 4095) & ~4095);
	cache_file_read(tag_index, bitmap->pixels_offset, size, pixels, &complete, TRUE);
	while (!complete)
		SwitchToThread();
	hash = shield_look_hash(UINT32_C(2166136261), pixels, size);
	free(pixels);
	/* Xbox A8Y8, CE ARGB, and the same CE pixels packed by MCC's existing
	bitmap adapter. No foreign resource path or adapter is used here. */
	return complete == TRUE && (bitmap->format == 3 ?
		(bitmap->flags & 8) && hash == UINT32_C(0xF2F70788) :
		(bitmap->flags & 8) ? hash == UINT32_C(0x18AC1E94) : hash == UINT32_C(0xE7F72DA4));
}

static boolean shield_look_stock_function(struct object_definition const *definition)
{
	struct object_function_definition const *function;
	uint32_t hash;

	/* The outgoing B function drives stock shield hits. Extra functions or
	a changed mapping may author a different shield even with stock tags. */
	if (definition->object.function_modes[0] != 4 || definition->object.function_modes[1] != 2 ||
		definition->object.function_modes[2] || definition->object.function_modes[3] ||
		definition->object.functions.count != 2 ||
		!cache_file_tag_cache_contains(definition->object.functions.address, 2 * sizeof(*function)))
		return FALSE;
	function = (struct object_function_definition const *)definition->object.functions.address + 1;
	hash = shield_look_hash(UINT32_C(2166136261), function, 60);
	hash = shield_look_hash(hash, &function->runtime_reciprocal_bounds_range, 4 * sizeof(real));
	return hash == UINT32_C(0xD130B3A8);
}

/* The stock body includes two transparent generator lights. They are not
shield shells. Recognize their complete material and texture, rather than
excluding every Chicago material or trusting a map's tag names. */
static boolean shield_look_stock_light_texture(long tag_index)
{
	struct bitmap_group *group;
	struct bitmap_data *bitmap;
	byte *pixels;
	boolean complete = FALSE;
	uint32_t hash;

	if (tag_index == NONE || tag_get_group_tag(tag_index) != 'bitm')
		return FALSE;
	group = bitmap_group_get(tag_index);
	if (!cache_file_tag_cache_contains(group, sizeof(*group)) || group->bitmaps.count != 1 ||
		!cache_file_tag_cache_contains(group->bitmaps.address, sizeof(*bitmap)))
		return FALSE;
	bitmap = group->bitmaps.address;
	if (bitmap->width != 512 || bitmap->height != 512 || bitmap->depth != 1 ||
		bitmap->type != 0 || bitmap->format != 14 || bitmap->mipmap_count != 9 ||
		(bitmap->flags & 0x1E) != 2 || bitmap->pixels_size < 174776 || bitmap->pixels_offset < 0)
		return FALSE;
	pixels = malloc(176128);
	if (!pixels)
		return FALSE;
	memset(pixels, 0, 176128);
	cache_file_read(tag_index, bitmap->pixels_offset, 174776, pixels, &complete, TRUE);
	while (!complete)
		SwitchToThread();
	hash = shield_look_hash(UINT32_C(2166136261), pixels, 174776);
	free(pixels);
	return complete == TRUE && (hash == UINT32_C(0x71B35CD8) ||
		hash == UINT32_C(0x42B1A420) || hash == UINT32_C(0xE5E7E894));
}

static boolean shield_look_stock_light(struct shader const *shader)
{
	byte copy[0x6C];
	byte stage[0xDC];
	struct tag_reference lens_flare;
	struct tag_reference bitmap;
	struct tag_block layers;
	struct tag_block maps;
	uint32_t hash;

	if (!cache_file_tag_cache_contains(shader, sizeof(copy)) || shader->base.type != 6)
		return FALSE;
	memcpy(copy, shader, sizeof(copy));
	memcpy(&lens_flare, copy + 0x38, sizeof(lens_flare));
	memcpy(&layers, copy + 0x48, sizeof(layers));
	memcpy(&maps, copy + 0x54, sizeof(maps));
	if (lens_flare.index != NONE || layers.count != 0 || maps.count != 1 ||
		!cache_file_tag_cache_contains(maps.address, sizeof(stage)))
		return FALSE;
	memset(copy + 0x24, 0, 4);
	memset(copy + 0x38, 0, 0x28);
	hash = shield_look_hash(UINT32_C(2166136261), copy, sizeof(copy));
	if (hash != UINT32_C(0x66E15FE3) && hash != UINT32_C(0x015C769C))
		return FALSE;
	memcpy(stage, maps.address, sizeof(stage));
	memcpy(&bitmap, stage + 0x6C, sizeof(bitmap));
	memset(stage + 0x6C, 0, sizeof(bitmap));
	return shield_look_hash(UINT32_C(2166136261), stage, sizeof(stage)) == UINT32_C(0x7FDA0175) &&
		shield_look_stock_light_texture(bitmap.index);
}

static boolean shield_look_model_has_no_overlay(long model_index)
{
	struct model *model;
	struct shield_look_model_shader const *references;
	struct shader const *surface;
	word flags;
	long index;
	unsigned long group_tag;

	if (model_index == NONE)
		return FALSE;
	model = model_definition_get(model_index);
	if (!cache_file_tag_cache_contains(model, sizeof(*model)) || model->shaders.count <= 0 ||
		model->shaders.count > MAXIMUM_SHADERS_PER_MODEL ||
		!cache_file_tag_cache_contains(model->shaders.address, model->shaders.count * sizeof(*references)))
		return FALSE;
	references = model->shaders.address;
	for (index = 0; index < model->shaders.count; index++)
	{
		/* A plasma shell, Chicago layer or other transparent surface may
		be an authored shield. Unknown surfaces are preserved too. */
		if (references[index].shader.index == NONE)
			return FALSE;
		group_tag = tag_get_group_tag(references[index].shader.index);
		if (group_tag != 'soso' && group_tag != 'schi')
			return FALSE;
		surface = shader_definition_get(references[index].shader.index);
		if (group_tag == 'schi' && shield_look_stock_light(surface))
			continue;
		if (group_tag != 'soso')
			return FALSE;
		if (!cache_file_tag_cache_contains(surface, sizeof(*surface) + sizeof(flags)))
			return FALSE;
		/* shader_model can also author a transparent shell with its
		alpha-blended-decal flag (rasterizer_xbox_models.c, bit 3). */
		memcpy(&flags, (byte const *)surface + sizeof(*surface), sizeof(flags));
		if (flags & (1 << 3))
			return FALSE;
	}
	return TRUE;
}

static void shield_look_profile(struct shield_look_plasma *plasma, boolean arms)
{
	/* CMT Snow Grove's body tag 1935 and hands tag 2163. These are shader
	parameters, not extracted textures, geometry or executable code. */
	plasma->intensity_source = 2;
	plasma->intensity_exponent = 1.f;
	plasma->offset_source = 3;
	plasma->offset_amount = arms ? 0.009f : 0.035f;
	plasma->offset_exponent = 2.f;
	plasma->perpendicular_alpha = plasma->parallel_alpha = 1.f;
	plasma->perpendicular_color.red = 0.909804f;
	plasma->perpendicular_color.green = 0.772549f;
	plasma->perpendicular_color.blue = 0.f;
	plasma->parallel_color.red = 0.937255f;
	plasma->parallel_color.green = 0.694118f;
	plasma->parallel_color.blue = 0.0627451f;
	plasma->color_source = 0;
	plasma->primary_period = 2.1f;
	plasma->primary_direction.i = 2.f;
	plasma->primary_direction.j = 0.f;
	plasma->primary_direction.k = -0.5f;
	plasma->primary_scale = arms ? 45.f : 18.f;
	plasma->secondary_period = 3.2f;
	plasma->secondary_direction.i = 0.f;
	plasma->secondary_direction.j = 2.f;
	plasma->secondary_direction.k = 0.5f;
	plasma->secondary_scale = arms ? 62.f : 14.f;
}

static struct shield_look_entry *shield_look_for_unit(long object_index, long hands_index)
{
	struct object_datum *object;
	struct object_definition *definition;
	struct shield_look_plasma const *plasma;
	struct shield_look_entry *entry;
	short index;
	long shader_index;

	if (!shield_look_enabled())
		return NULL;
	object = object_try_and_get(object_index);
	if (!object || object->object.type != _object_type_biped)
		return NULL;
	for (index = 0; index < shield_look_count; index++)
	{
		entry = &shield_looks[index];
		if (entry->definition_index == object->definition_index && entry->hands_index == hands_index)
			return entry->eligible ? entry : NULL;
	}
	/* Refuse overflow rather than recycling memory still used by a draw. */
	if (shield_look_count == SHIELD_LOOK_MAXIMUM)
		return NULL;
	entry = &shield_looks[shield_look_count++];
	entry->definition_index = object->definition_index;
	entry->hands_index = hands_index;
	definition = object_definition_get(object->definition_index);
	if (!cache_file_tag_cache_contains(definition, sizeof(*definition)))
		return NULL;
	shader_index = definition->object.modifier_shader.index;
	if (shader_index == NONE || tag_get_group_tag(shader_index) != 'spla' ||
		!shield_look_stock_function(definition) ||
		!shield_look_model_has_no_overlay(definition->object.model.index) ||
		!shield_look_model_has_no_overlay(hands_index))
		return NULL;
	plasma = (struct shield_look_plasma const *)shader_definition_get(shader_index);
	if (!shield_look_stock_shader(plasma) || plasma->primary_map.index != plasma->secondary_map.index ||
		!shield_look_stock_noise(plasma->primary_map.index))
		return NULL;
	entry->body = entry->arms = *plasma;
	shield_look_profile(&entry->body, FALSE);
	shield_look_profile(&entry->arms, TRUE);
	entry->outer_body = entry->body;
	/* A small normal displacement follows the visible hit intensity (B),
	not the unused stock C function. At full intensity this is 3.8 cm;
	the square root keeps a gentle outer layer while the hit fades. */
	entry->outer_body.offset_source = 2;
	entry->outer_body.offset_amount = 0.0125f;
	entry->outer_body.offset_exponent = 0.5f;
	entry->eligible = TRUE;
	return entry;
}

void shield_look_reset(void)
{
	memset(shield_looks, 0, sizeof(shield_looks));
	memset(shield_look_arm_values, 0, sizeof(shield_look_arm_values));
	shield_look_count = 0;
}

struct shader *shield_look_body(long object_index, struct shader *original)
{
	struct game_globals *globals;
	struct game_globals_first_person_interface *first_person;
	struct shield_look_entry *entry;

	if (!shield_look_enabled())
		return original;
	globals = scenario_get_game_globals();
	if (!cache_file_tag_cache_contains(globals, sizeof(*globals)) || globals->first_person_interface.count != 1 ||
		!cache_file_tag_cache_contains(globals->first_person_interface.address, sizeof(*first_person)))
		return original;
	first_person = TAG_BLOCK_GET_ELEMENT(&globals->first_person_interface, 0, struct game_globals_first_person_interface);
	entry = shield_look_for_unit(object_index, first_person->hands.index);
	if (!entry)
		return original;
	/* Keep both copies stable: queued draws can retain either profile. */
	return !strcmp(config_string("display.shield_look"), "enhanced2") ?
		&entry->outer_body.shader : &entry->body.shader;
}

boolean shield_look_arms(short local_player_index, long unit_index, long hands_model_index,
	struct render_model_effect *effect)
{
	struct shield_look_entry *entry;
	struct unit_datum *unit;
	real *values;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return FALSE;
	entry = shield_look_for_unit(unit_index, hands_model_index);
	if (!entry)
		return FALSE;
	unit = unit_get(unit_index);
	values = shield_look_arm_values[local_player_index];
	if (!overshield_effect_arms_values(unit_index, unit->object.outgoing_function_values, values))
		memcpy(values, unit->object.outgoing_function_values, sizeof(shield_look_arm_values[0]));
	effect->modifier_shader = &entry->arms.shader;
	effect->modifier_animation.colors = unit->object.outgoing_change_colors;
	effect->modifier_animation.values = values;
	return TRUE;
}
