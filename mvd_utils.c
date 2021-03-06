/*
Copyright (C) 2001-2002 jogihoogi

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

$Id: mvd_utils.c,v 1.57 2007-10-11 17:56:47 johnnycz Exp $
*/

// core module of the group of MVD tools: mvd_utils, mvd_xmlstats, mvd_autotrack

#include "quakedef.h"
#include "parser.h"
#include "localtime.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "teamplay.h"
#include "utils.h"
#include "mvd_utils_common.h"

mvd_gt_info_t mvd_gt_info[mvd_gt_types] = {
	{gt_1on1,"duel"},
	{gt_2on2,"2on2"},
	{gt_3on3,"3on3"},
	{gt_4on4,"4on4"},
	{gt_unknown,"unknown"},
};

mvd_cg_info_s mvd_cg_info;

mvd_wp_info_t mvd_wp_info[mvd_info_types] = {
	{AXE_INFO,"axe",IT_AXE},
	{SG_INFO,"sg",IT_SHOTGUN},
	{SSG_INFO,"ssg",IT_SUPER_SHOTGUN},
	{NG_INFO,"ng",IT_NAILGUN},
	{SNG_INFO,"sng",IT_SUPER_NAILGUN},
	{GL_INFO,"gl",IT_GRENADE_LAUNCHER},
	{RL_INFO,"rl",IT_ROCKET_LAUNCHER},
	{LG_INFO,"lg",IT_LIGHTNING},
	{RING_INFO,"ring",IT_INVISIBILITY},
	{QUAD_INFO,"quad",IT_QUAD},
	{PENT_INFO,"pent",IT_INVULNERABILITY},
	{GA_INFO,"ga",IT_ARMOR1},
	{YA_INFO,"ya",IT_ARMOR2},
	{RA_INFO,"ra",IT_ARMOR3},
	{MH_INFO,"mh",IT_SUPERHEALTH},
};

typedef struct quad_cams_s {
	vec3_t	org;
	vec3_t	angles;
} quad_cams_t;

typedef struct cam_id_s {
		quad_cams_t cam;
		char *tag;
} cam_id_t;

quad_cams_t quad_cams[3];
quad_cams_t pent_cams[3];

cam_id_t cam_id[7];

// NEW VERSION

typedef struct runs_s {
	double starttime;
	double endtime;
} runs_t;

typedef struct kill_s {
	int		type;	//0 - kill, 1 - selfkill
	double	time;
	vec3_t	location;
	int		lwf ;
} kill_t;

typedef struct death_s	{
	double time;
	vec3_t location;
	int id;
} death_t;

typedef struct spawn_s {
	double time;
	vec3_t location;
} spawn_t;

typedef struct mvd_new_info_cg_s {
	double game_starttime;
} mvd_new_info_cg_t; // mvd_new_info_cg;

typedef struct mvd_player_s {
	player_state_t	*p_state;
	player_info_t	*p_info;
} mvd_player_t;

typedef struct mvd_gameinfo_s {
	double starttime;
	char mapname[1024];
	char team1[1024];
	char team2[1024];
	char hostname[1024];
	int gametype;
	int timelimit;
	int pcount;
	int deathmatch;
} mvd_gameinfo_t;
/*
typedef struct mvd_info_s {
	mvd_gameinfo_t gameinfo;
	mvd_player_t player[MAX_CLIENTS];

} mvd_info_t;
*/

//extern mt_matchstate_t matchstate;
//extern matchinfo_t matchinfo;
extern	centity_t		cl_entities[CL_MAX_EDICTS];
extern	entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
double lasttime1 ,lasttime2;
double lasttime = 0;
double gamestart_time ;

double quad_time =0;
double pent_time =0;
int quad_is_active= 0;
int pent_is_active= 0;
int pent_mentioned = 0;
int quad_mentioned = 0;
int powerup_cam_active = 0;
int cam_1,cam_2,cam_3,cam_4;
static qbool was_standby = true;


extern cvar_t tp_name_quad, tp_name_pent, tp_name_ring, tp_name_separator, tp_name_backpack, tp_name_suit;
extern cvar_t tp_name_axe, tp_name_sg, tp_name_ssg, tp_name_ng, tp_name_sng, tp_name_gl, tp_name_rl, tp_name_lg,tp_name_ga,tp_name_ya,tp_name_ra,tp_name_mh;
extern cvar_t tp_name_none, tp_weapon_order;
char mvd_info_best_weapon[20];
extern int loc_loaded;

extern qbool TP_LoadLocFile (char *path, qbool quiet);
extern char *TP_LocationName(vec3_t location);
extern char *Weapon_NumToString(int num);

//matchinfo_t *MT_GetMatchInfo(void);

//char Mention_Win_Buf[80][1024];

mvd_new_info_t mvd_new_info[MAX_CLIENTS];

int FindBestNick(char *s, int use);

int mvd_demo_track_run = 0;

// mvd_info cvars
cvar_t			mvd_info		= {"mvd_info", "0"};
cvar_t			mvd_info_show_header	= {"mvd_info_show_header", "0"};
cvar_t			mvd_info_setup		= {"mvd_info_setup", "%p%n \x10%l\x11 %h/%a %w"}; // FIXME: non-ascii chars
cvar_t			mvd_info_x		= {"mvd_info_x", "0"};
cvar_t			mvd_info_y		= {"mvd_info_y", "0"};

// mvd_stats cvars
cvar_t			mvd_status		= {"mvd_status","0"};
cvar_t			mvd_status_x		= {"mvd_status_x","0"};
cvar_t			mvd_status_y		= {"mvd_status_y","0"};

cvar_t mvd_powerup_cam = {"mvd_powerup_cam","0"};

cvar_t mvd_pc_quad_1 = {"mvd_pc_quad_1","1010 -300 150 13 135"};
cvar_t mvd_pc_quad_2 = {"mvd_pc_quad_2","350 -20 157 34 360"};
cvar_t mvd_pc_quad_3 = {"mvd_pc_quad_3","595 360 130 17 360"};

