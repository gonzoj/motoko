/*
 * copyright (c) 2010 gonzoj
 *
 * please check the CREDITS file for further information.
 *
 * this program is free software: you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program.  if not, see <http://www.gnu.org/licenses/>.
 */

#define _PLUGIN

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <util/compat.h>

#include <module.h>

#include <moduleman.h>
#include <d2gs.h>
#include <packet.h>
#include <util/net.h>
#include <util/list.h>
#include <util/string.h>
#include <settings.h>
#include <gui.h>

#include <data/monster_fields.h>
#include <data/monster_names.h>
#include <data/super_uniques.h>
#include <data/skills.h>

#include <util/config.h>
#include <util/system.h>

/* ---------------------------- */

typedef void (*cleanup_handler)(void *);

/* SETTINGS */

static struct setting module_settings[] = (struct setting []) {
	SETTING("RunPindleskin", FALSE, BOOLEAN),
	SETTING("RunEldritch", FALSE, BOOLEAN),
	SETTING("RunShenk", FALSE, BOOLEAN),
	SETTING("UseMerc", FALSE, BOOLEAN),
	SETTING("CastDelay", 0, INTEGER),
	SETTING("MaxSequencesPerMonster", 0, INTEGER),
	SETTING("ShopAfterRuns", 1, INTEGER),
	SETTING("RepairAfterRuns", 1, INTEGER)
};

static struct list module_settings_list = LIST(module_settings, struct setting, 8);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

/* NPCs / OBJECTS */

typedef struct {
	word x;
	word y;
} point_t;

typedef struct {
	dword id;
	point_t location;
} object_t;

typedef struct {
	object_t object;
	word index;
	byte hp;
	bool super_unique;
	word super_unique_index;
	unsigned long long int mods;
} npc_t;

#define N_NPC_MOD 43

static const char *s_npc_mod[] = {
	"none", "rndname", "hpmultiply", "light", "leveladd", "strong", "fast", "curse",
	"resist", "fire", "poisondead", "durieldead", "bloodraven", "rage", "spcdamage", "partydead",
	"champion", "lightning", "cold", "hireable", "scarab", "killself", "questcomplete", "poisonhit",
	"thief", "manahit", "teleport", "spectralhit", "stoneskin", "multishot", "aura", "goboom",
	"fireskipe_explode", "suicideminion_explode", "ai_after_death", "shatter_on_death", "ghostly", "fanatic", "possessed", "berserk",
	"worms_on_death", "always_run_ai", "lightningdeath"
};

#define SQUARE(x) ((x) * (x))

#define DISTANCE(a, b) ((int) sqrt((double) (SQUARE((a).x - (b).x) + SQUARE((a).y - (b).y))))

#define MAX_WALKING_DISTANCE 35

#define object_clear(o) memset(&(o), 0, sizeof(object_t))

#define NPC_HP(hp) ((byte) ((((double) (((byte) hp > 128) ? (hp ^ 0xFF) : hp)) / 128) * 100))

#define NPC_MOD(npc, mod) (npc->mods & mod)

static struct list npcs;

static struct list npcs_l;

pthread_mutex_t npcs_m;

static object_t bot;

static object_t merc;

static object_t tp;

static object_t wp;

static object_t rp;

static int scrolls;

static dword scroll_id;

static byte act;

static unsigned long long int skip_mods = 0;

pthread_cond_t npc_interact_cv;

pthread_mutex_t npc_interact_mutex;

pthread_cond_t townportal_interact_cv;

pthread_mutex_t townportal_interact_mutex;

pthread_mutex_t act_notify_m;

pthread_cond_t act_notify_cv;

/* ACTIONS */

typedef void(*function_t)(object_t *, dword);

typedef struct {
	function_t function;
	dword arg;
} action_t;

static struct list precast_sequence = LIST(NULL, action_t, 0);

static struct list attack_sequence = LIST(NULL, action_t, 0);

static word cur_rskill;
static word cur_lskill;

pthread_mutex_t switch_slot_m;

pthread_cond_t switch_slot_cv;

/* STATISTICS */

static time_t run_start;

static int runs;

static int runtime;

static int merc_rez;

/* ---------------------------- */

void cast_self(object_t *obj, dword arg) {
	d2gs_send(0x0c, "%w %w", obj->location.x, obj->location.y);

	msleep(module_setting("CastDelay")->i_var);
}

void cast_right(object_t *obj, dword arg) {
	d2gs_send(0x0d, "01 00 00 00 %d", obj->id);

	msleep(module_setting("CastDelay")->i_var);
}

void cast_left(object_t *obj, dword arg) {
	d2gs_send(0x06, "01 00 00 00 %d", obj->id);

	msleep(module_setting("CastDelay")->i_var);
}

void swap_right(object_t *obj, dword arg) {
	if (cur_rskill == arg) {
		return;
	}

	d2gs_send(0x3c, "%w 00 00 ff ff ff ff", arg & 0xFFFF);

	msleep(300);

	cur_rskill = arg;
}

void swap_left(object_t *obj, dword arg) {
	if (cur_lskill == arg) {
		return;
	}

	d2gs_send(0x3c, "%w 00 80 ff ff ff ff", arg & 0xFFFF);

	msleep(300);

	cur_lskill = arg;
}

