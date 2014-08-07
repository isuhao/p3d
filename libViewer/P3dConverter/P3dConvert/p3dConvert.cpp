/*
 ------------------------------------------------------------------------------
 This file is part of the P3d .blend converter.

 Copyright (c) Nathan Letwory ( nathan@p3d.in / http://p3d.in )

 The converter uses FBT (File Binary Tools) from gamekit.
 http://gamekit.googlecode.com/

 ------------------------------------------------------------------------------
*/
#include "p3dConvert.h"
#include "fbtBlend.h"
#include "Blender.h"
#include <stdio.h>
#include <stdlib.h>

using namespace Blender;

P3dConverter::P3dConverter() {

}

P3dConverter::~P3dConverter() {
	m_pme.clear();
}

int P3dConverter::parse_blend(const char *data, size_t length) {
	bool gzipped = false;

	if (m_fp.parse(data, length, fbtFile::PM_READTOMEMORY, false) != fbtFile::FS_OK) {
		if (m_fp.parse(data, length, fbtFile::PM_COMPRESSED, false) != fbtFile::FS_OK)
		{
			fbtPrintf(" [NOK]\n");
			return 1;
		}
		gzipped = true;
	}
	fbtPrintf(" %s | %d%s[OK]\n", m_fp.getHeader().c_str(), m_fp.getVersion(), gzipped ? " compressed ": "");

	extract_all_geometry();

	fbtPrintf(" Done extracting all geometry\n");
	
	return 0;
}

void P3dConverter::loop_data(MLoopUV* mpuv, Chunk *chunk, int curf, MLoop *loop, MLoopUV *luv)
{
	chunk->f[curf] = (uint32_t)loop->v;
	if(mpuv && luv) {
		chunk->uv[loop->v*2] = luv->uv[0];
		chunk->uv[loop->v*2+1] = luv->uv[1];
	}
}

void P3dConverter::extract_geometry(Object *ob) {
	uint32_t totf3 = 0;
	uint32_t totfx = 0;
	totmesh = 0;

	if(ob->type != 1 || !ob->data) throw;

	P3dMesh pme = P3dMesh();

	auto me = (Mesh *)ob->data;
	auto mvert = me->mvert;

	/* for now only one chunk. */
	pme.m_chunk = new Chunk();

	auto chunk = pme.m_chunk;

	/* create vertex pos buffer */
	chunk->totvert = me->totvert;
	chunk->v = new float[3*me->totvert];
	for(uint32_t i=0, curv = 0; i < chunk->totvert; i++, mvert++) {
		chunk->v[curv++] = mvert->co[0];
		chunk->v[curv++] = mvert->co[1];
		chunk->v[curv++] = mvert->co[2];
		/* \todo read mvert->no */
	}

	/* if totface > 0 we have legacy mesh format */
	if(me->totface) {

		/* count tris */
		MFace *mf = me->mface;
		for(uint32_t j=0; j < (uint32_t)me->totface; j++, mf++) {
			if(mf->v4==0) {
				totf3++;
			} else {
				totfx++;
			}
		}
		/* create buffer for tri indices */
		chunk->totface = totf3 + totfx*2;
		chunk->f = new uint32_t[3 * chunk->totface];
		mf = me->mface;
		for(uint32_t j=0, curf=0; j < (uint32_t)me->totface; j++, mf++) {
			if(mf->v4==0) {
				chunk->f[curf] = (uint32_t)mf->v1;
				chunk->f[curf+1] = (uint32_t)mf->v2;
				chunk->f[curf+2] = (uint32_t)mf->v3;
				curf+=3;
			} else {
				chunk->f[curf] = (uint32_t)mf->v1;
				chunk->f[curf+1] = (uint32_t)mf->v2;
				chunk->f[curf+2] = (uint32_t)mf->v3;
				chunk->f[curf+3] = (uint32_t)mf->v1;
				chunk->f[curf+4] = (uint32_t)mf->v3;
				chunk->f[curf+5] = (uint32_t)mf->v4;
				curf+=6;
			}
		}
	/* mesh data since bmesh */
	} else if (me->totpoly) {
		auto mpuv = me->mloopuv;
		/* count tris */
		auto mp = me->mpoly;
		for(uint32_t j=0; j < (uint32_t)me->totpoly; j++, mp++) {
			if(mp->totloop==3) {
				totf3++;
			} else if (mp->totloop == 4) {
				totfx++;
			}
		}
		/* create buffer for tri indices */
		chunk->totface = totf3 + totfx*2;
		/* create buffer for UV coords */
		if(mpuv) {
			chunk->uv = new float[chunk->totvert*2];
			fbtPrintf("Got UV\n");
		}
		chunk->f = new uint32_t[3 * chunk->totface];
		/* reset mp to start of mpoly */
		mp = me->mpoly;
		for(int j=0, curf=0; j < me->totpoly; j++, mp++) {
			MLoopUV *luv = nullptr;
			MLoop *loop = &me->mloop[mp->loopstart];

			if(mpuv) luv = &me->mloopuv[mp->loopstart];

			if(mp->totloop==3) {
				loop_data(mpuv, chunk, curf, loop, luv);
				loop++;
				luv++;
				loop_data(mpuv, chunk, curf+1, loop, luv);
				loop++;
				luv++;
				loop_data(mpuv, chunk, curf+2, loop, luv);
				curf+=3;
			} else if (mp->totloop==4) {
				loop_data(mpuv, chunk, curf, loop, luv);
				loop_data(mpuv, chunk, curf+3, loop, luv);
				loop++;
				luv++;
				loop_data(mpuv, chunk, curf+1, loop, luv);
				loop++;
				luv++;
				loop_data(mpuv, chunk, curf+2, loop, luv);
				loop_data(mpuv, chunk, curf+4, loop, luv);
				loop++;
				luv++;
				loop_data(mpuv, chunk, curf+5, loop, luv);
				curf+=6;
			}
		}
	}
	m_pme.push_back(pme);
}

size_t P3dConverter::count_mesh_objects() {
	totmesh = 0;
	fbtList& objects = m_fp.m_object;
	for (Object* ob = (Object*)objects.first; ob; ob = (Object*)ob->id.next) {
		if (ob->data && ob->type == 1) {
			totmesh++;
		}
	}

	return totmesh;
}

void P3dConverter::extract_all_geometry() {

	size_t count = count_mesh_objects();

	fbtPrintf("%d mesh object%s found\n", count, count==1?"":"s");

	fbtList& objects = m_fp.m_object;
	for (Object* ob = (Object*)objects.first; ob; ob = (Object*)ob->id.next) {
		if (ob->data && ob->type == 1) {
			extract_geometry(ob);
		}
	}
	fbtPrintf(" @.\n");
}
	
