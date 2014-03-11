#include "BlendLoader.h"
#include "ModelLoader.h"
#include <cfloat>

BlendLoader::BlendLoader()
{
    m_loaded = false;
    m_minX = FLT_MAX;
    m_maxX = FLT_MIN;
    m_minY = FLT_MAX;
    m_maxY = FLT_MIN;
    m_minZ = FLT_MAX;
    m_maxZ = FLT_MIN;
}

BlendLoader::~BlendLoader()
{
}

/********** BLENDER DATA ***************************/

bool BlendLoader::load(const BlendData *blendData)
{
    uint64_t start = PlatformAdapter::currentMillis();
    uint32_t chunk = 0;

    uint16_t* new_faces = new uint16_t[blendData->totface*STRIDE];

    /* initialize counters and indices */
    m_new_pos_count = 0;
    m_new_norm_count = 0;
    m_new_uv_count = 0;
    m_total_index_count = 0;

    for(int vtype = 0; vtype < 4; vtype++)
    {
        m_new_index_count[vtype] = 0;
        m_new_f3_start[vtype] = 0;
        m_new_f4_start[vtype] = 0;
    }

    /* for now we do only VT_POS */
    m_new_index_count[VT_POS] = blendData->totface;
    m_new_f3_start[VT_POS] = 0;
    m_new_f4_start[VT_POS] = -1;
    m_total_index_count += m_new_index_count[VT_POS];

    /* reindex VT_POS */
    reindexTypeBlender(chunk, VT_POS, blendData, new_faces);

    GLfloat* new_pos = new GLfloat[blendData->totvert * STRIDE];
    GLfloat* new_uv = new GLfloat[blendData->totvert * STRIDE];
    GLfloat* new_norm = new GLfloat[blendData->totvert * STRIDE];

    if(new_pos==NULL || new_uv==NULL || new_norm==NULL) return false;

    P3D_LOGD("GLfloat buffers allocated");

    for(chunk = 0; chunk < m_chunks.size(); ++chunk)
    {
        P3D_LOGD("chunk: %d", chunk);
        P3D_LOGD(" index count: %d", m_chunks[chunk].indexCount);
        P3D_LOGD(" vert count: %d", m_chunks[chunk].vertCount);
        P3D_LOGD(" f3 offset: %d", m_chunks[chunk].f3Offset);
        P3D_LOGD(" f4 offset: %d", m_chunks[chunk].f4Offset);
        P3D_LOGD(" material: %d", m_chunks[chunk].material);
    }

    P3D_LOGD("----------");

    for(P3dMap<uint32_t, P3dMap<VertexIndex, uint32_t>*>::iterator itr = m_vertex_maps.begin(); itr.hasNext(); ++itr)
    {
        P3D_LOGD("vertex bank:");
        P3D_LOGD(" offset: %d", itr.key());
        P3D_LOGD(" count: %d", itr.value()->size());
        copyVertDataBlender(itr.key(), itr.value(), blendData, new_norm, new_uv, new_pos);
        itr.value()->dumpBucketLoad();
        delete itr.value();
    }

    P3D_LOGD("data copied");
    m_vertex_maps.clear();

    m_modelLoader->createModel(m_new_pos_count, m_new_norm_count, m_new_empty_norm_count, m_new_uv_count,
                new_pos, new_norm, new_uv, m_total_index_count,
                new_faces, m_chunks.size(), 0);

    delete [] new_norm;
    delete [] new_uv;
    delete [] new_pos;

    delete [] new_faces;

    P3D_LOGD("reindex took %lldms", PlatformAdapter::durationMillis(start));
    m_loaded = true;
    return true;

}

uint32_t BlendLoader::reindexTypeBlender(uint32_t &chunk, BlendLoader::VertexType vtype, const BlendData *blendData,
                                  uint16_t* new_faces)
{
    uint32_t pos_offset;
    uint16_t mat = 0;
    uint32_t face;
    uint32_t fcount;
    uint32_t vert;
    uint32_t verts;
    uint32_t new_offset;

    uint64_t start = PlatformAdapter::currentMillis();

    uint32_t result = 0;
    P3dMap<VertexIndex, uint32_t>* vertexMap = 0;
    bool in_quad = false;

    fcount = blendData->totface;

    if(fcount == 0)
    {
        // no faces of this type
        return result;
    }

#if 0
    P3D_LOGD("fcount: %u", fcount);


    float *vp = blendData->verts;
    for(uint i = 0; i < blendData->totvert; i++) {
        P3D_LOGD("v: %f %f %f", vp[i*3], vp[i*3+1], vp[i*3+2]);
    }

    P3D_LOGD("----------------");

    uint32_t *fp = blendData->faces;
    for(uint i = 0; i < blendData->totface; i++) {
        P3D_LOGD("f: %u %u %u", fp[i*3], fp[i*3+1], fp[i*3+2]);
    }

    P3D_LOGD("<<<<<<<<<<<<<<<");
#endif

    // tris
    pos_offset = 0;

    VertexIndex index;
    index.type = vtype;
    uint16_t new_index;
    new_offset = 0; //TODO: should be passed in?
    //new_mat_offset = 0;

    for(face = 0; face < fcount; ++face)
    {
        if(face == 0)
        {
            nextChunkBlender(chunk, vtype, in_quad, new_offset, m_new_pos_count / 3, true);
            m_chunks[chunk].material = mat;
            vertexMap = m_vertex_maps[m_chunks[chunk].vertOffset];
        }

        verts = in_quad ? 4 : 3;
        for(vert = 0; vert < verts; ++vert)
        {
            index.pos = blendData->faces[pos_offset];
            pos_offset++;

            /* TODO: norm */
            index.norm = 0;
            index.uv = 0;

            if(vertexMap->count(index))
            {
                new_index = (*vertexMap)[index];
            }
            else
            {
                new_index = vertexMap->size();
                vertexMap->insert(index, new_index);

                m_new_pos_count += 3;

                m_new_norm_count += 3;
                m_new_empty_norm_count += 3;
            }

            //P3D_LOGD("face %u, idx %u, new idx %u, %u", face, index.pos, new_index, vertexMap->size());
            new_faces[new_offset] = (uint16_t)new_index;
            ++new_offset;
        }
    }

    m_chunks[chunk].vertCount = (m_new_pos_count - STRIDE * m_chunks[chunk].vertOffset) / 3;

    P3D_LOGD("reindex type Blender took: %lldms", PlatformAdapter::durationMillis(start));

    return result;
}

