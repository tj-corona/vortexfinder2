#include <QMouseEvent>
#include <QMatrix4x4>
#include <QDebug>
#include <glut.h>
#include "widget.h"

#define RESET_GLERROR()\
{\
  while (glGetError() != GL_NO_ERROR) {}\
}

#define CHECK_GLERROR()\
{\
  GLenum err = glGetError();\
  if (err != GL_NO_ERROR) {\
    const GLubyte *errString = gluErrorString(err);\
    qDebug("[%s line %d] GL Error: %s\n",\
            __FILE__, __LINE__, errString);\
  }\
}

using namespace std; 

CGLWidget::CGLWidget(const QGLFormat& fmt, QWidget *parent, QGLWidget *sharedWidget)
  : QGLWidget(fmt, parent, sharedWidget), 
    _fovy(30.f), _znear(0.1f), _zfar(10.f), 
    _eye(0, 0, 2.5), _center(0, 0, 0), _up(0, 1, 0)
{
}

CGLWidget::~CGLWidget()
{
}

void CGLWidget::mousePressEvent(QMouseEvent* e)
{
  _trackball.mouse_rotate(e->x(), e->y()); 
}

void CGLWidget::mouseMoveEvent(QMouseEvent* e)
{
  _trackball.motion_rotate(e->x(), e->y()); 
  updateGL(); 
}

void CGLWidget::keyPressEvent(QKeyEvent* e)
{
  switch (e->key()) {
  default: break; 
  }
}

void CGLWidget::wheelEvent(QWheelEvent* e)
{
  _trackball.wheel(e->delta());
  updateGL(); 
}

void CGLWidget::initializeGL()
{
  _trackball.init();

  glEnable(GL_MULTISAMPLE);

  GLint bufs, samples; 
  glGetIntegerv(GL_SAMPLE_BUFFERS, &bufs); 
  glGetIntegerv(GL_SAMPLES, &samples); 

  glEnable(GL_LINE_SMOOTH); 
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST); 
  
  glEnable(GL_POLYGON_SMOOTH); 
  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST); 

  glEnable(GL_BLEND); 
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
  
  // initialze light for tubes
  {
    GLfloat ambient[]  = {0.1, 0.1, 0.1}, 
            diffuse[]  = {0.5, 0.5, 0.5}, 
            specular[] = {0.8, 0.8, 0.8}; 
    GLfloat dir[] = {0, 0, -1}; 
    GLfloat shiness = 100; 

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient); 
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse); 
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular); 
    // glLightfv(GL_LIGHT0, GL_POSITION, pos); 
    glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, dir); 
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE); 

    glEnable(GL_NORMALIZE); 
    glEnable(GL_COLOR_MATERIAL); 
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular); 
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shiness); 
  }

  // load data
#if 0
  {
    FILE *fp = fopen("out", "rb");
    int npts; 
    fread(&npts, sizeof(int), 1, fp); 
    _vertices.resize(npts*3); 
    fread((void*)_vertices.data(), sizeof(float), npts*3, fp);
    _rhos.resize(npts); 
    fread((void*)_rhos.data(), sizeof(float), npts, fp);
    fclose(fp);

    fprintf(stderr, "read %d vertices\n", npts); 
  }
#endif
  LoadVortexObjects(); 

  CHECK_GLERROR(); 
}

void CGLWidget::resizeGL(int w, int h)
{
  _trackball.reshape(w, h); 
  glViewport(0, 0, w, h);
  
  CHECK_GLERROR(); 
}

void CGLWidget::paintGL()
{
  glClearColor(1, 1, 1, 0); 
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

  QMatrix4x4 projmatrix, mvmatrix; 
  projmatrix.setToIdentity(); 
  projmatrix.perspective(_fovy, (float)width()/height(), _znear, _zfar); 
  mvmatrix.setToIdentity();
  mvmatrix.lookAt(_eye, _center, _up);
  mvmatrix.rotate(_trackball.getRotation());
  mvmatrix.scale(_trackball.getScale()); 
#if 0
  mvmatrix.scale(1.0/_ds->dims()[0]); 
  mvmatrix.translate(-0.5*_ds->dims()[0], -0.5*_ds->dims()[1], -0.5*_ds->dims()[2]);
#endif

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glLoadMatrixd(projmatrix.data()); 
  glMatrixMode(GL_MODELVIEW); 
  glLoadIdentity(); 
  glLoadMatrixd(mvmatrix.data()); 

  glColor3f(0, 0, 0); 
  glScalef(0.01, 0.01, 0.01); 

  glBegin(GL_POINTS); 
  for (int i=0; i<_vertices.size()/3; i++) {
    float r = std::min(_rhos[i], 1.f); 
    glColor4f(1-r, r, 0, 1-pow(r, 0.1)); 
    glVertex3f(_vertices[i*3], _vertices[i*3+1], _vertices[i*3+2]);
  }
  glEnd(); 

  CHECK_GLERROR(); 
}

void CGLWidget::LoadVortexObjects()
{
  size_t offset_size[2] = {0}; 
  FILE *fp_offset = fopen("offset", "rb"), 
       *fp = fopen("vortex", "rb");
  size_t count = _vortex_objects.size(); 

  fread(&count, sizeof(size_t), 1, fp_offset);
  _vortex_objects.resize(count); 

  for (int i=0; i<_vortex_objects.size(); i++) {
    std::string buf; 
    
    fread(offset_size, sizeof(size_t), 2, fp_offset);
    buf.resize(offset_size[1]); 

    fread((char*)buf.data(), 1, buf.size(), fp); 
    
    _vortex_objects[i].Unserialize(buf);
  }
}