cvar_t mvd_pc_pent_1 = {"mvd_pc_pent_1","1010 -300 150 13 135"};
cvar_t mvd_pc_pent_2 = {"mvd_pc_pent_2","1010 -300 150 13 135"};
cvar_t mvd_pc_pent_3 = {"mvd_pc_pent_3","1010 -300 150 13 135"};

cvar_t mvd_pc_view_1 = {"mvd_pc_view_1",""};
cvar_t mvd_pc_view_2 = {"mvd_pc_view_2",""};
cvar_t mvd_pc_view_3 = {"mvd_pc_view_3",""};
cvar_t mvd_pc_view_4 = {"mvd_pc_view_4",""};

cvar_t mvd_moreinfo = {"mvd_moreinfo","0"};

typedef struct bp_var_s{
	int id;
	int val;
} bp_var_t;

bp_var_t bp_var[MAX_CLIENTS];

char *Make_Red (char *s,int i){
	static char buf[1024];
	char *p,*ret;
	buf[0]= 0;
	ret=buf;
	for (p=s;*p;p++){
		if (!strspn(p,"1234567890.") || !(i))
		*ret++=*p | 128;
		else
		*ret++=*p;
	}
	*ret=0;
    return buf;
}

void MVD_Init_Info_f (void) {
	int i;
	int z;

	for (z=0,i=0;i<MAX_CLIENTS;i++){
		if (!cl.players[i].name[0] || cl.players[i].spectator == 1)
				continue;
		mvd_new_info[z].id = i;
		mvd_new_info[z++].p_info = &cl.players[i];
	}

	strlcpy(mvd_cg_info.mapname,TP_MapName(),sizeof(mvd_cg_info.mapname));
	mvd_cg_info.timelimit=cl.timelimit;

	strlcpy(mvd_cg_info.team1, (z ? mvd_new_info[0].p_info->team : ""),sizeof(mvd_cg_info.team1));
	for (i = 0; i < z; i++) {
		if(strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1)){
			strlcpy(mvd_cg_info.team2,mvd_new_info[i].p_info->team,sizeof(mvd_cg_info.team2));
			break;
		}
	}

	if (z==2)
		mvd_cg_info.gametype=0;
	else if (z==4)
		mvd_cg_info.gametype=1;
	else if (z==6)
		mvd_cg_info.gametype=2;
	else if (z==8)
		mvd_cg_info.gametype=3;
	else
		mvd_cg_info.gametype=4;

	strlcpy(mvd_cg_info.hostname,Info_ValueForKey(cl.serverinfo,"hostname"),sizeof(mvd_cg_info.hostname));
	mvd_cg_info.deathmatch=atoi(Info_ValueForKey(cl.serverinfo,"deathmatch"));

	mvd_cg_info.pcount = z;

	for (i = 0; i < mvd_cg_info.pcount; i++)
		mvd_new_info[i].p_state = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[mvd_new_info[i].id];
}

// this steps in action if the user has created a demo playlist and has specified
// which player should be prefered in the demos (so that he doesn't have to switch
// to that player at the start of each demo manually)
void MVD_Demo_Track (void){
	extern char track_name[16];
    extern cvar_t demo_playlist_track_name;
	int track_player ;

	#ifdef DEBUG
	printf("MVD_Demo_Track Started\n");
	#endif


	if(strlen(track_name)){
		track_player=FindBestNick(track_name,1);
		if (track_player != -1 )
			Cbuf_AddText (va("track \"%s\"\n",cl.players[track_player].name));
	}else if (strlen(demo_playlist_track_name.string)){
		track_player=FindBestNick(demo_playlist_track_name.string,1);
		if (track_player != -1 )
			Cbuf_AddText (va("track \"%s\"\n",cl.players[track_player].name));
	}

	mvd_demo_track_run = 1;
	#ifdef DEBUG
	printf("MVD_Demo_Track Stopped\n");
	#endif
}


int MVD_BestWeapon (int i) {
	int x;
	char *t[] = {tp_weapon_order.string, "78654321", NULL}, **s;
	for (s = t; *s; s++) {
		for (x = 0 ; x < strlen(*s) ; x++) {
			switch ((*s)[x]) {
				case '1': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_AXE) return IT_AXE; break;
				case '2': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SHOTGUN) return IT_SHOTGUN; break;
				case '3': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) return IT_SUPER_SHOTGUN; break;
				case '4': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_NAILGUN) return IT_NAILGUN; break;
				case '5': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_NAILGUN) return IT_SUPER_NAILGUN; break;
				case '6': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) return IT_GRENADE_LAUNCHER; break;
				case '7': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) return IT_ROCKET_LAUNCHER; break;
				case '8': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_LIGHTNING) return IT_LIGHTNING; break;
			}
		}
	}
	return 0;
}

char *MVD_BestWeapon_strings (int i) {
	switch (MVD_BestWeapon(i)) {
		case IT_AXE: return tp_name_axe.string;
		case IT_SHOTGUN: return tp_name_sg.string;
		case IT_SUPER_SHOTGUN: return tp_name_ssg.string;
		case IT_NAILGUN: return tp_name_ng.string;
		case IT_SUPER_NAILGUN: return tp_name_sng.string;
		case IT_GRENADE_LAUNCHER: return tp_name_gl.string;
		case IT_ROCKET_LAUNCHER: return tp_name_rl.string;
		case IT_LIGHTNING: return tp_name_lg.string;
		default: return tp_name_none.string;
	}
}

char *MVD_Weapon_strings (int i) {
	switch (i) {
		case IT_AXE: return tp_name_axe.string;
		case IT_SHOTGUN: return tp_name_sg.string;
		case IT_SUPER_SHOTGUN: return tp_name_ssg.string;
		case IT_NAILGUN: return tp_name_ng.string;
		case IT_SUPER_NAILGUN: return tp_name_sng.string;
		case IT_GRENADE_LAUNCHER: return tp_name_gl.string;
		case IT_ROCKET_LAUNCHER: return tp_name_rl.string;
		case IT_LIGHTNING: return tp_name_lg.string;
		default: return tp_name_none.string;
	}
}