void BlendLoader::nextChunkBlender(uint32_t &chunk, BlendLoader::VertexType vtype, bool in_f4, uint32_t new_offset,
                            uint32_t vertOffset, bool firstOfType)
{
    P3D_LOGD("new_offset: %d", new_offset);
    P3D_LOGD("vertOffset: %d", vertOffset);
    if(m_chunks.size() != 0)
    {
        ++chunk;
    }
    m_chunks[chunk] = MeshChunk();
    MeshChunk& newChunk = m_chunks[chunk];
    newChunk.indexCount = m_new_index_count[vtype];
    newChunk.f3Offset = new_offset;

    newChunk.validNormals = false;
    newChunk.hasUvs = false;

    newChunk.vertOffset = vertOffset;

    if(firstOfType)
    {
        newChunk.indexCount = m_new_index_count[vtype];
        newChunk.f3Offset = m_new_f3_start[vtype];
        newChunk.f4Offset = m_new_f4_start[vtype];
    }
    /*else
    {
        MeshChunk& oldChunk = m_chunks[chunk - 1];
        if(!in_f4)
        {
            newChunk.f4Offset = oldChunk.f4Offset - new_offset;
        }
        newChunk.indexCount = oldChunk.indexCount - (new_offset - oldChunk.f3Offset);
        oldChunk.indexCount = new_offset - oldChunk.f3Offset;
        oldChunk.vertCount = (m_new_pos_count - oldChunk.vertOffset * 3) / 3;
    }*/

    if(m_vertex_maps.count(newChunk.vertOffset) == 0)
    {
        m_vertex_maps[newChunk.vertOffset] = new P3dMap<VertexIndex, uint32_t>(8192);
    }
}



void BlendLoader::copyVertDataBlender(uint32_t vertOffset, P3dMap<VertexIndex, uint32_t>* vertexMap, const BlendData* data,
                               GLfloat* new_norm, GLfloat* new_uv, GLfloat* new_pos)
{
    uint32_t new_offset = 0;
    uint32_t vert_offset = vertOffset;
    float x;
    float y;
    float z;
    uint vertCount = 0;
    P3D_LOGD("vert offset: %u", vertOffset);
    for(P3dMap<VertexIndex, uint32_t>::iterator itr = vertexMap->begin(); itr.hasNext(); ++itr)
    {
        ++vertCount;
        const VertexIndex& index = itr.key();
        uint32_t new_index = itr.value();
        P3D_LOGD("%u: %u/%u/%u > %u", vertCount, index.pos, index.uv, index.norm, new_index);
        // pos
        vert_offset = index.pos * STRIDE;
        new_offset = (new_index + vertOffset) * STRIDE;

        x = data->verts[vert_offset++];
        y = data->verts[vert_offset++];
        z = data->verts[vert_offset];

        P3D_LOGD("%u @ %u: (%f,%f,%f)", vertCount, vert_offset, x, y, z);

        if(x > m_maxX) m_maxX = x;
        if(x < m_minX) m_minX = x;
        if(y > m_maxY) m_maxY = y;
        if(y < m_minY) m_minY = y;
        if(z > m_maxZ) m_maxZ = z;
        if(z < m_minZ) m_minZ = z;
        new_pos[new_offset++] = x;
        new_pos[new_offset++] = y;
        new_pos[new_offset] = z;

        // norm
        vert_offset = index.pos * STRIDE;
        new_offset = (new_index + vertOffset) * STRIDE;
        // store empty normal
        // TODO: actual normal storage if present in BlendData
        new_norm[new_offset] = 0.0f;
        new_uv[new_offset++] = 0.0f;
        new_norm[new_offset] = 0.0f;
        new_uv[new_offset++] = 0.0f;
        new_norm[new_offset] = 0.0f;
        new_uv[new_offset] = 0.0f;
    }

    P3D_LOGD("BB: %f:%f %f:%f %f:%f", m_minX, m_maxX, m_minY, m_maxY, m_minZ, m_maxZ);
}
