#include "BOX.H"

#include "CONTROL.H"
#include "DELTAPAK.H"
#include "DRAW.H"
#include "ITEMS.H"
#include "LOT.H"
#include "MATHS.H"
#include "SPECIFIC.H"
#include "TOMB4FX.H"

#ifdef PC_VERSION
#include "GAME.H"
#else
#include "SETUP.H"
#endif

#include "SPECTYPES.H"
#include <assert.h>
#include <stddef.h>
#include "CAMERA.H"

int number_boxes;
struct box_info* boxes;
unsigned short* overlap;
short* ground_zone[5][2];
unsigned short testclip;
unsigned short loops;

void DropBaddyPickups(struct ITEM_INFO* item)//259BC(<), 25BC8(<) (F)
{
	short pickup_number;
	short room_number;
	struct ITEM_INFO* pickup;

	for(pickup_number = item->carried_item; pickup_number != -1; pickup_number = pickup->carried_item)
	{
		pickup = &items[pickup_number];
		pickup->pos.x_pos = (item->pos.x_pos & 0xFFFFFC00) | 0x200;
		pickup->pos.z_pos = (item->pos.z_pos & 0xFFFFFC00) | 0x200;

		room_number = item->room_number;
		pickup->pos.y_pos = GetHeight(GetFloor(pickup->pos.x_pos, item->pos.y_pos, pickup->pos.z_pos, &room_number), 
			pickup->pos.x_pos, item->pos.y_pos, pickup->pos.z_pos);
		pickup->pos.y_pos -= GetBoundsAccurate(pickup)[3];

		ItemNewRoom(pickup_number, item->room_number);
		pickup->flags |= 0x20;
	}
}

int MoveCreature3DPos(struct PHD_3DPOS* srcpos, struct PHD_3DPOS* destpos, int velocity, short angdif, int angadd)// (F)
{
	int x = destpos->x_pos - srcpos->x_pos;
	int y = destpos->y_pos - srcpos->y_pos;
	int z = destpos->z_pos - srcpos->z_pos;
	int dist = phd_sqrt_asm(x * x + y * y + z * z);

	if (velocity < dist)
	{
		srcpos->x_pos += velocity * x / dist;
		srcpos->y_pos += velocity * y / dist;
		srcpos->z_pos += velocity * z / dist;
	}
	else
	{
		srcpos->x_pos = destpos->x_pos;
		srcpos->y_pos = destpos->y_pos;
		srcpos->z_pos = destpos->z_pos;
	}

	if (angdif <= angadd)
	{
		if (angdif >= -angadd)
			srcpos->y_rot = destpos->y_rot;
		else
			srcpos->y_rot -= angadd;
	}
	else
	{
		srcpos->y_rot += angadd;
	}

	return srcpos->x_pos == destpos->x_pos
		&& srcpos->y_pos == destpos->y_pos
		&& srcpos->z_pos == destpos->z_pos
		&& srcpos->y_rot == destpos->y_rot;
}

void CreatureYRot(struct PHD_3DPOS* srcpos, short angle, short angadd)//25738(<), ? (F)
{
	if (angadd < angle)
	{
		srcpos->y_rot += angadd;
		return;
	}//0x25768

	if (angle < -angadd)
	{
		srcpos->y_rot -= angadd;
		return;
	}//0x25788

	srcpos->y_rot += angle;

	return;
}

short SameZone(struct creature_info* creature, struct ITEM_INFO* target_item)//255F8(<), ? (F)
{
	struct room_info* r;
	short* zone;
	struct ITEM_INFO* item;

	item = &items[creature->item_num];

	zone = ground_zone[creature->LOT.zone][flip_status];

	r = &room[item->room_number];
	item->box_number = r->floor[((item->pos.z_pos - r->z) / 1024) + ((item->pos.x_pos - r->x) / 1024) * r->x_size].box;

	r = &room[target_item->room_number];
	target_item->box_number = r->floor[(target_item->pos.z_pos - r->z) / 1024 + ((target_item->pos.x_pos - r->x) / 1024) * r->x_size].box;
	
	return (zone[item->box_number] == zone[target_item->box_number]);
}

