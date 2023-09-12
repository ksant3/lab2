//modified by: Karen Santiago
//date: 9/5/2023
//
//author: Gordon Griesel
//date: Fall 2023
//purpose: 1. learn OpenGL
//         2. write an aerospace related program
//         3. prepare to apply for a job at SpaceX
//
//This is a game in which we try to land the rocket booster back on
//the launch pad. You must add code to check for a good landing, and
//also show the rocket landed and secure on the pad.
//
//If the rocket doesn't land safely, then you can show some kind of
//explosion or whatever.
//
#include <iostream>
using namespace std;
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include "log.h"
#include "fonts.h"

//floating point random numbers
#define rnd() (float)rand() / (float)RAND_MAX

//gravity pulling the rocket straight down
const float GRAVITY = 0.005;  //we can adjust gravity

class Global {
public:
	int xres, yres;
	unsigned int keys[65536];
	int failed_landing;
    int successful_landing;
    int debug;
	Global() {
		xres = 400;
		yres = 600;
		failed_landing = 0;
        successful_landing = 0;
        debug = 0;
    }
} g;

class Lz {
	//landing zone
	public:
	float pos[2];
	float width;
	float height;
	Lz() {
		pos[0] = 100.0f;
		pos[1] = 20.0f;
		width =  50.0f;
		height =  8.0f;
	}
} lz;

class Lander {
	//the rocket
	public:
	float pos[2];
	float vel[2];
	float verts[3][2];
	float thrust;
	double angle;
	Lander() {
		init();
	}
	void init() {
		pos[0] = 200.0f;
		pos[1] = g.yres - 60.0f;
#ifdef TESTING
        pos[0] = 100.0f;
        pos[1] = 40.0f;
#endif
		vel[0] = vel[1] = 0.0f;
		//3 vertices of triangle-shaped rocket lander
		verts[0][0] = -10.0f;
		verts[0][1] =   0.0f;
		verts[1][0] =   0.0f;
		verts[1][1] =  30.0f;
		verts[2][0] =  10.0f;
		verts[2][1] =   0.0f;
		angle = 0.0;
		thrust = 0.0f;
		g.failed_landing = 0;
        g.successful_landing = 0;
	}
} lander;

class X11_wrapper {
private:
	Display *dpy;
	Window win;
	GLXContext glc;
public:
	~X11_wrapper();
	X11_wrapper();
	void set_title();
	bool getXPending();
	XEvent getXNextEvent();
	void swapBuffers();
	void reshape_window(int width, int height);
	void check_resize(XEvent *e);
	void check_mouse(XEvent *e);
	int check_keys(XEvent *e);
} x11;

//Function prototypes
void init_opengl(void);
void physics(void);
void render(void);



//=====================================
// MAIN FUNCTION IS HERE
//=====================================
int main(int argc, char *argv[])
//int main()
{
    if (argc > 1) {
        g.debug = 1;
    }
    if (g.debug) {
        logOpen();
    }
	init_opengl();
	printf("Press T or Up-arrow for thrust.\n");
	printf("Press Left or Right arrows for rocket thrust vector.\n");
	//Main loop
	int done = 0;
	while (!done) {
		//Process external events.
		while (x11.getXPending()) {
			XEvent e = x11.getXNextEvent();
			x11.check_resize(&e);
			x11.check_mouse(&e);
			done = x11.check_keys(&e);
		}
		physics();
		render();
		x11.swapBuffers();
		usleep(400);
    } 
    if (g.debug) {
        logClose();
    }
    cleanup_fonts(); //
	return 0;
}