int MVD_Weapon_LWF (int i) {
	switch (i) {
		case IT_AXE: return 0;
		case IT_SHOTGUN: return 1;
		case IT_SUPER_SHOTGUN: return 2;
		case IT_NAILGUN: return 3;
		case IT_SUPER_NAILGUN: return 4;
		case IT_GRENADE_LAUNCHER: return 5;
		case IT_ROCKET_LAUNCHER: return 6;
		case IT_LIGHTNING: return 7;
		default: return 666;
	}
}

char *MVD_BestAmmo (int i) {

	switch (MVD_BestWeapon(i)) {
		case IT_SHOTGUN: case IT_SUPER_SHOTGUN:
			return va("%i",mvd_new_info[i].p_info->stats[STAT_SHELLS]);

		case IT_NAILGUN: case IT_SUPER_NAILGUN:
			return va("%i",mvd_new_info[i].p_info->stats[STAT_NAILS]);

		case IT_GRENADE_LAUNCHER: case IT_ROCKET_LAUNCHER:
			return va("%i",mvd_new_info[i].p_info->stats[STAT_ROCKETS]);

		case IT_LIGHTNING:
			return va("%i",mvd_new_info[i].p_info->stats[STAT_CELLS]);

		default: return "0";
	}
}


void MVD_Info (void){
	char str[1024];
	char mvd_info_final_string[1024], mvd_info_powerups[20], mvd_info_header_string[1024];
	char *mapname;
	int x,y,z,i;



	#ifdef DEBUG
	printf("MVD_Info Started\n");
	#endif

	z=1;

	if (loc_loaded == 0){
		mapname = TP_MapName();
		TP_LoadLocFile (mapname, true);
		loc_loaded = 1;
	}

	if (!mvd_info.value)
		return;

	if (!cls.mvdplayback)
		return;

	x = ELEMENT_X_COORD(mvd_info);
	y = ELEMENT_Y_COORD(mvd_info);

	if (mvd_info_show_header.value){
		strlcpy(mvd_info_header_string,mvd_info_setup.string,sizeof(mvd_info_header_string));
		Replace_In_String(mvd_info_header_string,sizeof(mvd_info_header_string),'%',\
			10,\
			"a","Armor",\
			"f","Frags",\
			"h","Health",\
			"l","Location",\
			"n","Nick",\
			"P","Ping",\
			"p","Powerup",\
			"v","Value",\
			"w","Cur.Weap.",\
			"W","Best Weap.");
		strlcpy(mvd_info_header_string,Make_Red(mvd_info_header_string,0),sizeof(mvd_info_header_string));
		Draw_String (x, y+((z++)*8), mvd_info_header_string);
	}

	for ( i=0 ; i<mvd_cg_info.pcount ; i++ ){

	mvd_info_powerups[0]=0;
	if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_QUAD)
		//strlcpy(mvd_info_powerups, tp_name_quad.string, sizeof(mvd_info_powerups));

	if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		//if (mvd_info_powerups[0])
		//	strlcat(mvd_info_powerups, tp_name_separator.string, sizeof(mvd_info_powerups));
		//strlcat(mvd_info_powerups, tp_name_pent.string, sizeof(mvd_info_powerups));
	}

	if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVISIBILITY) {
		//if (mvd_info_powerups[0])
		//	strlcat(mvd_info_powerups, tp_name_separator.string, sizeof(mvd_info_powerups));
		//strlcat(mvd_info_powerups, tp_name_ring.string, sizeof(mvd_info_powerups));
	}

	strlcpy(mvd_info_final_string,mvd_info_setup.string,sizeof(mvd_info_final_string));
    Replace_In_String(mvd_info_final_string,sizeof(mvd_info_final_string),'%',\
			10,\
			"w",va("%s:%i",Weapon_NumToString(mvd_new_info[i].p_info->stats[STAT_ACTIVEWEAPON]),mvd_new_info[i].p_info->stats[STAT_AMMO]),\
			"W",va("%s:%s",MVD_BestWeapon_strings(i),MVD_BestAmmo(i)),\
			"a",va("%i",mvd_new_info[i].p_info->stats[STAT_ARMOR]),\
			"f",va("%i",mvd_new_info[i].p_info->frags),\
			"h",va("%i",mvd_new_info[i].p_info->stats[STAT_HEALTH]),\
			"l",TP_LocationName(mvd_new_info[i].p_state->origin),\
			"n",mvd_new_info[i].p_info->name,\
			"P",va("%i",mvd_new_info[i].p_info->ping),\
			"p",mvd_info_powerups,\
			"v",va("%f",mvd_new_info[i].value));
	strlcpy(str, mvd_info_final_string,sizeof(str));
	Draw_String (x, y+((z++)*8), str);

	#ifdef DEBUG
	printf("MVD_Info Stopped\n");
	#endif
	}
}

