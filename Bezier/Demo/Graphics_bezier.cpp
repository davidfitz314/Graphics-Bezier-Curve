// Bezier Surfaces Demo.

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <GL/glut.h>
#include "graphics.h"
#include "vector.h"
#include "trackbal.h"

double curquat[4];
static int rotation_mouse=GL_FALSE;
static int rotation_mousex,rotation_mousey;
static double lastquat[4];

class BezierSurface
{
public:
	Point3 cp[4][4];

	BezierSurface();
	Point3 Evaluate(float s, float t);
	void DrawSurfaceWireframe();
	void DrawPoints();
};

float random_float(float low, float high)
{
	float x = (float)rand() / (float)RAND_MAX; // 0 to 1
	float scale = high-low;
	x *= scale; // 0 to scale
	x += low; // low to high;
	return x;
}

BezierSurface::BezierSurface()
{
	for(int r=0; r<4; r++)
	{
		for(int c=0; c<4; c++)
		{
			cp[r][c].p[0] = (c-1.5)/1.5;
			cp[r][c].p[1] = (r-1.5)/1.5;
			cp[r][c].p[2] = random_float(-1, 1);;
		}
	}
}

Point3 EvalCurve(float t, Point3 p0, Point3 p1, Point3 p2, Point3 p3)
{
	Point3 result;
	for(int i=0; i<3; i++)
	{
		result.p[i] = p0.p[i]*(1-t)*(1-t)*(1-t) + 3*p1.p[i]*(1-t)*(1-t)*t + 3*p2.p[i]*(1-t)*t*t + p3.p[i]*t*t*t;
	}
	return result;
}

Point3 BezierSurface::Evaluate(float s, float t)
{
	Point3 temp[4];
	for(int i=0; i<4; i++)
	{
		temp[i] = EvalCurve(s, cp[i][0], cp[i][1], cp[i][2], cp[i][3]);
	}

	Point3 result = EvalCurve(t, temp[0], temp[1], temp[2], temp[3]);
	return result;
}

void DrawSphere(float x, float y, float z, float radius)
{
	glPointSize(10);
	glBegin(GL_POINTS);
	glVertex3f(x,y,z);
	glEnd();

	/*
	glPushMatrix();
	glTranslatef(x,y,z);
	int slices = 40;
	int stacks = 40;
	glutSolidSphere(radius, slices, stacks);
	glPopMatrix();
	*/
}

void DrawAxes(float length)
{
	glLineWidth(3.0);
	glBegin(GL_LINES);

	// x axis in red
	glColor3f(1,0,0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(length, 0.0, 0.0);
	// y axis in green
	glColor3f(0,1,0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, length, 0.0);
	// z axis in blue
	glColor3f(0,0,1);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, length);

	glEnd();
	glLineWidth(1.0);
}

void BezierSurface::DrawPoints()
{
	glColor3fv(whiteMaterial);
	for(int r=0; r<4; r++)
	{
		for(int c=0; c<4; c++)
		{
			DrawSphere(cp[r][c].p[0], cp[r][c].p[1], cp[r][c].p[2], .05);
		}
	}
}

void BezierSurface::DrawSurfaceWireframe()
{
	const float steps = 20;
	const float inc = 1.0 / steps;
	float s=0,t=0;

	glColor3fv(whiteMaterial);
	glBegin(GL_LINES);

	int i,j;

	// Draw lines in s direction
	for(j=0; j<=steps; j++)
	{
		s=0;
		for(i=0; i<steps; i++)
		{
			Point3 from = Evaluate(s,t);
			Point3 to = Evaluate(s+inc,t);
			glVertex3fv((float*)&(from.p[0]));
			glVertex3fv((float*)&(to.p[0]));
			s+= inc;
		}
		t+= inc;
	}

	// Draw lines in t direction
	s=0;
	for(j=0; j<=steps; j++)
	{
		t=0;
		for(i=0; i<steps; i++)
		{
			Point3 from = Evaluate(s,t);
			Point3 to = Evaluate(s,t+inc);
			glVertex3fv((float*)&(from.p[0]));
			glVertex3fv((float*)&(to.p[0]));
			t+= inc;
		}
		s+= inc;
	}

	glEnd();
}


BezierSurface bez;

// Global Variables
// Some colors you can use, or make your own and add them
// here and in graphics.h
GLfloat redMaterial[] = {0.7, 0.1, 0.2, 1.0};
GLfloat greenMaterial[] = {0.1, 0.7, 0.4, 1.0};
GLfloat brightGreenMaterial[] = {0.1, 0.9, 0.1, 1.0};
GLfloat blueMaterial[] = {0.1, 0.2, 0.7, 1.0};
GLfloat whiteMaterial[] = {1.0, 1.0, 1.0, 1.0};

float screen_x = 600;
float screen_y = 500;