X11_wrapper::~X11_wrapper()
{
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

X11_wrapper::X11_wrapper()
{
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	int w = g.xres, h = g.yres;
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		cout << "\n\tcannot connect to X server\n" << endl;
		exit(EXIT_FAILURE);
	}
	Window root = DefaultRootWindow(dpy);
	XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
	if (vi == NULL) {
		cout << "\n\tno appropriate visual found\n" << endl;
		exit(EXIT_FAILURE);
	} 
	Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	XSetWindowAttributes swa;
	swa.colormap = cmap;
	swa.event_mask =
		ExposureMask | KeyPressMask | KeyReleaseMask |
		ButtonPress | ButtonReleaseMask |
		PointerMotionMask |
		StructureNotifyMask | SubstructureNotifyMask;
	win = XCreateWindow(dpy, root, 0, 0, w, h, 0, vi->depth,
		InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
	set_title();
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);
}

void X11_wrapper::set_title()
{
	//Set the window title bar.
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "3350 Aerospace Lander Challenge");
}

bool X11_wrapper::getXPending()
{
	//See if there are pending events.
	return XPending(dpy);
}

XEvent X11_wrapper::getXNextEvent()
{
	//Get a pending event.
	XEvent e;
	XNextEvent(dpy, &e);
	return e;
}

void X11_wrapper::swapBuffers()
{
	glXSwapBuffers(dpy, win);
}

void X11_wrapper::reshape_window(int width, int height)
{
	//window has been resized.
	g.xres = width;
	g.yres = height;
	//
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
}

void X11_wrapper::check_resize(XEvent *e)
{
	//The ConfigureNotify is sent by the
	//server if the window is resized.
	if (e->type != ConfigureNotify)
		return;
	XConfigureEvent xce = e->xconfigure;
	if (xce.width != g.xres || xce.height != g.yres) {
		//Window size did change.
		reshape_window(xce.width, xce.height);
	}
}
//-----------------------------------------------------------------------------

void X11_wrapper::check_mouse(XEvent *e)
{
	static int savex = 0;
	static int savey = 0;

	//Weed out non-mouse events
	if (e->type != ButtonRelease &&
		e->type != ButtonPress &&
		e->type != MotionNotify) {
		//This is not a mouse event that we care about.
		return;
	}
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button was pressed.
			//int y = g.yres - e->xbutton.y;
			return;
		}
		if (e->xbutton.button==3) {
			//Right button was pressed.
			return;
		}
	}
	if (e->type == MotionNotify) {
		//The mouse moved!
		if (savex != e->xbutton.x || savey != e->xbutton.y) {
			savex = e->xbutton.x;
			savey = e->xbutton.y;
			//Code placed here will execute whenever the mouse moves.


		}
	}
}

int X11_wrapper::check_keys(XEvent *e)
{
	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	int key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress)
		g.keys[key] = 1;
	if (e->type == KeyRelease)
		g.keys[key] = 0;
	if (e->type == KeyPress) {
		switch (key) {
			case XK_r:
				//Key R was pressed
				lander.init();
				break;
			case XK_Escape:
				//Escape key was pressed
				return 1;
		}
	}
	return 0;
}