void FindAITargetObject(struct creature_info* creature, short obj_num)// (F)
{
	if (nAIObjects > 0)
	{
		struct ITEM_INFO* item = &items[creature->item_num];
		struct AIOBJECT* target_item = &AIObjects[0];
		short i;

		for (i = 0; i < nAIObjects; i++, target_item++)
		{
			if (target_item->object_number == obj_num
				&& target_item->trigger_flags == item->item_flags[3]
				&& target_item->room_number != 255)
			{
				short* zone = ground_zone[0][flip_status + 2 * creature->LOT.zone];
				struct room_info* r = &room[item->room_number];

				item->box_number = XZ_GET_SECTOR(r, item->pos.x_pos - r->x, item->pos.z_pos - r->z).box;
				target_item->box_number = XZ_GET_SECTOR(r, target_item->x - r->x, target_item->z - r->z).box;

				if (zone[item->box_number] == zone[target_item->box_number])
					break;
			}
		}

		creature->enemy = &creature->ai_target;

		creature->ai_target.object_number = target_item->object_number;
		creature->ai_target.room_number = target_item->room_number;
		creature->ai_target.pos.x_pos = target_item->x;
		creature->ai_target.pos.y_pos = target_item->y;
		creature->ai_target.pos.z_pos = target_item->z;
		creature->ai_target.pos.y_rot = target_item->y_rot;
		creature->ai_target.flags = target_item->flags;
		creature->ai_target.trigger_flags = target_item->trigger_flags;
		creature->ai_target.box_number = target_item->box_number;

		if (!(creature->ai_target.flags & 0x20))
		{
			creature->ai_target.pos.x_pos += CLICK * (SIN(creature->ai_target.pos.y_rot)) >> W2V_SHIFT;
			creature->ai_target.pos.z_pos += CLICK * (COS(creature->ai_target.pos.y_rot)) >> W2V_SHIFT;
		}
	}
}

void GetAITarget(struct creature_info* creature)
{
	S_Warn("[GetAITarget] - Unimplemented!\n");
}

short AIGuard(struct creature_info* creature)//24DF0(<), ? (F)
{
	int random;

	if (items[creature->item_num].ai_bits & 5)
	{
		return 0;
	}

	random = GetRandomControl();

	if (random < 256)
	{
		creature->head_right = TRUE;
		creature->head_left = TRUE;
	}
	else if (random < 384)
	{
		creature->head_right = FALSE;
		creature->head_left = TRUE;
	}
	else if (random < 512)
	{
		creature->head_right = TRUE;
		creature->head_left = FALSE;
	}

	if (!creature->head_left)
		return (creature->head_right) << 12;

	if (creature->head_right)
		return 0;

	return -16384;
}

void AlertNearbyGuards(struct ITEM_INFO* item)//24D20(<), 24F2C(<) (F)
{
	int slot = 4;
	struct creature_info* cinfo = baddie_slots;
	struct ITEM_INFO* target;
	long x = 0;
	long y = 0;
	long z = 0;
	long distance = 0;
	int i = 0;

	//loc_24D3C
	for (i = 0; i < slot; i++)
	{
		if (cinfo->item_num == -1)
		{
			continue;
		}

		target = &items[cinfo->item_num + i];

		//Rooms match, alert the guards!
		if (item->room_number == target->room_number)
		{
			//24DCC
			cinfo->alerted = 1;
			continue;
		}

		x = (target->pos.x_pos - item->pos.x_pos) / 64;
		y = (target->pos.y_pos - item->pos.y_pos) / 64;
		z = (target->pos.z_pos - item->pos.z_pos) / 64;

		distance = (x * x) + (y * y) + (z * z);

		//Though the item is not in the same room.
		//If the distance between them is < 0x1F40, alert the guards!
		if (distance < 0x1F40)
		{
			//24DCC
			cinfo->alerted = 1;
		}
	}

	return;
}

void AlertAllGuards(short item_number)// (F)
{
	int slot;
	struct creature_info* cinfo = &baddie_slots[0];
	short obj_number = items[item_number].object_number;

	for(slot = 0; slot < 6; slot++, cinfo++)
	{
		struct ITEM_INFO* target;

		if (cinfo->item_num == -1)
		{
			continue;
		}

		target = &items[cinfo->item_num];

		if (obj_number == target->object_number)
		{
			if (target->status == ITEM_ACTIVE)
			{
				cinfo->alerted = TRUE;
			}
		}
	}
}

void CreatureKill(struct ITEM_INFO* item, int kill_anim, int kill_state, short lara_anim)
{
	S_Warn("[CreatureKill] - Unimplemented!\n");
}

int CreatureVault(short item_number, short angle, int vault, int shift)
{
	S_Warn("[CreatureVault] - Unimplemented!\n");
	return 0;
}