void switch_slot(object_t *obj, dword arg) {
	msleep(300);

	pthread_mutex_lock(&switch_slot_m);
	pthread_cleanup_push((cleanup_handler) pthread_mutex_unlock, (void *) &switch_slot_m);

	d2gs_send(0x60, "");

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 2;

	pthread_cond_timedwait(&switch_slot_cv, &switch_slot_m, &ts);

	pthread_cleanup_pop(1);

	msleep(200);
}

int skill_lookup(char *skill) {
	if (string_is_numeric(skill)) {
		int d;
		sscanf(skill, "%i", &d);

		return d;
	} else {
		int i;
		for (i = 0; i < n_skills; i++) {
			if (string_compare((char *) skills[i], skill, FALSE)) {
				return i;
			}
		}

		return -1;
	}
}

void assemble_sequence(struct list *sequence, char *description) {
	int lskill = -1, rskill = -1;

	string_to_lower_case(description);

	char *tok = strtok(description, "/");
	while (tok) {
		if (!strcmp(tok, "switch")) {
			action_t a = { switch_slot, 0 };
			list_add(sequence, &a);
		} else {
			char *separator = strchr(tok, ':');
			if (separator) {
				*separator = '\0';
				char *side = tok;
				char *s_skill = ++separator;
				int skill = skill_lookup(s_skill);

				if (skill >= 0) {

					if (!strcmp(side, "left") && skill >= 0) {
						if (skill != lskill) {
							action_t a = { swap_left, skill };
							list_add(sequence, &a);
							lskill = skill;
						}
	
						action_t a = { cast_left, 0 };
						list_add(sequence, &a);
					} else if (!strcmp(side, "right") && skill >= 0) {
						if (skill != rskill) {
							action_t a = { swap_right, skill };
							list_add(sequence, &a);
							rskill = skill;
						}

						action_t a = { cast_right, 0 };
						list_add(sequence, &a);
					} else if (!strcmp(side, "self") && skill >= 0) {
						if (skill != rskill) {
							action_t a = { swap_right, skill };
							list_add(sequence, &a);
							rskill = skill;
						}
					
						action_t a = { cast_self, 0 };
						list_add(sequence, &a);
					}
				}
			}
		}

		tok = strtok(NULL, "/");
	}
}

int bsr(int x) {
	int y;

	asm("bsr %1, %0" :"=r" (y) :"m" (x));

	return y;
}

npc_t * npc_new(d2gs_packet_t *packet, npc_t *new) {
	// incomplete

	new->object.id = net_get_data(packet->data, 0, dword);
	new->index = net_get_data(packet->data, 4, word);
	new->object.location.x = net_get_data(packet->data, 6, word);
	new->object.location.y = net_get_data(packet->data, 8, word);
	new->hp = net_get_data(packet->data, 10, byte);
	new->mods = 0;
	
	int i = 12 * 8;

	// section 1: animation mode
	i += 4;

	// section 2: monster comp info
	if (net_extract_bits(packet->data, i, 1)) {
		i += 1;

		int j;
		for (j = 0; j < 16; j++) {
			int comp = monster_fields[new->index][j];
			//if (comp > 0) {
			if (comp > 2 && comp < 10) {
				i += (bsr(comp - 1) + 1);
			} else {
				i += 1;
			}
		}
	} else {
		i += 1;
	}

	// section 3: monster mod info
	if (net_extract_bits(packet->data, i, 1)) {
		i += 1;

		i += 2;

		new->super_unique = net_extract_bits(packet->data, i, 1);
		i += 1;

		i += 2;

		if (new->super_unique) {
			new->super_unique_index = net_extract_bits(packet->data, i, 16);
			i += 16;
		} else {
			new->super_unique_index = -1;
		}

		int j;
		for (j = 0; j < 10; j++) {
			byte mod = net_extract_bits(packet->data, i, 8);

			i += 8;

			if (!mod) break;

			new->mods |= (1 << mod);
		}
	} else {
		i += 1;

		new->super_unique = FALSE;
		new->super_unique_index = -1;
	}

	return new;
}

char * npc_default_lookup_name(dword index) {
	return index < n_monster_names ? (char *) monster_names[index] : "";
}

char * npc_superunique_lookup_name(dword index) {
	return index < n_super_uniques ? (char *) super_uniques[index] : "";
}

#define npc_lookup_name(npc) (((npc)->super_unique && (npc)->super_unique_index < n_super_uniques) ? npc_superunique_lookup_name((npc)->super_unique_index) : npc_default_lookup_name((npc)->index))

int npc_lookup_mod(char *mod) {
	int i;
	
	for (i = 0; i < N_NPC_MOD; i++) {
		if (string_compare(mod, (char *) s_npc_mod[i], FALSE)) {
			return i;
		}
	}

	return -1;
}

char * npc_mod_string(long long int mod, char *s_mod, size_t len) {
	memset(s_mod, 0, len);

	int i;

	for (i = 0; i < N_NPC_MOD; i++) {
		if (mod & (1 << i)) {
			if (strlen(s_mod) && strlen(s_npc_mod[i])) {
				strncat(s_mod, " ", len - strlen(s_mod) - 1);
			}
			strncat(s_mod, s_npc_mod[i], len - strlen(s_mod) - 1);
		}
	}

	return s_mod;
}