void init_opengl(void)
{
	//OpenGL initialization
	glViewport(0, 0, g.xres, g.yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	//Set 2D mode (no perspective)
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
	//Set the screen background color
	glClearColor(0.1, 0.1, 0.1, 1.0);
    //fonts
    glEnable(GL_TEXTURE_2D);
    initialize_fonts();
}

void physics()
{
	//Lander physics
	if (g.failed_landing) {
	   return;
    }
    if (g.successful_landing) {
        return;
    }
	lander.pos[0] += lander.vel[0];
	lander.pos[1] += lander.vel[1];
	lander.vel[1] -= GRAVITY;
	//apply thrust
	//convert angle to radians...
	float ang = ((lander.angle+90.0) / 360.0) * (3.14159 * 2.0);
	//make a thrust vector...
	float xthrust = cos(ang) * lander.thrust;
	float ythrust = sin(ang) * lander.thrust;
	lander.vel[0] += xthrust;
	lander.vel[1] += ythrust;
	lander.thrust *= 0.95f;
	if (g.keys[XK_t] || g.keys[XK_Up]) {
		//Thrust for the rocket
		lander.thrust = 0.01;
	}
	if (g.keys[XK_Left])
		lander.angle += 0.5;
	if (g.keys[XK_Right])
		lander.angle -= 0.5;
	//check for landing failure...
	if (lander.pos[1] < 0.0) {
		g.failed_landing = 1;
	}
    if (lander.pos[1] <= (lz.pos[1]) + lz.height) {
        if ((lander.pos[0] >= 50.0) && (lander.pos[0] <= 150.0) && 
                (lander.angle >= -5.0) && (lander.angle <= 5.0)) {
            g.successful_landing = 1;
        }
        if ((lander.pos[0] >= 50.0) && (lander.pos[0] <= 150.0) && 
                ((lander.angle < -5.0) || (lander.angle > 5.0))) {
            g.failed_landing = 1;
        }

    }
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT);
	//Draw Sky
	glPushMatrix();
	glBegin(GL_QUADS);
		//Each vertex has a color.
		glColor3ub(250, 160,  80); glVertex2i(0, 0); //250, 200, 90
		glColor3ub(100,  80, 200); glVertex2i(0, g.yres); //100, 80, 200
		glColor3ub(100,  80, 200); glVertex2i(g.xres, g.yres); //100, 80, 200
		glColor3ub(250, 160,  80); glVertex2i(g.xres, 0);  // 250, 200, 90
	glEnd();
	glPopMatrix();
	//Draw LZ
	glPushMatrix();
	glColor3ub(250, 250, 20);
	glTranslatef(lz.pos[0], lz.pos[1], 0.0f);
	glBegin(GL_QUADS);
		glVertex2f(-lz.width, -lz.height);
		glVertex2f(-lz.width,  lz.height);
		glVertex2f( lz.width,  lz.height);
		glVertex2f( lz.width, -lz.height);
	glEnd();
//	glColor3ub(20, 20, 20);
//	glBegin(GL_LINE_LOOP);
//		glVertex2f(-lz.width, -lz.height);
//		glVertex2f(-lz.width,  lz.height);
//		glVertex2f( lz.width,  lz.height);
//		glVertex2f( lz.width, -lz.height);
//		glVertex2f(-lz.width, -lz.height);
//	glEnd();
	glPopMatrix();
	//Draw Lander
	glPushMatrix();
	glColor3ub(250, 250, 250);
	if (g.failed_landing) {
		glColor3ub(250, 0, 0);
    }
    if (g.successful_landing) {
        glColor3f(0.0, 1.0, 0.0);

    }
	glTranslatef(lander.pos[0], lander.pos[1], 0.0f);
	glRotated(lander.angle, 0.0, 0.0, 1.0);
	glBegin(GL_TRIANGLES);
		for (int i=0; i<3; i++) {
			glVertex2f(lander.verts[i][0], lander.verts[i][1]);
		}
	glEnd();
	//Lander thrust
	if (lander.thrust > 0.0) {
		//draw the thrust vector
		glBegin(GL_LINES);
			for (int i=0; i<25; i++) {
				glColor3ub(0, 0, 255);
				glVertex2f(rnd()*10.0-5.0, 0.0);
				glColor3ub(250, 250, 0);
				glVertex2f(0.0+rnd()*14.0-7.0,
					lander.thrust * (-4000.0 - rnd() * 2000.0));
			}
		glEnd();
	}
	if (g.failed_landing) {
		//show crash graphics here...
			   
	}
	glPopMatrix();


    Rect r;
    r.bot = g.yres - 20;
    r.left = 10;
    r.center = 0;
    //ggprint06(&r, 12, 0 "ggprint06  ");
    //ggprint07(&r, 12, 0 "ggprint07  ");
    //ggprint08(&r, 12, 0 "ggprint08  ");    
    if (g.debug) {
        //cout << lander.pos[1] << " " << lz.pos[1] << endl;
        //Log("%f %f\n", lander.pos[1], lz.pos[1]);
        ggprint08(&r, 12, 0, "%f %f", lander.pos[1], lz.pos[1]);
    }

}






