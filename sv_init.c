/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: sv_init.c,v 1.17 2007-10-07 04:59:47 disconn3ct Exp $
*/

#include "qwsvdef.h"
#include "crc.h"


serverPersistent_t	svs;	// persistent server info
server_t sv;				// local server

char localmodels[MAX_MODELS][5];			// inline model names for precache

char localinfo[MAX_LOCALINFO_STRING + 1];	// local game info

cvar_t	sv_loadentfiles = {"sv_loadentfiles", "0"};


/*
================
SV_ModelIndex
================
*/
int SV_ModelIndex (char *name)
{
	int i;
	
	if (!name || !name[0])
		return 0;

	for (i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;

	Host_Error ("SV_ModelIndex: model %s not precached", name);

	return 0; // shut up compiler
}

/*
================
SV_FlushSignon

Moves to the next signon buffer if needed
================
*/
void SV_FlushSignon (void)
{
	if (sv.signon.cursize < sv.signon.maxsize - 512)
		return;

	if (sv.num_signon_buffers == MAX_SIGNON_BUFFERS - 1)
		Host_Error ("sv.num_signon_buffers == MAX_SIGNON_BUFFERS-1");

	sv.signon_buffer_size[sv.num_signon_buffers - 1] = sv.signon.cursize;
	sv.signon.data = sv.signon_buffers[sv.num_signon_buffers];
	sv.num_signon_buffers++;
	sv.signon.cursize = 0;
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline (void)
{
	int i, entnum, max_edicts;	
	edict_t *svent;

	// because baselines for entnum >= 512 don't make sense
	// FIXME, translate baselines nums as well as packet entity nums?
	max_edicts = min (sv.num_edicts, 512);

	for (entnum = 0; entnum < max_edicts ; entnum++) {
		svent = EDICT_NUM(entnum);
		if (svent->free)
			continue;
		// create baselines for all player slots, and any other edict that has a visible model
		if (entnum > MAX_CLIENTS && !svent->v.modelindex)
			continue;

		// create entity baseline
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skinnum = svent->v.skin;
		if (entnum > 0 && entnum <= MAX_CLIENTS) {
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
		} else {
			svent->baseline.colormap = 0;
			svent->baseline.modelindex = SV_ModelIndex(PR_GetString(svent->v.model));
		}

		// flush the signon message out to a separate buffer if
		// nearly full
		SV_FlushSignon ();

		// add to the message
		MSG_WriteByte (&sv.signon,svc_spawnbaseline);		
		MSG_WriteShort (&sv.signon,entnum);

		MSG_WriteByte (&sv.signon, svent->baseline.modelindex);
		MSG_WriteByte (&sv.signon, svent->baseline.frame);
		MSG_WriteByte (&sv.signon, svent->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->baseline.skinnum);
		for (i = 0; i < 3; i++) {
			MSG_WriteCoord(&sv.signon, svent->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->baseline.angles[i]);
		}
	}
}

/*
================
SV_SaveSpawnparms

Grabs the current state of the progs serverinfo flags 
and each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	int i, j;

	if (!sv.state)
		return; // no progs loaded yet

	// serverflags is the only game related thing maintained
	svs.serverflags = pr_global_struct->serverflags;

	for (i = 0, sv_client = svs.clients; i < MAX_CLIENTS; i++, sv_client++) {
		if (sv_client->state != cs_spawned)
			continue;

		// needs to reconnect
		sv_client->state = cs_connected;

		// call the progs to get default spawn parms for the new client
		pr_global_struct->self = EDICT_TO_PROG(sv_client->edict);
		PR_ExecuteProgram (pr_global_struct->SetChangeParms);
		for (j = 0; j < NUM_SPAWN_PARMS; j++)
			sv_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
	}
}

unsigned SV_CheckModel (char *mdl)
{
	byte *buf;
	unsigned short crc;
	int filesize;
	int mark;

	mark = Hunk_LowMark ();
	buf = (byte *) FS_LoadHunkFile (mdl, &filesize);
	if (!buf) {
		if (!strcmp(mdl, "progs/player.mdl"))
			return 33168;
		else if (!strcmp(mdl, "progs/eyes.mdl"))
			return 6967;
		else
			Host_Error ("SV_CheckModel: could not load %s\n", mdl);
	}

	crc = CRC_Block(buf, filesize);
	Hunk_FreeToLowMark (mark);

	return crc;
}

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

This is only called from the SV_Map_f() function.
================
*/
void SV_SpawnServer (char *mapname, qbool devmap)
{
	char *entitystring;
	edict_t *ent;
	int i;
	extern qbool sv_allow_cheats;
	extern cvar_t sv_cheats;

	Com_DPrintf ("SpawnServer: %s\n", mapname);

	NET_InitServer();

	SV_SaveSpawnparms ();

	svs.spawncount++; // any partially connected client will be restarted

	sv.state = ss_dead;
	com_serveractive = false;
	Cvar_ForceSet (&sv_paused, "0");

	Host_ClearMemory();

	if (deathmatch.value)
		Cvar_Set (&coop, "0");
	current_skill = (int)(skill.value + 0.5);
	if (current_skill < 0)
		current_skill = 0;
	if (current_skill > 3)
		current_skill = 3;
	Cvar_Set (&skill, va("%d", (int)current_skill));

	if ((sv_cheats.value || devmap) && !sv_allow_cheats) {
		sv_allow_cheats = true;
		Info_SetValueForStarKey (svs.info, "*cheats", "ON", MAX_SERVERINFO_STRING);
	} else if ((!sv_cheats.value && !devmap) && sv_allow_cheats) {
		sv_allow_cheats = false;
		Info_SetValueForStarKey (svs.info, "*cheats", "", MAX_SERVERINFO_STRING);
	}

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof(sv));

	SZ_Init (&sv.datagram, sv.datagram_buf, sizeof(sv.datagram_buf));
	sv.datagram.allowoverflow = true;

	SZ_Init (&sv.reliable_datagram, sv.reliable_datagram_buf, sizeof(sv.reliable_datagram_buf));

	SZ_Init (&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));

	SZ_Init (&sv.signon, sv.signon_buffers[0], sizeof(sv.signon_buffers[0]));
	sv.num_signon_buffers = 1;

	// load progs to get entity field count
	// which determines how big each edict is
	PR_LoadProgs ();

	// allocate edicts
	sv.edicts = (edict_t *) Hunk_AllocName (SV_MAX_EDICTS*pr_edict_size, "edicts");

	// leave slots at start for clients only
	sv.num_edicts = MAX_CLIENTS+1;
	for (i = 0; i < MAX_CLIENTS; i++) {
		ent = EDICT_NUM(i+1);
		svs.clients[i].edict = ent;
		svs.clients[i].old_frags = 0; //ZOID - make sure we update frags right
	}

	sv.time = 1.0;

#ifndef SERVERONLY
	{
		void R_PreMapLoad (char *mapname);
		R_PreMapLoad (mapname);
	}
#endif

	strlcpy (sv.mapname, mapname, sizeof(sv.mapname));
	Cvar_ForceSet (&host_mapname, mapname);
	snprintf (sv.modelname, sizeof(sv.modelname), "maps/%s.bsp", mapname);

	sv.worldmodel = CM_LoadMap (sv.modelname, false, &sv.map_checksum, &sv.map_checksum2);

	// clear physics interaction links
	SV_ClearWorld ();

	sv.sound_precache[0] = pr_strings;

	sv.model_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;
	sv.models[1] = sv.worldmodel;
	for (i = 1; i < CM_NumInlineModels(); i++) {
		sv.model_precache[1+i] = localmodels[i];
		sv.models[i + 1] = CM_InlineModel (localmodels[i]);
	}

	//check player/eyes models for hacks
	sv.model_player_checksum = SV_CheckModel("progs/player.mdl");
	sv.eyes_player_checksum = SV_CheckModel("progs/eyes.mdl");

	// spawn the rest of the entities on the map

	// precache and static commands can be issued during map initialization
	sv.state = ss_loading;
	com_serveractive = true;

	ent = EDICT_NUM(0);
	ent->free = false;
	ent->v.model = PR_SetString(sv.modelname);
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	pr_global_struct->mapname = PR_SetString(sv.mapname);
	// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;

	// run the frame start qc function to let progs check cvars
	SV_ProgStartFrame ();

	// load and spawn all other entities
	entitystring = NULL;
	if ((int) sv_loadentfiles.value) {
		int filesize;
		entitystring = (char *)FS_LoadHunkFile (va("maps/%s.ent", sv.mapname), &filesize);
		if (entitystring) {
			Com_DPrintf ("Using entfile maps/%s.ent\n", sv.mapname);
			Info_SetValueForStarKey (svs.info, "*entfile", va("%i",
				CRC_Block((byte *)entitystring, filesize)), MAX_SERVERINFO_STRING);
		}
	}

	if (!entitystring) {
		Info_SetValueForStarKey (svs.info,  "*entfile", "", MAX_SERVERINFO_STRING);
		entitystring = CM_EntityString();
	}
	ED_LoadFromFile (entitystring);

	// look up some model indexes for specialized message compression
	SV_FindModelNumbers ();

	// all spawning is completed, any further precache statements
	// or prog writes to the signon message are errors
	sv.state = ss_active;

	// run two frames to allow everything to settle
	SV_Physics ();
	sv.time += 0.1;
	SV_Physics ();
	sv.time += 0.1;
	sv.old_time = sv.time;

	// save movement vars
	SV_SetMoveVars();

	// create a baseline for more efficient communications
	SV_CreateBaseline ();
	sv.signon_buffer_size[sv.num_signon_buffers - 1] = sv.signon.cursize;

	Info_SetValueForKey (svs.info, "map", sv.mapname, MAX_SERVERINFO_STRING);
	Com_DPrintf ("Server spawned.\n");

#ifndef SERVERONLY
	if (!dedicated)
	{
		void CL_ClearState (void);
		CL_ClearState ();
	}
#endif
}