char * npc_dump(npc_t *npc, char *s_npc, size_t len) {
	snprintf(s_npc, len - 1, "%s (%08X) %i%% %i/%i", npc_lookup_name(npc), npc->object.id, NPC_HP(npc->hp), npc->object.location.x, npc->object.location.y);
	if (npc->mods) {
		strncat(s_npc, "(", len - strlen(s_npc) - 1);
		char s_mod[512];
		npc_mod_string(npc->mods, s_mod, 512);
		strncat(s_npc, s_mod, len - strlen(s_npc) - 1);
		strncat(s_npc, ")", len - strlen(s_npc) - 1);
	}

	return s_npc;
}

int npc_compare_id(npc_t *a, npc_t *b) {
	return a->object.id == b->object.id;
}

bool npc_compare_name(npc_t *n, char *s_n) {
	return string_compare(npc_lookup_name(n), s_n, FALSE);
}

void npc_refresh() {
	pthread_mutex_lock(&npcs_m);

	list_clear(&npcs);	

	list_clone(&npcs_l, &npcs);

	pthread_mutex_unlock(&npcs_m);
}

bool npc_interact(npc_t *npc) {
	int s;

	pthread_mutex_lock(&npc_interact_mutex);
	pthread_cleanup_push((cleanup_handler) pthread_mutex_unlock, (void *) &npc_interact_mutex);

	d2gs_send(0x59, "01 00 00 00 %d %d %d", npc->object.id, bot.location.x, bot.location.y);

	unsigned d = DISTANCE(bot.location, npc->object.location);
	int diff;

	for (diff = d * 120; diff > 0; diff -= 200) {
		d2gs_send(0x04, "01 00 00 00 %d", npc->object.id);

		msleep((diff < 200) ? diff : 200);
	}

	d2gs_send(0x13, "01 00 00 00 %d", npc->object.id);

	msleep(200);

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 4;

	if ((s = pthread_cond_timedwait(&npc_interact_cv, &npc_interact_mutex, &ts))) {
		plugin_error("pes", "failed to interact with %s\n", npc_lookup_name(npc));
	} else {
		d2gs_send(0x2f, "01 00 00 00 %d", npc->object.id);

		msleep(200);
	}

	pthread_cleanup_pop(1);

	return s ? FALSE : TRUE;
}

bool npc_shop(char *s_npc) {
	bool s;

	//pthread_mutex_lock(&npcs_m);
	//pthread_cleanup_push((cleanup_handler) pthread_mutex_unlock, (void *) &npcs_m);
	
	npc_refresh();

	npc_t *npc = list_find(&npcs, (comparator_t) npc_compare_name, s_npc);
	if (npc) {
		if (npc_interact(npc)) {
			plugin_print("pes", "healing at %s\n", npc_lookup_name(npc));

			if ((module_setting("ShopAfterRuns")->i_var && !(runs % module_setting("ShopAfterRuns")->i_var)) || !scrolls) {
				plugin_print("pes", "shopping at %s\n", npc_lookup_name(npc));

				int j = scrolls;

				d2gs_send(0x38, "01 00 00 00 %d 00 00 00 00", npc->object.id);

				msleep(2000);

				int i;
				for (i = 0; i < 20 - j && scroll_id; i++) {
					d2gs_send(0x32, "%d %d 00 00 00 00 64 00 00 00", npc->object.id, scroll_id);

					msleep(200);
				}

				if (j < 20) plugin_print("pes", "restocked %i townportal(s)\n", 20 - j);
			}

			s =  TRUE;
		} else {
			plugin_print("pes", "failed to heal/shop at %s\n", npc_lookup_name(npc));
	
			s = FALSE;
		}

		d2gs_send(0x30, "01 00 00 00 %d", npc->object.id);
	} else {
		plugin_debug("pes", "couldn't find %s\n", s_npc);

		s = FALSE;
	}

	//pthread_cleanup_pop(1);

	return s;
}

bool npc_repair(char *s_npc) {
	bool s;

	npc_refresh();

	npc_t *npc = list_find(&npcs, (comparator_t) npc_compare_name, s_npc);
	if (npc) {
		if (npc_interact(npc)) {
			plugin_print("pes", "repairing equipment at %s\n", npc_lookup_name(npc));

			d2gs_send(0x35, "%d 00 00 00 00 00 00 00 00 00 00 00 80", npc->object.id);

			msleep(200);

			s = TRUE;
		} else {
			plugin_print("pes", "failed to repair at %s\n", npc_lookup_name(npc));

			s = FALSE;
		}

		d2gs_send(0x30, "01 00 00 00 %d", npc->object.id);
	} else {
		plugin_debug("pes", "couldn't find %s\n", s_npc);

		s = FALSE;
	}

	return s;
}