short CreatureEffectT(struct ITEM_INFO* item, struct BITE_INFO* bite, short damage, short angle, short (*generate)(long x, long y, long z, short speed, short yrot, short room_number))// (F)
{
	struct PHD_VECTOR pos;

	pos.x = bite->x;
	pos.y = bite->y;
	pos.z = bite->z;

	GetJointAbsPosition(item, &pos, bite->mesh_num);

	return generate(pos.x, pos.y, pos.z, damage, angle, item->room_number);
}

short CreatureEffect(struct ITEM_INFO* item, struct BITE_INFO* bite, short(*generate)(long x, long y, long z, short speed, short yrot, short room_number))// (F)
{
	struct PHD_VECTOR pos;

	pos.x = bite->x;
	pos.y = bite->y;
	pos.z = bite->z;

	GetJointAbsPosition(item, &pos, bite->mesh_num);

	return generate(pos.x, pos.y, pos.z, item->speed, item->pos.y_rot, item->room_number);
}

void CreatureUnderwater(struct ITEM_INFO* item, long depth)//s0, s1 2468C(<) ?
{
	long water_level; // $v0
	long floorheight; // $v1
	short room_number; // stack offset -24

	S_Warn("[CreatureUnderwater] - Unimplemented!\n");
}

void CreatureFloat(short item_number)
{
	S_Warn("[CreatureFloat] - Unimplemented!\n");
}

void CreatureJoint(struct ITEM_INFO* item, short joint, short required)//24484(<) ?
{
	short change;

	if (item->data == NULL)
	{
		return;
	}

	change = ((short*) item->data)[joint] - required;

	if (change < -0x222)
	{
		change -= 0x222;
	}
	else
	{
		change += 0x222;
	}

	((short*) item->data)[joint] += change;

	if (((short*) item->data)[joint] < 0x3001)
	{
		((short*) item->data)[joint] -= 0x3000;
	}
	else
	{
		((short*) item->data)[joint] += 0x3000;
	}
}

void CreatureTilt(struct ITEM_INFO* item, short angle)//24418(<), 24624(<) (F)
{
	angle = (angle << 2) - item->pos.z_rot;
	
	if(angle < ANGLE(-3))
		angle = ANGLE(-3);
	else if (angle > ANGLE(3))
		angle = ANGLE(3);

	if (abs(item->pos.z_rot) - ANGLE(15) > ANGLE(15))
	{
		angle >>= 1;
	}
	
	item->pos.z_rot += angle; // todo in orig code (mips) z_rot is lhu'd into v0 as unsigned, here i skipped that part, maybe it'll break
}

short CreatureTurn(struct ITEM_INFO* item, short maximum_turn)
{
	S_Warn("[CreatureTurn] - Unimplemented!\n");
	return 0;
}

int CreatureAnimation(short item_number, short angle, short tilt)
{
	S_Warn("[CreatureAnimation] - Unimplemented!\n");
	return 0;
}

void CreatureDie(short item_number, int explode)// (F)
{
	struct ITEM_INFO* item = &items[item_number];
	item->hit_points = 0xC000;
	item->collidable = FALSE;
	if (explode)
	{
		if (objects[item->object_number].HitEffect == 1)
			ExplodingDeath2(item_number, -1, 258);
		else
			ExplodingDeath2(item_number, -1, 256);

		KillItem(item_number);
	}
	else
	{
		RemoveActiveItem(item_number);
	}

	DisableBaddieAI(item_number);
	item->flags |= IFLAG_KILLED | IFLAG_INVISIBLE;
	DropBaddyPickups(item);

	if (item->object_number == SCIENTIST && item->ai_bits == 20)
	{
		item = find_a_fucking_item(ROLLINGBALL);
		if (item)
		{
			if (!(item->flags & IFLAG_INVISIBLE))
			{
				item->flags |= IFLAG_ACTIVATION_MASK;
				AddActiveItem(item - items);
			}
		}
	}
}

int BadFloor(long x, long y, long z, long box_height, long next_height, int room_number, struct lot_info* LOT)
{
	S_Warn("[BadFloor] - Unimplemented!\n");
	return 0;
}

int CreatureCreature(short item_number)
{
	S_Warn("[CreatureCreature] - Unimplemented!\n");
	return 0;
}

enum target_type CalculateTarget(struct PHD_VECTOR* target, struct ITEM_INFO* item, struct lot_info* LOT)
{
	S_Warn("[CalculateTarget] - Unimplemented!\n");
	return NO_TARGET;
}