void MVD_Status_Announcer_f (int i, int z){
	char *pn;
	vec3_t *pl;
	pn=mvd_new_info[i].p_info->name;
	pl=&mvd_new_info[i].p_state->origin;
	if (mvd_new_info[i].info.info[z].mention==1)
	{
		mvd_new_info[i].info.info[z].mention = 0;

		if (!mvd_moreinfo.integer)
			return;

		switch (z){
			case 2: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ssg.string,TP_LocationName(*pl));break;
			case 3: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ng.string,TP_LocationName(*pl));break;
			case 4: Com_Printf("%s Took %s @ %s\n",pn, tp_name_sng.string,TP_LocationName(*pl));break;
			case 5: Com_Printf("%s Took %s @ %s\n",pn, tp_name_gl.string,TP_LocationName(*pl));break;
			case 6: Com_Printf("%s Took %s @ %s\n",pn, tp_name_rl.string,TP_LocationName(*pl));break;
			case 7: Com_Printf("%s Took %s @ %s\n",pn, tp_name_lg.string,TP_LocationName(*pl));break;
			case 8: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ring.string,TP_LocationName(*pl));break;
			case 9: Com_Printf("%s Took %s @ %s\n",pn, tp_name_quad.string,TP_LocationName(*pl));break;
			case 10: Com_Printf("%s Took %s @ %s\n",pn, tp_name_pent.string,TP_LocationName(*pl));break;
			case 11: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ga.string,TP_LocationName(*pl));break;
			case 12: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ya.string,TP_LocationName(*pl));break;
			case 13: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ra.string,TP_LocationName(*pl));break;
			case 14: Com_Printf("%s Took %s @ %s\n",pn, tp_name_mh.string,TP_LocationName(*pl));break;
		}
	}
	else if (mvd_new_info[i].info.info[z].mention==-1)
	{
		mvd_new_info[i].info.info[z].mention = 0;

		if (!mvd_moreinfo.integer)
			return;

		switch (z) {
			case 5: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_gl.string,TP_LocationName(*pl));break;
			case 6: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_rl.string,TP_LocationName(*pl));break;
			case 7: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_lg.string,TP_LocationName(*pl));break;
			case 8: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_ring.string,TP_LocationName(*pl));break;
			case 9: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_quad.string,TP_LocationName(*pl));break;
			case 10:
				if (mvd_new_info[i].info.info[QUAD_INFO].starttime - cls.demotime < 30) {
					Com_Printf("%s Lost %s @ %s\n",pn, tp_name_pent.string,TP_LocationName(*pl));break;
				} else {
					Com_Printf("%s's %s ended\n",pn, tp_name_pent.string,TP_LocationName(*pl));break;
				}
			case 11: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_ga.string,TP_LocationName(*pl));break;
			case 12: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_ya.string,TP_LocationName(*pl));break;
			case 13: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_ra.string,TP_LocationName(*pl));break;
			case 14: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_mh.string,TP_LocationName(*pl));break;
		}
	}
}

void MVD_Status_WP_f (int i){
	int j,k;
	for (k=j=2;j<8;j++){
		if (!mvd_new_info[i].info.info[j].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & k){
			mvd_new_info[i].info.info[j].mention = 1;
			mvd_new_info[i].info.info[j].has = 1;
			mvd_new_info[i].info.info[j].count++;
		}
	k=k*2;
	}

}

void MVD_Stats_Cleanup_f (void){
	quad_is_active=0;
	pent_is_active=0;
	powerup_cam_active=0;
	cam_1=cam_2=cam_3=cam_4=0;
	was_standby = true;

	memset(&mvd_new_info, 0, sizeof(mvd_new_info_t));
	memset(&mvd_cg_info, 0, sizeof(mvd_cg_info_s));
}

void MVD_Set_Armor_Stats_f (int z,int i){
	switch(z){
		case GA_INFO:
			mvd_new_info[i].info.info[YA_INFO].has=0;
			mvd_new_info[i].info.info[RA_INFO].has=0;
			break;
		case YA_INFO:
			mvd_new_info[i].info.info[GA_INFO].has=0;
			mvd_new_info[i].info.info[RA_INFO].has=0;
			break;
		case RA_INFO:
			mvd_new_info[i].info.info[GA_INFO].has=0;
			mvd_new_info[i].info.info[YA_INFO].has=0;
			break;

	}
}

// calculates the average values of run statistics
void MVD_Stats_CalcAvgRuns(void)
{
	int i;
	static double lastupdate = 0;

	// no need to recalculate the values in every frame
	if (cls.realtime - lastupdate < 0.5) return;
	else lastupdate = cls.realtime;

	for (i = 0; i < MAX_CLIENTS; i++) {
		mvd_info_t *pi = &mvd_new_info[i].info;
//		int r = mvd_new_info[i].info.run;
		int tf, ttf, tt;
		int j;
		tf = ttf = tt = 0;

		for (j = 0; j < pi->run; j++) {
			tf += pi->runs[j].frags;
			ttf += pi->runs[j].teamfrags;
			tt += pi->runs[j].time;
		}

		if (pi->run) {
			pi->run_stats.all.avg_frags = tf / (double) pi->run;
			pi->run_stats.all.avg_teamfrags = ttf / (double) pi->run;
			pi->run_stats.all.avg_time = tt / (double) pi->run;
		}
	}
}