bool npc_merc(char *s_npc) {
	bool s;

	//pthread_mutex_lock(&npcs_m);
	//pthread_cleanup_push((cleanup_handler) pthread_mutex_unlock, (void *) &npcs_m);
	
	npc_refresh();

	npc_t *npc = list_find(&npcs, (comparator_t) npc_compare_name, s_npc);
	if (npc) {
		if (npc_interact(npc)) {
			d2gs_send(0x38, "03 00 00 00 %d 00 00 00 00", npc->object.id);

			msleep(300);

			d2gs_send(0x62, "%d", npc->object.id);

			msleep(300);

			d2gs_send(0x38, "03 00 00 00 %d 00 00 00 00", npc->object.id);

			msleep(300);

			plugin_print("pes", "resurrected merc at %s\n", npc_lookup_name(npc));

			s = TRUE;;
		} else {
			plugin_print("pes", "failed to resurrect merc at %s\n", npc_lookup_name(npc));

			s = FALSE;
		}

		d2gs_send(0x30, "01 00 00 00 %d", npc->object.id);
	} else {
		plugin_debug("pes", "couldn't find %s\n", s_npc);

		s = FALSE;
	}

	//pthread_cleanup_pop(1);

	return s;
}

void npc_attack(npc_t *npc) {
	int i;

	plugin_print("pes", "attacking %s\n", npc_lookup_name(npc));

	for (i = 0; npc->hp > 0; i++) {

		if (module_setting("MaxSequencesPerMonster")->i_var) {
			if (module_setting("MaxSequencesPerMonster")->i_var <= i) {
				break;
			}
		}

		struct iterator it = list_iterator(&attack_sequence);
		action_t *a;

		while ((a = iterator_next(&it))) {
			a->function(&(npc->object), a->arg);
		}

		plugin_print("pes", "%s's hp: %i%%\n", npc_lookup_name(npc), NPC_HP(npc->hp));
	}
}

int d2gs_assign_npc(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	npc_t new;
	npc_new(packet, &new);

	pthread_mutex_lock(&npcs_m);

	if (!list_find(&npcs_l, (comparator_t) npc_compare_id, &new)) {
		list_add(&npcs_l, &new);
	}

	pthread_mutex_unlock(&npcs_m);

	char s_new[512];
	npc_dump(&new, s_new, 512);
	plugin_debug("pes", "detected npc: %s\n", s_new);

	return FORWARD_PACKET;
}

int d2gs_update_npc(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	pthread_mutex_lock(&npcs_m);

	npc_t n, *npc = NULL;
	switch(packet->id) {

		case 0x0c: {
			n.object.id = net_get_data(packet->data, 1, dword);

			npc = list_find(&npcs_l, (comparator_t) npc_compare_id, &n);
			if (npc) {
				npc->hp = net_get_data(packet->data, 7, byte);

				if (!npc->hp) {
					list_remove(&npcs_l, npc);
				}
			}

			npc = list_find(&npcs, (comparator_t) npc_compare_id, &n);
			if (npc) {
				npc->hp = net_get_data(packet->data, 7, byte);
			}
		}
		break;

		case 0x69: {
			n.object.id = net_get_data(packet->data, 0, dword);

			npc = list_find(&npcs_l, (comparator_t) npc_compare_id, &n); 
			if (npc) {
				npc->object.location.x = net_get_data(packet->data, 5, word);
				npc->object.location.y = net_get_data(packet->data, 7, word);
				byte state = net_get_data(packet->data, 4, byte);
				if (state == 0x08 || state == 0x09) {
					npc->hp = 0;
				} else {
					npc->hp = net_get_data(packet->data, 9, byte);
				}

				if (!npc->hp) {
					list_remove(&npcs_l, npc);
				}
			}

			npc = list_find(&npcs, (comparator_t) npc_compare_id, &n); 
			if (npc) {
				npc->object.location.x = net_get_data(packet->data, 5, word);
				npc->object.location.y = net_get_data(packet->data, 7, word);
				byte state = net_get_data(packet->data, 4, byte);
				if (state == 0x08 || state == 0x09) {
					npc->hp = 0;
				} else {
					npc->hp = net_get_data(packet->data, 9, byte);
				}
			}
		}
		break;

		case 0x6d: {
			n.object.id = net_get_data(packet->data, 0, dword);

			npc = list_find(&npcs_l, (comparator_t) npc_compare_id, &n);
			if (npc) {
				npc->object.location.x = net_get_data(packet->data, 4, word);
				npc->object.location.y = net_get_data(packet->data, 6, word);
				npc->hp = net_get_data(packet->data, 8, byte);

				if (!npc->hp) {
					list_remove(&npcs_l, npc);
				}
			}

			npc = list_find(&npcs, (comparator_t) npc_compare_id, &n);
			if (npc) {
				npc->object.location.x = net_get_data(packet->data, 4, word);
				npc->object.location.y = net_get_data(packet->data, 6, word);
				npc->hp = net_get_data(packet->data, 8, byte);
			}
		}
		break;

	}

	if (npc) {
		char s_npc[512];
		npc_dump(npc, s_npc, 512);
		plugin_debug("pes", "npc update: %s\n", s_npc);
	}

	pthread_mutex_unlock(&npcs_m);

	return FORWARD_PACKET;
}

