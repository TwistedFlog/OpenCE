/*
FIRST_PERSON_LEGS.H

The local player's own legs in first person (first_person_legs.c).
*/

#ifndef FIRST_PERSON_LEGS_H
#define FIRST_PERSON_LEGS_H

/* the pose the local player's own body is drawn with in its first-person
view (display.first_person_legs), the legs alone, or NULL: not drawn, as
the stock game does not */
real_matrix4x3 const *first_person_legs_node_matrices(short local_player_index, long object_index);
/* the bipeds' legs found so far forgotten (a map loaded or unloaded: its
tag indices are another map's) */
void first_person_legs_reset(void);

#endif