// 
// Functions that draw basic primitives
//
void DrawCircle(int x1, int y1, int radius)
{
	glBegin(GL_POLYGON);
	for(int i=0; i<32; i++)
	{
		double theta = (double)i/32.0 * 2.0 * 3.1415926;
		double x = x1 + radius * cos(theta);
		double y = y1 + radius * sin(theta);
		glVertex2f(x, y);
	}
	glEnd();
}

void DrawRectangle(int x1, int y1, int x2, int y2)
{
	glBegin(GL_QUADS);
	glVertex2f(x1,y1);
	glVertex2f(x2,y1);
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);
	glEnd();
}

void DrawTriangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
	glBegin(GL_TRIANGLES);
	glVertex2f(x1,y1);
	glVertex2f(x2,y2);
	glVertex2f(x3,y3);
	glEnd();
}

// Outputs a string of text at the specified location.
void text_output(int x, int y, char *string)
{
	void *font = GLUT_BITMAP_9_BY_15;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
	
	int len, i;
	glRasterPos2f(x, y);
	len = (int) strlen(string);
	for (i = 0; i < len; i++) 
	{
		glutBitmapCharacter(font, string[i]);
	}

    glDisable(GL_BLEND);
}


//
// GLUT callback functions
//

// This callback function gets called by the Glut
// system whenever it decides things need to be redrawn.
void display(void)
{
	// Test lines that draw all three shapes and some text.
	// Delete these when you get your code working.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glLoadIdentity();

	// front view
	// gluLookAt(0,0,5,  0,0,0,  0,1,0);

	// Trackball view
	double m[4][4];
	build_rotmatrix(m,curquat);
	glMultMatrixd(m[0]);


	DrawAxes(1.0);
	bez.DrawPoints();
	bez.DrawSurfaceWireframe();

	glutSwapBuffers();
	glutPostRedisplay(); 
}


// This callback function gets called by the Glut
// system whenever a key is pressed.
void keyboard(unsigned char c, int x, int y)
{
  switch (c) {
  case 27: // escape character means to quit the program
	exit(0);
    break;
  case 'q':
	exit(0);
	break;
  case 'b':
    // do something when 'b' character is hit.
    break;
  default:
    return;
  }
  glutPostRedisplay();
}


void SetPerspectiveView(int w, int h)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float aspectRatio = (GLdouble) w/(GLdouble) h;
	gluPerspective( 
	/* field of view in degree */ 38.0,
	/* aspect ratio */ aspectRatio,
	/* Z near */ .9, /* Z far */ 100.0);
	gluLookAt(0,0,5,  0,0,0,  0,1,0);
	glMatrixMode(GL_MODELVIEW);
}

// This callback function gets called by the Glut
// This callback function gets called by the Glut
// system whenever the window is resized by the user.
void reshape(int w, int h)
{
	screen_x = w;
	screen_y = h;
	glViewport(0, 0, w, h);

	SetPerspectiveView(w,h);
}


// This callback function gets called by the Glut
// system whenever any mouse button goes up or down.
void mouse(int mouse_button, int state, int x, int y)
{
	y=screen_y-y-1; /* orient screen so 0,0 is bottom left, not top left */
	if (mouse_button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) 
	{
	}
	if (mouse_button == GLUT_LEFT_BUTTON && state == GLUT_UP) 
	{
	}
	if (mouse_button == GLUT_MIDDLE_BUTTON) 
	{
		rotation_mousex = x;
		rotation_mousey = y;
		if(state==GLUT_DOWN)
		{
			rotation_mouse = GL_TRUE;
		}
		if(state==GLUT_UP)
		{
			rotation_mouse = GL_FALSE;
		}
	}

	glutPostRedisplay();
}

void mousemotion(int x,int y)
{
	y=screen_y-y-1; /* orient screen so 0,0 is bottom left, not top left */
	if(rotation_mouse)
	{
		if(rotation_mousex!=x || rotation_mousey!=y)
		{
			trackball(lastquat,
					(double)(2*rotation_mousex-screen_x)/(double)screen_x,
					(double)(2*rotation_mousey-screen_y)/(double)screen_y,
					(double)(2*x-screen_x)/(double)screen_x,
					(double)(2*y-screen_y)/(double)screen_y);
			add_quats(lastquat,curquat,curquat);
			rotation_mousex=x;
			rotation_mousey=y;
		}
	}
} /* mousemotion */

// Your initialization code goes here.
void InitializeMyStuff()
{
	trackball(curquat, (double)0.0, (double)0.0, (double)0.0, (double)0.0);
}



int main(int argc, char **argv)
{
	glutInitWindowSize(700, 500);
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

	int fullscreen = 0;
	if (fullscreen) 
	{
		glutGameModeString("800x600:32");
		glutEnterGameMode();
	} 
	else 
	{
		glutCreateWindow("Shapes");
	}

	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutMotionFunc(mousemotion);

	glClearColor(.0,.0,.0,0);	
	InitializeMyStuff();

	glutMainLoop();

	return 0;
}