int d2gs_item_action(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	byte action = net_extract_bits(packet->data, 0, 8);
	dword id = net_extract_bits(packet->data, 24, 32);

	if (action == 0x0b) {
		char code[4] = { 0 };
		*(dword *)code = net_extract_bits(packet->data, 116, 24);

		if (!strcmp((char *) &code, "tsc")) {
			scroll_id = id;
		}
	}

	return FORWARD_PACKET;
}

int d2gs_char_location_update(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch (packet->id) {

		case 0x15: { // received packet
			bot.location.x = net_get_data(packet->data, 5, word);
			bot.location.y = net_get_data(packet->data, 7, word);
		}
		break;

		case 0x95: { // received packet
			bot.location.x = net_extract_bits(packet->data, 45, 15);
			bot.location.y = net_extract_bits(packet->data, 61, 15);
		}
		break;

		case 0x01:
		case 0x03: { // sent packets
			bot.location.x = net_get_data(packet->data, 0, word);
			bot.location.y = net_get_data(packet->data, 2, word);
		}
		break;

		case 0x0c: { // sent packet
			if (cur_rskill == 0x36) {
				bot.location.x = net_get_data(packet->data, 0, word);
				bot.location.y = net_get_data(packet->data, 2, word);
			}
		}
		break;

	}

	plugin_debug("pes", "bot update (%02X): %i/%i\n", packet->id, bot.location.x, bot.location.y);

	return FORWARD_PACKET;
}

int process_incoming_packet(void *p) {
	d2gs_packet_t *packet = (d2gs_packet_t *) p;

	switch (packet->id) {

	case 0x03: {
		pthread_mutex_lock(&act_notify_m);

		act = net_get_data(packet->data, 0, byte) + 1;

		pthread_cond_signal(&act_notify_cv);
		pthread_mutex_unlock(&act_notify_m);
		break;
	}

	case 0x0c: {
		d2gs_update_npc(packet);
		break;
	}

	case 0x11: {
		if (net_get_data(packet->data, 1, dword) == merc.id) {
			merc.id = 0;
		}
		break;
	}

	case 0x15: {
		d2gs_char_location_update(packet);
		break;
	}

	case 0x22: {
		if (net_get_data(packet->data, 6, word) == 0xdc) {
			scrolls = net_get_data(packet->data, 8, byte);
		}
		break;
	}

	case 0x27: {
		pthread_mutex_lock(&npc_interact_mutex);
		pthread_cond_signal(&npc_interact_cv);
		pthread_mutex_unlock(&npc_interact_mutex);
		break;
	}

	case 0x51: {
		if (net_get_data(packet->data, 0, byte) == 0x02 && net_get_data(packet->data, 5, word) == 0x3c) {
			rp.id = net_get_data(packet->data, 1, dword);
			rp.location.x = net_get_data(packet->data, 7, word);
			rp.location.y = net_get_data(packet->data, 9, word);

			plugin_debug("oes", "detected red portal at %i/%i\n", rp.location.x, rp.location.y);
		}
		if (net_get_data(packet->data, 0, byte) == 0x02 && net_get_data(packet->data, 5, word) == 0x1ad) {
			wp.id = net_get_data(packet->data, 1, dword);
			wp.location.x = net_get_data(packet->data, 7, word);
			wp.location.y = net_get_data(packet->data, 9, word);

			plugin_debug("pes", "detected waypoint at %i/%i\n", wp.location.x, wp.location.y);
		}
		break;
	}

	case 0x59: {
		if (!bot.id) {
			bot.id = net_get_data(packet->data, 0, dword);
			bot.location.x = net_get_data(packet->data, 21, word);
			bot.location.y = net_get_data(packet->data, 23, word);

			plugin_debug("pes", "detected bot character at %i/%i\n", bot.location.x, bot.location.y);
		}
		break;
	}

	case 0x5a: { // auto-accept party
		if (net_get_data(packet->data, 0, byte) == 0x07) {
			if (net_get_data(packet->data, 1, byte) == 0x02) {
				dword id = net_get_data(packet->data, 2, dword);

				if (net_get_data(packet->data, 6, byte) == 0x05) {
					d2gs_send(0x5e, "08 %d", id);
				}
			}
		}
		break;
	}

	case 0x69: {
		d2gs_update_npc(packet);
		break;
	}

	case 0x6d: {
		d2gs_update_npc(packet);
		break;
	}

	case 0x81: {
		if (net_get_data(packet->data, 3, dword) == bot.id) {
			merc.id = net_get_data(packet->data, 7, dword);
		}
		break;
	}

	case 0x82: {
		if (net_get_data(packet->data, 0, dword) == bot.id) {
			tp.id = net_get_data(packet->data, 20, dword);

			pthread_mutex_lock(&townportal_interact_mutex);
			pthread_cond_signal(&townportal_interact_cv);
			pthread_mutex_unlock(&townportal_interact_mutex);
		}
		break;
	}

	case 0x95: {
		d2gs_char_location_update(packet);
		break;
	}

	case 0x97: {
		pthread_mutex_lock(&switch_slot_m);
		pthread_cond_signal(&switch_slot_cv);
		pthread_mutex_unlock(&switch_slot_m);
		break;
	}

	case 0x9c: {
		d2gs_item_action(packet);
		break;
	}

	case 0xac: {
		d2gs_assign_npc(packet);
		break;
	}

	}

	return FORWARD_PACKET;
}

