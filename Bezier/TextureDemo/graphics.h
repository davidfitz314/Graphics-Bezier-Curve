#include <GL/glut.h>

void text_output(float x, float y, char *string);
void DrawCircle(float x1, float y1, float radius);
void DrawRectangle(float x1, float y1, float x2, float y2);
void DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3);

// "Prototypes" of some color variables defined in graphics.cpp
extern GLfloat redMaterial[];
extern GLfloat greenMaterial[];
extern GLfloat brightGreenMaterial[];
extern GLfloat blueMaterial[];
extern GLfloat whiteMaterial[];
