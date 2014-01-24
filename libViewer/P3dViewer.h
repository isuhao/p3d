#ifndef P3DVIEWER_H
#define P3DVIEWER_H

#include <cstdlib>

typedef unsigned int GLuint;
typedef unsigned int GLenum;

class PlatformAdapter;
class ModelLoader;

class P3dViewer
{
public:
    //! \brief creates a P3dViewer
    //! \arg adapter PlatformAdaper
    //! \note P3dViewer takes ownership of adapter and will delete it in dtor
    P3dViewer(PlatformAdapter* adapter = 0);
    virtual ~P3dViewer();
    void onSurfaceCreated();
    void onSurfaceChanged(int width, int height);
    void drawFrame();
    bool loadModel(const char* binaryData, size_t size);

private:
    GLuint loadShader (GLenum type, const char *shaderSrc);
    GLuint loadShaderFromFile (GLenum type, const char *shaderFile);

    ModelLoader* m_ModelLoader;
    GLuint m_ProgramObject;
    GLuint m_VertexPosObject;
    int m_Width;
    int m_Height;
    bool m_InitOk;
};

#endif // P3DVIEWER_H