bool _moveto(int x, int y) {
	//pthread_mutex_lock(&game_exit_m);
	//pthread_cleanup_push((cleanup_handler) pthread_mutex_unlock, (void *) &game_exit_m);

	/*if (exited) {
		goto end;
	}*/

	point_t p = { x, y };

	int d = DISTANCE(bot.location, p);

	if (d > MAX_WALKING_DISTANCE) {
		plugin_print("pes", "got stuck trying to move to %i/%i (%i)\n", p.x, p.y, d);

		return FALSE;
	}

	plugin_print("pes", "moving to %i/%i\n", (word) p.x, (word) p.y);

	/*
	 * sleeping can lead to a problem:
	 * bot's at 10000/10000, opens portal, chickens, moves to 5000/5000 -> long sleep
	 * C -> S 0x03 | C-> S 0x69 | msleep()
	 */

	int t = d * 80;

	//int t_s = t / 1000;
	//int t_ns = (t - (t_s * 1000)) * 1000 * 1000;
	
	//struct timespec ts;
	//clock_gettime(CLOCK_REALTIME, &ts);
	//ts.tv_sec += t_s;
	//ts.tv_nsec += t_ns;

	d2gs_send(0x03, "%w %w", (word) p.x, (word) p.y);

	//pthread_cond_timedwait(&game_exit_cv, &game_exit_m, &ts);
	
	plugin_debug("pes", "sleeping for %ims\n", t);
	
	msleep(t > 3000 ? 3000 : t);
	
	//end:
	//pthread_cleanup_pop(1);
	
	return TRUE;
}

#define moveto(x, y) if (!_moveto(x, y)) goto stuck

void teleport(int x, int y) {
	plugin_print("pes", "teleporting to %i/%i\n", x, y);

	swap_right(NULL, 0x36);

	d2gs_send(0x0c, "%w %w", x, y);
	
	msleep(module_setting("CastDelay")->i_var);
}


void attack(char *s_npc) {
	npc_refresh();

	struct iterator it = list_iterator(&npcs);
	npc_t *npc;

	bool found = FALSE;

	while ((npc = iterator_next(&it))) {
		if (string_compare(s_npc, npc_lookup_name(npc), FALSE)) {
			if (DISTANCE(bot.location, npc->object.location) > 50) {
				plugin_debug("pes", "%s out of range (%i, %i/%i <> %i/%i)\n", npc_lookup_name(npc), DISTANCE(bot.location, npc->object.location), bot.location.x, bot.location.y, npc->object.location.x, npc->object.location.y);

				found = TRUE;

				npc_refresh();

				continue;
			}

			if (NPC_MOD(npc, skip_mods)) {
				char s_mod[512];
				npc_mod_string(NPC_MOD(npc, skip_mods), s_mod, 512);
				plugin_print("pes", "skip %s (mods: %s)\n", npc_lookup_name(npc), s_mod);

				found = TRUE;

				npc_refresh();

				continue;
			}
			
			npc_attack(npc);

			found = TRUE;
		}

		npc_refresh();
	}

	if (!found) plugin_debug("pes", "couldn't find %s\n", s_npc);
}

void precast() {
	plugin_print("pes", "casting buff sequence...\n");

	struct iterator it = list_iterator(&precast_sequence);
	action_t *a;

	while ((a = iterator_next(&it))) {
		a->function(&bot, a->arg);
	}
}

bool townportal() {
	int s;

	d2gs_send(0x3c, "%d ff ff ff ff", 0xdc);

	msleep(400);

	pthread_mutex_lock(&townportal_interact_mutex);
	pthread_cleanup_push((cleanup_handler) pthread_mutex_unlock, (void *) &townportal_interact_mutex);

	d2gs_send(0x0c, "%w %w", bot.location.x, bot.location.y);

	msleep(500);

	plugin_print("pes", "opened townportal\n");

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 2;

	if ((s = pthread_cond_timedwait(&townportal_interact_cv, &townportal_interact_mutex, &ts))) {
		plugin_error("pes", "failed to take townportal\n");
	}
	else {
		d2gs_send(0x13, "02 00 00 00 %d", tp.id);

		msleep(400);

		if (merc.id) {
			d2gs_send(0x4b, "01 00 00 00 %d", merc.id);

			msleep(100);
		}

		plugin_print("pes", "took townportal\n");
	}

	pthread_cleanup_pop(1);

	return s ? FALSE : TRUE;
}

void waypoint(dword area) {
	d2gs_send(0x13, "02 00 00 00 %d", wp.id);

	msleep(300);

	d2gs_send(0x49, "%d %d", wp.id, area);

	if (merc.id) {
		msleep(300);

		d2gs_send(0x4b, "01 00 00 00 %d", merc.id);
	}

	msleep(500);
	
	plugin_print("pes", "took waypoint %02X\n", area);
}

void redportal() {
	d2gs_send(0x13, "02 00 00 00 %d", rp.id);
	
	if (merc.id) {
		msleep(300);

		d2gs_send(0x4b, "01 00 00 00 %d", merc.id);
	}

	msleep(500);

	plugin_print("pes", "took red portal\n");
}