void CreatureMood(struct ITEM_INFO* item, struct AI_info* info, int violent)
{
	S_Warn("[CreatureMood] - Unimplemented!\n");
}

void GetCreatureMood(struct ITEM_INFO* item, struct AI_info* info, int violent)
{
	S_Warn("[GetCreatureMood] - Unimplemented!\n");
}

int ValidBox(struct ITEM_INFO* item, short zone_number, short box_number)//222A4(<), ? (F)
{
	struct box_info* box;
	struct creature_info* creature;
	short* zone;

	creature = (struct creature_info*)item->data;
	zone = ground_zone[creature->LOT.zone][flip_status];

	if (creature->LOT.fly == 0 && zone[box_number] != zone_number)
	{
		return 0;
	}

	box = &boxes[box_number];

	if (box->overlap_index & creature->LOT.block_mask)
	{
		return 0;
	}

	if ((item->pos.z_pos > (box->left * 1024)) || ((box->right  * 1024) > item->pos.z_pos) ||
		(item->pos.x_pos > (box->top  * 1024)) || ((box->bottom * 1024) > item->pos.x_pos))
	{
		return 1;
	}

	return 0;
}

int EscapeBox(struct ITEM_INFO* item, struct ITEM_INFO* enemy, short box_number)//221C4(<), ?
{
	struct box_info *box; // $a0
	long x; // $a3
	long z; // $a2


	return 0;
}

void TargetBox(struct lot_info* LOT, short box_number)//220F4(<), ? (F)
{
	struct box_info* box;

	box_number &= 0x7FF;
	box = &boxes[box_number];

	LOT->required_box = box_number;

	LOT->target.z = (((((box->right - box->left) - 1) * GetRandomControl()) / 32) + (box->left * 1024)) + 512;
	LOT->target.x = (((((box->bottom - box->top) - 1) * GetRandomControl()) / 32) + (box->top * 1024)) + 512;

	if (LOT->fly == 0)
	{
		LOT->target.y = box->height;
	}
	else
	{
		LOT->target.y = box->height - 1152;
	}

	return;
}

int UpdateLOT(struct lot_info* LOT, int expansion)//22034(<), ? (F)
{
	struct box_node* expand;

	if (LOT->required_box != 0x7FF && LOT->required_box != LOT->target_box)
	{
		LOT->target_box = LOT->required_box;
		expand = &LOT->node[LOT->required_box];
		
		if (expand->next_expansion == 0x7FF && LOT->tail != LOT->required_box)
		{
			expand->next_expansion = LOT->head;

			if (LOT->head == LOT->node[LOT->required_box].next_expansion)
			{
				LOT->tail = LOT->target_box;
			}//0x220B8

			LOT->head = LOT->target_box;

		}//0x220C4

		expand->search_number = ++LOT->search_number;
		expand->exit_box = 0x7FF;
	}//220DC

	return SearchLOT(LOT, expansion);
}

int SearchLOT(struct lot_info* LOT, int expansion)
{
	S_Warn("[SearchLOT] - Unimplemented!\n");
	return 0;
}

void CreatureAIInfo(struct ITEM_INFO* item, struct AI_info* info)
{
	S_Warn("[CreatureAIInfo] - Unimplemented!\n");
}

int CreatureActive(short item_number)//218B0(<), ? (F)
{
	struct ITEM_INFO* item = &items[item_number];

	if (item->flags & IFLAG_KILLED)
	{
		if (item->status != ITEM_INVISIBLE)
		{
			return 1;
		}

		if (EnableBaddieAI(item_number, 0) != 0)
		{
			item->status = ITEM_DEACTIVATED;
			return 1;
		}
	}

	return 0;
}

void InitialiseCreature(short item_number)//21800(<), ? (F)
{
	struct ITEM_INFO* item = &items[item_number];

	item->hit_status = 1;
	item->data = NULL;
	item->draw_room = (((item->pos.z_pos - room[item->room_number].z) / 1024) & 0xFF) | (((item->pos.x_pos - room[item->room_number].mesh->x) / 4) & 0xFF00);
	item->TOSSPAD = item->pos.y_rot & 0xE000;
	item->item_flags[2] = item->room_number | (item->pos.y_pos - room->minfloor);

	return;
}

int StalkBox(struct ITEM_INFO* item, struct ITEM_INFO* enemy, short box_number)
{
	S_Warn("[StalkBox] - Unimplemented!\n");
	return 0;
}