int MVD_Stats_Gather_f (void){
	int death_stats = 0;
	int x,i,z,killdiff;

	if(cl.countdown == true){
		return 0;
		quad_time = pent_time = 0;
	}
	if(cl.standby == true)
		return 0;

	for ( i=0; i<mvd_cg_info.pcount ; i++ ){
		if (quad_time == pent_time && quad_time == 0 && !mvd_new_info[i].info.firstrun){
			powerup_cam_active = 3;
			quad_time=pent_time=cls.demotime;
		}

		if (mvd_new_info[i].info.firstrun == 0){
			mvd_new_info[i].info.das.alivetimestart = cls.demotime;
			gamestart_time = cls.demotime;
			mvd_new_info[i].info.firstrun = 1;
			mvd_new_info[i].info.lfw = -1;
		}
		// death alive stats
		if (mvd_new_info[i].p_info->stats[STAT_HEALTH]>0 && mvd_new_info[i].info.das.isdead == 1){
			mvd_new_info[i].info.das.isdead = 0;
			mvd_new_info[i].info.das.alivetimestart = cls.demotime;
			mvd_new_info[i].info.lfw = -1;
		}

		mvd_new_info[i].info.das.alivetime = cls.demotime - mvd_new_info[i].info.das.alivetimestart;
		if (mvd_new_info[i].p_info->stats[STAT_HEALTH]<=0 && mvd_new_info[i].info.das.isdead != 1){
			mvd_new_info[i].info.das.isdead = 1;
			mvd_new_info[i].info.das.deathcount++;
			death_stats=1;
		}

		if (death_stats){
			death_stats=0;
			mvd_new_info[i].info.run++;


			for(x=0;x<13;x++){

				if (x == MVD_Weapon_LWF(mvd_new_info[i].info.lfw)){
						mvd_new_info[i].info.info[x].mention=-1;
						mvd_new_info[i].info.info[x].lost++;
				}

				if (x == QUAD_INFO && mvd_new_info[i].info.info[QUAD_INFO].has){
					if (mvd_new_info[i].info.info[x].starttime - cls.demotime < 30 )
						quad_is_active = 0;
						mvd_new_info[i].info.info[x].run++;
						mvd_new_info[i].info.info[x].lost++;
				}
				mvd_new_info[i].info.info[x].has=0;
			}
			mvd_new_info[i].info.lfw = -1;
		}
		if(!mvd_new_info[i].info.das.isdead){


		for (x=GA_INFO;x<=RA_INFO && mvd_cg_info.deathmatch!=4;x++){
			if(mvd_new_info[i].p_info->stats[STAT_ITEMS] & mvd_wp_info[x].it){
				if (!mvd_new_info[i].info.info[x].has){
					MVD_Set_Armor_Stats_f(x,i);
					mvd_new_info[i].info.info[x].count++;
					mvd_new_info[i].info.info[x].lost=mvd_new_info[i].p_info->stats[STAT_ARMOR];
					mvd_new_info[i].info.info[x].mention=1;
					mvd_new_info[i].info.info[x].has=1;
				}
				if (mvd_new_info[i].info.info[x].lost < mvd_new_info[i].p_info->stats[STAT_ARMOR]) {
					mvd_new_info[i].info.info[x].count++;
					mvd_new_info[i].info.info[x].mention = 1;
				}
				mvd_new_info[i].info.info[x].lost=mvd_new_info[i].p_info->stats[STAT_ARMOR];
			}
		}


		for (x=RING_INFO;x<=PENT_INFO && mvd_cg_info.deathmatch!=4;x++){
			if(!mvd_new_info[i].info.info[x].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & mvd_wp_info[x].it){
				mvd_new_info[i].info.info[x].mention = 1;
				mvd_new_info[i].info.info[x].has = 1;
				if (x==PENT_INFO && (powerup_cam_active == 3 || powerup_cam_active == 2)){
					pent_mentioned=0;
					pent_is_active=1;
					powerup_cam_active-=2;
				}
				if (x==QUAD_INFO && (powerup_cam_active == 3 || powerup_cam_active == 1)){
					quad_mentioned=0;
					quad_is_active=1;
					powerup_cam_active-=1;
				}
				mvd_new_info[i].info.info[x].starttime = cls.demotime;
				mvd_new_info[i].info.info[x].count++;
			}
			if (mvd_new_info[i].info.info[x].has && !(mvd_new_info[i].p_info->stats[STAT_ITEMS] & mvd_wp_info[x].it)){
				mvd_new_info[i].info.info[x].has = 0;
				if (x==QUAD_INFO && quad_is_active){
					quad_is_active=0;
				}
				if (x==PENT_INFO && pent_is_active){
					pent_is_active=0;
				}
				mvd_new_info[i].info.info[x].runs[mvd_new_info[i].info.info[x].run].starttime = mvd_new_info[i].info.info[x].starttime;
				mvd_new_info[i].info.info[x].runs[mvd_new_info[i].info.info[x].run].time = cls.demotime - mvd_new_info[i].info.info[x].starttime;
				mvd_new_info[i].info.info[x].run++;
			}
		}

		if (!mvd_new_info[i].info.info[MH_INFO].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPERHEALTH){
			mvd_new_info[i].info.info[MH_INFO].mention = 1;
			mvd_new_info[i].info.info[MH_INFO].has = 1;
			mvd_new_info[i].info.info[MH_INFO].count++;
		}
		if (mvd_new_info[i].info.info[MH_INFO].has && !(mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPERHEALTH))
			mvd_new_info[i].info.info[MH_INFO].has = 0;

		for (z=RING_INFO;z<=PENT_INFO;z++){
			if (mvd_new_info[i].info.info[z].has == 1){
				mvd_new_info[i].info.info[z].runs[mvd_new_info[i].info.info[z].run].starttime = mvd_new_info[i].info.info[z].starttime;
				mvd_new_info[i].info.info[z].runs[mvd_new_info[i].info.info[z].run].time = cls.demotime - mvd_new_info[i].info.info[z].starttime;
			}
		}

		if (mvd_new_info[i].info.lastfrags != mvd_new_info[i].p_info->frags ){
			if (mvd_new_info[i].info.lastfrags < mvd_new_info[i].p_info->frags){
				killdiff = mvd_new_info[i].p_info->frags - mvd_new_info[i].info.lastfrags;
				for (z=0;z<8;z++){
					if (z == MVD_Weapon_LWF(mvd_new_info[i].info.lfw))
							mvd_new_info[i].info.killstats.normal[z].kills+=killdiff;
				}
				if (mvd_new_info[i].info.lfw == -1)
					mvd_new_info[i].info.spawntelefrags+=killdiff;
				for(z=8;z<11;z++){
					if(mvd_new_info[i].info.info[z].has)
						mvd_new_info[i].info.info[z].runs[mvd_new_info[i].info.info[z].run].frags+=killdiff;
				}
				mvd_new_info[i].info.runs[mvd_new_info[i].info.run].frags++;
				}else if (mvd_new_info[i].info.lastfrags > mvd_new_info[i].p_info->frags){
				killdiff = mvd_new_info[i].info.lastfrags - mvd_new_info[i].p_info->frags ;
				for (z=AXE_INFO;z<=LG_INFO;z++){
					if (z == MVD_Weapon_LWF(mvd_new_info[i].info.lfw))
						mvd_new_info[i].info.killstats.normal[z].teamkills+=killdiff;
				}
				if (mvd_new_info[i].info.lfw == -1){
					mvd_new_info[i].info.teamspawntelefrags+=killdiff;

				}
				for(z=8;z<11;z++){
					if(mvd_new_info[i].info.info[z].has)
						mvd_new_info[i].info.info[z].runs[mvd_new_info[i].info.info[z].run].teamfrags+=killdiff;
				}
				mvd_new_info[i].info.runs[mvd_new_info[i].info.run].teamfrags++;
				}


			mvd_new_info[i].info.lastfrags = mvd_new_info[i].p_info->frags ;
		}

		mvd_new_info[i].info.runs[mvd_new_info[i].info.run].time=cls.demotime - mvd_new_info[i].info.das.alivetimestart;

		if (mvd_new_info[i].info.lfw == -1){
			if (mvd_new_info[i].info.lastfrags > mvd_new_info[i].p_info->frags ){
				mvd_new_info[i].info.teamspawntelefrags += mvd_new_info[i].p_info->frags - mvd_new_info[i].info.lastfrags ;
			}else if (mvd_new_info[i].info.lastfrags < mvd_new_info[i].p_info->frags ){
				mvd_new_info[i].info.spawntelefrags += mvd_new_info[i].p_info->frags -mvd_new_info[i].info.lastfrags  ;
			}
			mvd_new_info[i].info.lastfrags = mvd_new_info[i].p_info->frags;
		}

		if (mvd_new_info[i].p_state->weaponframe > 0)
				mvd_new_info[i].info.lfw=mvd_new_info[i].p_info->stats[STAT_ACTIVEWEAPON];
		if (mvd_cg_info.deathmatch!=4){
			MVD_Status_WP_f(i);
			for (z=SSG_INFO;z<=RA_INFO;z++)
				MVD_Status_Announcer_f(i,z);
			}
		}
		if ((((pent_time + 300) - cls.demotime) < 5) && !pent_is_active){
			if(!pent_mentioned){
				pent_mentioned = 1;
                // fixme
				// Com_Printf("pent in 5 secs\n");
			}
			if (powerup_cam_active ==1)
					powerup_cam_active = 3;
			else if (powerup_cam_active == 0)
					powerup_cam_active = 2;
		}
		if ((((quad_time + 60) - cls.demotime) < 5) && !quad_is_active){
			if(!quad_mentioned){
				quad_mentioned = 1;
                // fixme
				// Com_Printf("quad in 5 secs\n");
			}
			if (powerup_cam_active ==2)
				powerup_cam_active = 3;
			else if (powerup_cam_active == 0)
				powerup_cam_active = 1;
		}

	}

	return 1;

}