void leave() {
	d2gs_send(0x69, "");

	plugin_print("pes", "leaving game\n");
}

_export void * module_thread(void *unused) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 2;

	pthread_mutex_lock(&act_notify_m);

	if (!act && pthread_cond_timedwait(&act_notify_cv, &act_notify_m, &ts)) {
		pthread_mutex_unlock(&act_notify_m);

		plugin_error("pes", "haven't received act number in time\n");
		leave();

		pthread_exit(NULL);
	}

	pthread_mutex_unlock(&act_notify_m);

	if (act != 5) {
		plugin_error("pes", "can't start from act %i\n", act);
		leave();

		pthread_exit(NULL);
	}

	time(&run_start);
	plugin_print("pes", "starting P/E/S run %i\n", ++runs);

	// move to larzuk and repair equipment
	if (module_setting("RepairAfterRuns")->i_var && !(runs % module_setting("RepairAfterRuns")->i_var)) {
		moveto(5106, 5045);
		moveto(5124, 5040);
		moveto(5144, 5044);

		npc_repair("Larzuk");

		// move back to spawn
		moveto(5144, 5044);
		moveto(5124, 5040);
		moveto(5106, 5045);
		moveto(5098, 5018);
	}

	// move to malah and heal / shop
	moveto(5089, 5029);
	moveto(5074, 5027);

	npc_shop("Malah");

	moveto(5074, 5027);
	moveto(5072, 5040);
	moveto(5080, 5062);

	// resurrect merc
	if (module_setting("UseMerc")->b_var && !merc.id) {
		merc_rez++;

		moveto(5082, 5079);
		moveto(5061, 5076);

		npc_merc("qual-kehk");

		moveto(5082, 5079);
	} else {
		moveto(5082, 5079);
	}

	if (module_setting("RunPindleskin")->b_var) {
		// move to red portal
		moveto(5081, 5088);
		moveto(5085, 5112);
		moveto(5108, 5123);
		moveto(5119, 5123);

		msleep(700);

		// take portal
		redportal();

		precast();

		plugin_print("pes", "running pindleskin now...\n");

		// teleport to pindleskin - let's test farcast
		teleport(10062, 13287);
		teleport(10061, 13262);
		teleport(10058, 13235);

		msleep(500); // dirty fix; detecting pindle is delayed so we have to give it some time

		// attack pindleskin
		//execute_module_schedule(MODULE_D2GS);
		attack("pindleskin");
		attack("defiled warrior");

		msleep(1200);

		// pickit
		//internal_send(0x9c, "%s 00", "pickit");
		execute_module_schedule(MODULE_D2GS);
		
		if (module_setting("RunEldritch")->b_var || module_setting("RunShenk")->b_var) {
			if (!townportal()) {
				leave();

				pthread_exit(NULL);
			}

			// move to waypoint
			moveto(5104, 5043);
			moveto(5114, 5069);
		}

	} else if (module_setting("RunEldritch")->b_var || module_setting("RunShenk")->b_var) {
		// move to waypoint from qual-khek
		moveto(5102, 5087);
		moveto(5120, 5084);
		moveto(5114, 5069);
	}

	if (module_setting("RunEldritch")->b_var ||  module_setting("RunShenk")->b_var) {
		// take waypoint 1
		waypoint(0x6f);

		precast();
	}

	if (module_setting("RunEldritch")->b_var) {
		plugin_print("pes", "running eldritch now...\n");

		// teleport to eldritch
		teleport(3763, 5094);
		teleport(3741, 5074);

		// attack eldritch
		//execute_module_schedule(MODULE_D2GS);
		attack("eldritch the rectifier");
		attack("enslaved");

		msleep(1200);

		// pickit
		//internal_send(0x9c, "%s 00", "pickit");
		execute_module_schedule(MODULE_D2GS);
	
		if (module_setting("RunShenk")->b_var) {
			// teleport to shenk
			teleport(3741, 5074);
			teleport(3763, 5085);
		}
	}

	if (module_setting("RunShenk")->b_var) {
		plugin_print("pes", "running shenk now...\n");

		teleport(3796, 5100);
		teleport(3822, 5107);
		teleport(3847, 5117);
		teleport(3869, 5116);
		teleport(3887, 5112);

		// attack shenk
		//execute_module_schedule(MODULE_D2GS);
		attack("shenk the overseer");
		attack("enslaved");

		msleep(1200);

		// pickit
		//internal_send(0x9c, "%s 00", "pickit");
		execute_module_schedule(MODULE_D2GS);
	}

	stuck:

	// leave game
	leave();

	// clear bloody foothills
	// monsters:
	// enslaved, death mauler, 

	pthread_exit(NULL);
}

_export void module_cleanup() {
	object_clear(bot);
	object_clear(merc);
	object_clear(tp);
	object_clear(wp);
	object_clear(rp);
	scroll_id = 0;
	act = 0;
	list_clear(&npcs);
	list_clear(&npcs_l);
	
	if (!runs) return;
	
	time_t cur;
	int duration = (int) difftime(time(&cur), run_start);
	runtime += duration;

	char * s_duration, * s_average;
	s_duration = string_format_time(duration);
	s_average = string_format_time(runs > 0 ? runtime / runs : 0);

	plugin_print("pes", "run took %s (average: %s)\n", s_duration, s_average);

	free(s_duration);
	free(s_average);
}