void MVD_Status (void){
	int x, y,p ;
	char str[1024];
	int i;
	int id = 0;
	int z = 0;
	double av_f =0;
	double av_t =0;
	double av_tk=0;


	if (!mvd_status.value)
		return;

	if (!cls.mvdplayback)
		return;

	for (i=0;i<mvd_cg_info.pcount;i++)
		if (mvd_new_info[i].id == spec_track)
			id = i;

	x = ELEMENT_X_COORD(mvd_status);
	y = ELEMENT_Y_COORD(mvd_status);
	if (mvd_new_info[id].p_info)
		strlcpy(str,mvd_new_info[id].p_info->name,sizeof(str));
	else
		str[0] = '\0';
	Draw_ColoredString (x, y+((z++)*8), str,1);
	strlcpy(str,"&cf40Took",sizeof(str));

	Draw_ColoredString (x, y+((z++)*8), str,1);

	strlcpy(str,va("RL: %i LG: %i GL: %i RA: %i YA: %i GA:%i",\
		mvd_new_info[id].info.info[RL_INFO].count,\
		mvd_new_info[id].info.info[LG_INFO].count,\
		mvd_new_info[id].info.info[GL_INFO].count,\
		mvd_new_info[id].info.info[RA_INFO].count,\
		mvd_new_info[id].info.info[YA_INFO].count,\
		mvd_new_info[id].info.info[RA_INFO].count),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);
    strlcpy(str,va("Ring: %i Quad: %i Pent: %i MH: %i",\
		mvd_new_info[id].info.info[RING_INFO].count,\
		mvd_new_info[id].info.info[QUAD_INFO].count,\
		mvd_new_info[id].info.info[PENT_INFO].count,\
		mvd_new_info[id].info.info[MH_INFO].count),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

//	Com_Printf("%f %f %f \n",lasttime,mvd_new_info[id].info.das.alivetimestart, mvd_new_info[id].info.das.alivetime);
	if (cls.demotime >+ lasttime + .1){
		lasttime=cls.demotime;
		lasttime1=mvd_new_info[id].info.das.alivetime;
	}

	strlcpy(str,va("Deaths: %i",mvd_new_info[id].info.das.deathcount),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,"Average Run:",sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,"Time      Frags TKS",sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	for (p=0;p<=mvd_new_info[id].info.run;p++){
		av_t += mvd_new_info[id].info.runs[p].time;
		av_f += mvd_new_info[id].info.runs[p].frags;
		av_tk += mvd_new_info[id].info.runs[p].teamfrags;
	}
	if (av_t>0){
	av_t = av_t / (mvd_new_info[id].info.run +1);
	av_f = av_f / (mvd_new_info[id].info.run +1);
	av_tk = av_tk / (mvd_new_info[id].info.run +1);
	}

	strlcpy(str,va("%9.3f %3.3f %3.3f",av_t,av_f,av_tk),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);


	strlcpy(str,"Last 3 Runs:",sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,"No. Time      Frags TKS",sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	p=mvd_new_info[id].info.run-3;
	if (p<0)
		p=0;
	//&& mvd_new_info[id].info.runs[p].time
	for(;p<=mvd_new_info[id].info.run ;p++){
		strlcpy(str,va("%3i %9.3f %5i %3i",p+1,mvd_new_info[id].info.runs[p].time,mvd_new_info[id].info.runs[p].frags,mvd_new_info[id].info.runs[p].teamfrags),sizeof(str));
		Draw_ColoredString (x, y+((z++)*8),str,1);
	}
	strlcpy(str,va("Last Fired Weapon: %s",MVD_Weapon_strings(mvd_new_info[id].info.lfw)),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,"&cf40Lost",sizeof(str));
	Draw_ColoredString (x, y+((z++)*8), str,1);

	strlcpy(str,va("RL: %i LG: %i GL: %i QUAD: %i",\
		mvd_new_info[id].info.info[RL_INFO].lost,\
		mvd_new_info[id].info.info[LG_INFO].lost,\
		mvd_new_info[id].info.info[GL_INFO].lost,\
		mvd_new_info[id].info.info[QUAD_INFO].lost),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,"&cf40Kills",sizeof(str));
	Draw_ColoredString (x, y+((z++)*8), str,1);

	strlcpy(str,va("RL: %i LG: %i GL: %i SNG: %i NG: %i SSG: %i SG: %i AXE: %i",\
		mvd_new_info[id].info.killstats.normal[RL_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[LG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[GL_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[SNG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[NG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[SSG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[SG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[AXE_INFO].kills),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,va("SPAWN: %i",\
		mvd_new_info[id].info.spawntelefrags),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,"&cf40Teamkills",sizeof(str));
	Draw_ColoredString (x, y+((z++)*8), str,1);

	strlcpy(str,va("RL: %i LG: %i GL: %i SNG: %i NG: %i SSG: %i SG: %i AXE: %i",\
		mvd_new_info[id].info.killstats.normal[RL_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[LG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[GL_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[SNG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[NG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[SSG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[SG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[AXE_INFO].teamkills),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);
	strlcpy(str,va("SPAWN: %i",\
		mvd_new_info[id].info.teamspawntelefrags),sizeof(str));
	strlcpy(str,Make_Red(str,1),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,"Last 3 Quad Runs:",sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strlcpy(str,"No. Time      Frags TKS",sizeof(str));
	strlcpy(str,Make_Red(str,0),sizeof(str));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	p=mvd_new_info[id].info.info[QUAD_INFO].run-3;
	if (p<0)
		p=0;
	for(;p<=mvd_new_info[id].info.info[QUAD_INFO].run && mvd_new_info[id].info.info[QUAD_INFO].runs[p].time ;p++){
		strlcpy(str,va("%3i %9.3f %5i %3i",p+1,mvd_new_info[id].info.info[QUAD_INFO].runs[p].time,mvd_new_info[id].info.info[QUAD_INFO].runs[p].frags,mvd_new_info[id].info.info[QUAD_INFO].runs[p].teamfrags),sizeof(str));
		Draw_ColoredString (x, y+((z++)*8),str,1);
	}
}

#ifdef _DEBUG
void MVD_Testor_f (void) {
	extern qbool Match_Running;
	Com_Printf("%i\n",Match_Running);
	Com_Printf("%s : %s \n",mvd_cg_info.team1,mvd_cg_info.team2);
}
#endif

qbool MVD_MatchStarted(void) {
	if (was_standby && !cl.standby) {
		was_standby = false;
		return true;
	}
	was_standby = cl.standby;
	return false;
}

void MVD_Mainhook_f (void){
	if (MVD_MatchStarted())
		MVD_Init_Info_f();

	MVD_Stats_Gather_f();
	MVD_Stats_CalcAvgRuns();
	MVD_AutoTrack_f ();
	if (cls.mvdplayback && mvd_demo_track_run == 0)
		MVD_Demo_Track ();
}

void MVD_PC_Get_Coords (void){
	char val[1024];
	//cvar_t *p;

	strlcpy (val, mvd_pc_quad_1.string, sizeof (val));
	cam_id[0].cam.org[0]=(float)atof(strtok(val, " "));
	cam_id[0].cam.org[1]=(float)atof(strtok(NULL, " "));
	cam_id[0].cam.org[2]=(float)atof(strtok(NULL, " "));
	cam_id[0].cam.angles[0]=(float)atof(strtok(NULL, " "));
	cam_id[0].cam.angles[1]=(float)atof(strtok(NULL, " "));
	cam_id[0].tag="q1";

	strlcpy (val,mvd_pc_quad_2.string, sizeof (val));
	cam_id[1].cam.org[0]=(float)atof(strtok(val, " "));
	cam_id[1].cam.org[1]=(float)atof(strtok(NULL, " "));
	cam_id[1].cam.org[2]=(float)atof(strtok(NULL, " "));
	cam_id[1].cam.angles[0]=(float)atof(strtok(NULL, " "));
	cam_id[1].cam.angles[1]=(float)atof(strtok(NULL, " "));
	cam_id[1].tag="q2";

	strlcpy (val,mvd_pc_quad_3.string, sizeof (val));
	cam_id[2].cam.org[0]	=(float)atof(strtok(val, " "));
	cam_id[2].cam.org[1]=(float)atof(strtok(NULL, " "));
	cam_id[2].cam.org[2]=(float)atof(strtok(NULL, " "));
	cam_id[2].cam.angles[0]=(float)atof(strtok(NULL, " "));
	cam_id[2].cam.angles[1]=(float)atof(strtok(NULL, " "));
	cam_id[2].tag="q3";

	strlcpy (val,mvd_pc_pent_1.string, sizeof (val));
	cam_id[3].cam.org[0]=(float)atof(strtok(val, " "));
	cam_id[3].cam.org[1]=(float)atof(strtok(NULL, " "));
	cam_id[3].cam.org[2]=(float)atof(strtok(NULL, " "));
	cam_id[3].cam.angles[0]=(float)atof(strtok(NULL, " "));
	cam_id[3].cam.angles[1]=(float)atof(strtok(NULL, " "));
	cam_id[3].tag="p1";

	strlcpy (val,mvd_pc_pent_2.string, sizeof (val));
	cam_id[4].cam.org[0]=(float)atof(strtok(val, " "));
	cam_id[4].cam.org[1]=(float)atof(strtok(NULL, " "));
	cam_id[4].cam.org[2]=(float)atof(strtok(NULL, " "));
	cam_id[4].cam.angles[0]=(float)atof(strtok(NULL, " "));
	cam_id[4].cam.angles[1]=(float)atof(strtok(NULL, " "));
	cam_id[4].tag="p2";

	strlcpy (val,mvd_pc_pent_3.string, sizeof (val));
	cam_id[5].cam.org[0]=(float)atof(strtok(val, " "));
	cam_id[5].cam.org[1]=(float)atof(strtok(NULL, " "));
	cam_id[5].cam.org[2]=(float)atof(strtok(NULL, " "));
	cam_id[5].cam.angles[0]=(float)atof(strtok(NULL, " "));
	cam_id[5].cam.angles[1]=(float)atof(strtok(NULL, " "));
	cam_id[5].tag="p3";
}


void MVD_Powerup_Cams_f (void){
	int i;
	int x=1;


	if (!mvd_powerup_cam.value || !powerup_cam_active){
		cam_1=cam_2=cam_3=cam_4=0;
		return;
	}

	MVD_PC_Get_Coords();

	if (CURRVIEW == 1 && strlen(mvd_pc_view_1.string)){
		cam_1=0;
		for (i=0,x=0;i<6;i++){
			if(i<=2 && powerup_cam_active == 2)
				continue;
			if(i>=3 && powerup_cam_active == 1)
				continue;
			if(!strcmp(mvd_pc_view_1.string,cam_id[i].tag)){
				VectorCopy(cam_id[i].cam.angles,r_refdef.viewangles);
				VectorCopy(cam_id[i].cam.org,r_refdef.vieworg);
				x=1;
				cam_1=1;
			}
		}
		/*
		if (!x){
				Cvar_SetValue(&mvd_pc_view_1,0);
				mvd_pc_view_1.string[0]='\0';
				Com_Printf("wrong tag for mvd_pc_view_1\n");
		}
		*/
	}
	if (CURRVIEW == 2 && strlen(mvd_pc_view_2.string)){
		cam_2=0;
		for (i=0;i<6;i++){
			if(i<=2 && powerup_cam_active == 2)
				continue;
			if(i>=3 && powerup_cam_active == 1)
				continue;
			if(!strcmp(mvd_pc_view_2.string,cam_id[i].tag)){
				VectorCopy(cam_id[i].cam.angles,r_refdef.viewangles);
				VectorCopy(cam_id[i].cam.org,r_refdef.vieworg);
				x=1;
				cam_2=1;
			}
		}
		if (!x){
				Cvar_SetValue(&mvd_pc_view_2,0);
				mvd_pc_view_2.string[0]='\0';
				Com_Printf("wrong tag for mvd_pc_view_2\n");
		}
	}

	if (CURRVIEW == 3 && strlen(mvd_pc_view_3.string)){
		cam_3=0;
		for (i=0;i<6;i++){
			if(i<=2 && powerup_cam_active == 2)
				continue;
			if(i>=3 && powerup_cam_active == 1)
				continue;
			if(!strcmp(mvd_pc_view_3.string,cam_id[i].tag)){
				VectorCopy(cam_id[i].cam.angles,r_refdef.viewangles);
				VectorCopy(cam_id[i].cam.org,r_refdef.vieworg);
				x=1;
				cam_3=1;
			}
		}
		if (!x){
				Cvar_SetValue(&mvd_pc_view_3,0);
				mvd_pc_view_3.string[0]='\0';
				Com_Printf("wrong tag for mvd_pc_view_3\n");
		}
	}

	if (CURRVIEW == 4 && strlen(mvd_pc_view_4.string)){
		cam_4=0;
		for (i=0;i<6;i++){
			if(i<=2 && powerup_cam_active == 2)
				continue;
			if(i>=3 && powerup_cam_active == 1)
				continue;
			if(!strcmp(mvd_pc_view_4.string,cam_id[i].tag)){
				VectorCopy(cam_id[i].cam.angles,r_refdef.viewangles);
				VectorCopy(cam_id[i].cam.org,r_refdef.vieworg);
				x=1;
				cam_4=1;
			}
		}
		if (!x){
				Cvar_SetValue(&mvd_pc_view_4,0);
				mvd_pc_view_4.string[0]='\0';
				Com_Printf("wrong tag for mvd_pc_view_4\n");
		}
	}
}

void MVD_Utils_Init (void) {
	MVD_AutoTrack_Init();
	MVD_XMLStats_Init();

	Cvar_SetCurrentGroup(CVAR_GROUP_MVD);
	Cvar_Register (&mvd_info);
	Cvar_Register (&mvd_info_show_header);
	Cvar_Register (&mvd_info_setup);
	Cvar_Register (&mvd_info_x);
	Cvar_Register (&mvd_info_y);

	Cvar_Register (&mvd_status);
	Cvar_Register (&mvd_status_x);
	Cvar_Register (&mvd_status_y);

	Cvar_Register (&mvd_powerup_cam);

	Cvar_Register (&mvd_pc_quad_1);
	Cvar_Register (&mvd_pc_quad_2);
	Cvar_Register (&mvd_pc_quad_3);

	Cvar_Register (&mvd_pc_pent_1);
	Cvar_Register (&mvd_pc_pent_2);
	Cvar_Register (&mvd_pc_pent_3);

	Cvar_Register (&mvd_pc_view_1);
	Cvar_Register (&mvd_pc_view_2);
	Cvar_Register (&mvd_pc_view_3);
	Cvar_Register (&mvd_pc_view_4);

	Cvar_Register (&mvd_moreinfo);

#ifdef _DEBUG
	Cmd_AddCommand ("mvd_test",MVD_Testor_f);
#endif

	Cmd_AddCommand ("mvd_utils_reinit", MVD_Init_Info_f);

	Cvar_ResetCurrentGroup();
}

void MVD_Screen (void){
	MVD_Info ();
	MVD_Status ();
}