_export const char * module_get_title() {
	return "pes";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "a simple bot for pindle, eldritch and shenk";
}

_export int module_get_license() {
	return LICENSE_GPL_V3;
}

_export module_type_t module_get_type() {
	return (module_type_t) { MODULE_D2GS, MODULE_ACTIVE };
}

_export bool module_load_config(struct setting_section *s) {
	int i;
	for (i = 0; i < s->entries; i++) {
		if (!strcmp(s->settings[i].name, "PrecastSequence")) {
			char *c_setting = strdup(s->settings[i].s_var);
			assemble_sequence(&precast_sequence, c_setting);
			free(c_setting);
		} else if (!strcmp(s->settings[i].name, "AttackSequence")) {
			char *c_setting = strdup(s->settings[i].s_var);
			assemble_sequence(&attack_sequence, c_setting);
			free(c_setting);
		} else if (!strcmp(s->settings[i].name, "SkipMods")) {
			char *c_setting = strdup(s->settings[i].s_var);
			char *tok = strtok(c_setting, ":");
			while (tok) {
				int mod = npc_lookup_mod(tok);
				if (mod >= 0) {
					skip_mods |= (1 << mod);
				}

				tok = strtok(NULL, ":");
			}
			free(c_setting);
		} else {
			struct setting *set = module_setting(s->settings[i].name);
			if (set) {
				if (s->settings[i].type == INTEGER && set->type == INTEGER) {
					set->i_var = s->settings[i].i_var;
				} else if (s->settings[i].type == STRING) {
					if (set->type == BOOLEAN) {
						set->b_var = !strcmp(string_to_lower_case(s->settings[i].s_var), "yes");
					} else if (set->type == STRING){
						set->s_var = strdup(s->settings[i].s_var);
					} else if (set->type == INTEGER) {
						sscanf(s->settings[i].s_var, "%li", &set->i_var);
					}
				}
			}
		}
	}

	return TRUE;
}

_export bool module_init() {
	register_packet_handler(D2GS_RECEIVED, 0x03, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x11, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x59, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x51, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0xac, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x97, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x9c, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x27, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x81, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x82, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x22, process_incoming_packet);
	if (setting("Debug")->b_var)
		register_packet_handler(D2GS_RECEIVED, 0x5a, process_incoming_packet); // auto-accept party
	register_packet_handler(D2GS_RECEIVED, 0x15, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x95, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x0c, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x69, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x6d, process_incoming_packet);

	register_packet_handler(D2GS_SENT, 0x01, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x03, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x0c, d2gs_char_location_update);

	pthread_mutex_init(&npcs_m, NULL);

	pthread_cond_init(&npc_interact_cv, NULL);
	pthread_mutex_init(&npc_interact_mutex, NULL);

	pthread_cond_init(&townportal_interact_cv, NULL);
	pthread_mutex_init(&townportal_interact_mutex, NULL);

	pthread_cond_init(&act_notify_cv, NULL);
	pthread_mutex_init(&act_notify_m, NULL);

	pthread_mutex_init(&switch_slot_m, NULL);
	pthread_cond_init(&switch_slot_cv, NULL);

	npcs = list_new(npc_t);
	npcs_l = list_new(npc_t);

	cur_rskill = -1;
	cur_lskill = -1;

	act = 0;

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(D2GS_RECEIVED, 0x03, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x11, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x59, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x51, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0xac, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x97, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x9c, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x27, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x81, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x82, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x22, process_incoming_packet);
	if (setting("Debug")->b_var)
		unregister_packet_handler(D2GS_RECEIVED, 0x5a, process_incoming_packet); // auto-accept party
	unregister_packet_handler(D2GS_RECEIVED, 0x15, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x95, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x0c, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x69, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x6d, process_incoming_packet);

	unregister_packet_handler(D2GS_SENT, 0x01, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x03, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x0c, d2gs_char_location_update);

	pthread_mutex_destroy(&npcs_m);

	pthread_cond_destroy(&npc_interact_cv);
	pthread_mutex_destroy(&npc_interact_mutex);

	pthread_cond_destroy(&townportal_interact_cv);
	pthread_mutex_destroy(&townportal_interact_mutex);

	pthread_cond_destroy(&act_notify_cv);
	pthread_mutex_destroy(&act_notify_m);

	pthread_mutex_destroy(&switch_slot_m);
	pthread_cond_destroy(&switch_slot_cv);

	list_clear(&precast_sequence);
	list_clear(&attack_sequence);

	char *s_average, *s_runtime;
	s_average = string_format_time(runs > 0 ? runtime / runs : 0);
	s_runtime = string_format_time(runtime);

	ui_add_statistics_plugin("pes", "runs: %i\n", runs);
	ui_add_statistics_plugin("pes", "average run duration: %s\n", s_average);
	ui_add_statistics_plugin("pes", "total time spent running: %s\n", s_runtime);
	ui_add_statistics_plugin("pes", "merc resurrections: %i\n", merc_rez);

	free(s_average);
	free(s_runtime);

	return TRUE;
}
