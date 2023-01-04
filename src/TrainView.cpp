/************************************************************************
     File:        TrainView.cpp

     Author:     
                  Michael Gleicher, gleicher@cs.wisc.edu

     Modifier
                  Yu-Chi Lai, yu-chi@cs.wisc.edu
     
     Comment:     
						The TrainView is the window that actually shows the 
						train. Its a
						GL display canvas (Fl_Gl_Window).  It is held within 
						a TrainWindow
						that is the outer window with all the widgets. 
						The TrainView needs 
						to be aware of the window - since it might need to 
						check the widgets to see how to draw

	  Note:        we need to have pointers to this, but maybe not know 
						about it (beware circular references)

     Platform:    Visio Studio.Net 2003/2005

*************************************************************************/
/*
todo：
 3. 地板
 (done)4. 打光
 (done)7. arclength 參數化
 8. 輪子
 (done)9. 重力
 10. 煙
 11. 非平坦地形
*/

// opencv
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include "time.h"
#include <iostream>
#include <fstream>
#include <Fl/fl.h>

// we will need OpenGL, and OpenGL needs windows.h
#include <windows.h>
//#include "GL/gl.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "GL/glu.h"

#include "TrainView.H"
#include "TrainWindow.H"
#include "Utilities/3DUtils.H"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace cv;


// load model 
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "model.h"
#include "shader.h"
#include"filesystem.h"

#define EXAMPLE_SOLUTION_TRACK

#ifdef EXAMPLE_SOLUTION
#	include "TrainExample/TrainExample.H"
#endif
// curve function
// state 1,2,3 = linear, cardinal, B-Spline 
// pos 1/0 = pos
Pnt3f  curve_function(Pnt3f G1, Pnt3f G2, Pnt3f G3, Pnt3f G4, float t, int state, bool pos) {
	//做GMT
	float G[3][4] = {
		{G1.x, G2.x, G3.x, G4.x},
		{G1.y, G2.y, G3.y, G4.y},
		{G1.z, G2.z, G3.z, G4.z},
	};
	float M_cardinal[4][4] = {
	{-0.5f, 1.0f, -0.5f, 0.0f},
	{1.5f, -2.5f, 0.0f, 1.0f},
	{-1.5f, 2.0f, 0.5f, 0.0f},
	{0.5f, -0.5f, 0.0f, 0.0f}
	};
	float M_bspline[4][4] = {
	{-0.167f ,	0.5f, -0.5f, 0.167f},
	{0.5f, -1.0f, 0.0f, 0.667f},
	{-0.5f, 0.5f, 0.5f, 0.167f},
	{0.167f, 0.0f, 0.0f, 0.0f}
	};
	float T[4][1] = {
	{t * t * t},
	{t * t},
	{t},
	{1}
	};
	/*else {
		float T[4][1] = {
		{t * t },
		{t },
		{1},
		{0}
		};
	}*/
	
	float G_M[4][4] = {
		{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}
	};
	float G_M_T[3][1] = {
		{0},{0},{0},
	};
	//G*M
	if (state == 2) {
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; j++) {
				for (int k = 0; k < 4; k++) {
					G_M[i][j] += G[i][k] * M_cardinal[k][j];
				}
			}
		}
	}
	else if (state == 3) {
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; j++) {
				for (int k = 0; k < 4; k++) {
					G_M[i][j] += G[i][k] * M_bspline[k][j];
				}
			}
		}
	}

	//GM*T
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 1; j++) {
			for (int k = 0; k < 4; k++) {
				G_M_T[i][j] += G_M[i][k] *T[k][j];
			}
		}
	}
	return Pnt3f(G_M_T[0][0], G_M_T[1][0], G_M_T[2][0]);
}

//************************************************************************
//
// * Constructor to set up the GL window
//========================================================================
TrainView::
TrainView(int x, int y, int w, int h, const char* l) 
	: Fl_Gl_Window(x,y,w,h,l)
//========================================================================
{
	mode( FL_RGB|FL_ALPHA|FL_DOUBLE | FL_STENCIL );

	resetArcball();
}

//************************************************************************
//
// * Reset the camera to look at the world
//========================================================================
void TrainView::
resetArcball()
//========================================================================
{
	// Set up the camera to look at the world
	// these parameters might seem magical, and they kindof are
	// a little trial and error goes a long way
	arcball.setup(this, 40, 250, .2f, .4f, 0);
}

//************************************************************************
//
// * FlTk Event handler for the window
//########################################################################
// TODO: 
//       if you want to make the train respond to other events 
//       (like key presses), you might want to hack this.
//########################################################################
//========================================================================
int TrainView::handle(int event)
{
	// see if the ArcBall will handle the event - if it does, 
	// then we're done
	// note: the arcball only gets the event if we're in world view
	if (tw->worldCam->value())
		if (arcball.handle(event))
			return 1;

	// remember what button was used
	static int last_push;

	switch (event) {
		// Mouse button being pushed event
	case FL_PUSH:
		last_push = Fl::event_button();
		// if the left button be pushed is left mouse button
		if (last_push == FL_LEFT_MOUSE) {
			doPick();
			damage(1);
			return 1;
		};
		break;

		// Mouse button release event
	case FL_RELEASE: // button release
		damage(1);
		last_push = 0;
		return 1;

		// Mouse button drag event
	case FL_DRAG:

		// Compute the new control point position
		if ((last_push == FL_LEFT_MOUSE) && (selectedCube >= 0)) {
			ControlPoint* cp = &m_pTrack->points[selectedCube];

			double r1x, r1y, r1z, r2x, r2y, r2z;
			getMouseLine(r1x, r1y, r1z, r2x, r2y, r2z);

			double rx, ry, rz;
			mousePoleGo(r1x, r1y, r1z, r2x, r2y, r2z,
				static_cast<double>(cp->pos.x),
				static_cast<double>(cp->pos.y),
				static_cast<double>(cp->pos.z),
				rx, ry, rz,
				(Fl::event_state() & FL_CTRL) != 0);

			cp->pos.x = (float)rx;
			cp->pos.y = (float)ry;
			cp->pos.z = (float)rz;
			damage(1);
		}
		break;

		// in order to get keyboard events, we need to accept focus
	case FL_FOCUS:
		return 1;

		// every time the mouse enters this window, aggressively take focus
	case FL_ENTER:
		focus(this);
		break;

	case FL_KEYBOARD:
		int k = Fl::event_key();
		int ks = Fl::event_state();
		if (k == 'p') {
			// Print out the selected control point information
			if (selectedCube >= 0)
				printf("Selected(%d) (%g %g %g) (%g %g %g)\n",
					selectedCube,
					m_pTrack->points[selectedCube].pos.x,
					m_pTrack->points[selectedCube].pos.y,
					m_pTrack->points[selectedCube].pos.z,
					m_pTrack->points[selectedCube].orient.x,
					m_pTrack->points[selectedCube].orient.y,
					m_pTrack->points[selectedCube].orient.z);
			else
				printf("Nothing Selected\n");

			return 1;
		};
		break;
	}

	return Fl_Gl_Window::handle(event);
}

//************************************************************************
//
// * this is the code that actually draws the window
//   it puts a lot of the work into other routines to simplify things
//========================================================================
void TrainView::draw()
{

	//*********************************************************************
	//
	// * Set up basic opengl informaiton
	//
	//**********************************************************************
	//initialized glad
	if (gladLoadGL())
	{
		//initiailize VAO, VBO, Shader...


		//initMonitor();
		//if (!this->fbos)
		//	this->fbos = new WaterFrameBuffers();

		initSkyboxShader();
		initUBO();
		initModel();
		initParticle();
		initParticle2D();
		initModelTree();
		initModelTerrain();
		initModelChair();
		initFerris();

	}
	else
		throw std::runtime_error("Could not initialize GLAD!");

	// Set up the view port
	glViewport(0,0,w(),h());

	// clear the window, be sure to clear the Z-Buffer too
	glClearColor(0,0,.3f,0);		// background should be blue

	// we need to clear out the stencil buffer since we'll use
	// it for shadows
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH);

	// Blayne prefers GL_DIFFUSE
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	// prepare for projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();		// put the code to set up matrices here

	//######################################################################
	// TODO: 
	// you might want to set the lighting up differently. if you do, 
	// we need to set up the lights AFTER setting up the projection
	//######################################################################
	// enable the lighting
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	// top view only needs one light
	if (tw->topCam->value()) {
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
	} else {
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
	}

	//*********************************************************************
	//
	// * set the light parameters
	//
	//**********************************************************************
	GLfloat lightPosition1[]	= {0,1,1,0}; // {50, 200.0, 50, 1.0};
	GLfloat lightPosition2[]	= {1, 0, 0, 0};
	GLfloat lightPosition3[]	= {0, -1, 0, 0};
	GLfloat yellowLight[]		= {0.5f, 0.5f, .1f, 1.0};
	GLfloat whiteLight[]			= {1.0f, 1.0f, 1.0f, 1.0};
	GLfloat blueLight[]			= {.1f,.1f,.3f,1.0};
	GLfloat grayLight[]			= {.3f, .3f, .3f, 1.0};

	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
	glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

	glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

	glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);


	float yellowAmbientDiffuse[] = { 0.0f,0.0f, 1.0f, 0.3f };
	float position[] = { 0.0f, 100.0f, 0.0f, 1.0f };

	//glEnable(GL_LIGHT1);
	//glLightfv(GL_LIGHT1, GL_AMBIENT, yellowAmbientDiffuse);
	//glLightfv(GL_LIGHT1, GL_POSITION, position);


	//*********************************************************************
	// now draw the ground plane
	//*********************************************************************
	// set to opengl fixed pipeline(use opengl 1.x draw function)
	glUseProgram(0);

	setupFloor();

	glDisable(GL_LIGHTING);
	//drawFloor(400,8);


	//*********************************************************************
	// now draw the object and we need to do it twice
	// once for real, and then once for shadows
	//*********************************************************************

	glEnable(GL_LIGHTING);
	setupObjects();
	drawStuff();

	// this time drawing is for shadows (except for top view)
	if (!tw->topCam->value()) {
		setupShadows();
		drawStuff(true);
		unsetupShadows();
	}

	setUBO();
	glBindBufferRange(
		GL_UNIFORM_BUFFER, /*binding point*/0, this->commom_matrices->ubo, 0, this->commom_matrices->size);

	//fbos->bindReflectionFrameBuffer();
	drawSkybox();
	drawModel();
	drawParticle();
	drawParticle2D();
	drawModelTree(glm::vec3(100.0f + (float)tw->treeX->value(), -50.0f + (float)tw->treeY->value(), 50.0f + (float)tw->treeZ->value()), (float)tw->treeScale->value());
	drawModelTree(glm::vec3(-50.0f + (float)tw->treeX->value(), -50.0f + (float)tw->treeY->value(), -50.0f + (float)tw->treeZ->value()), (float)tw->treeScale->value());
	drawModelTree(glm::vec3(50.0f + (float)tw->treeX->value(), -50.0f + (float)tw->treeY->value(), -100.0f + (float)tw->treeZ->value()), (float)tw->treeScale->value());
	drawModelTree(glm::vec3(-100.0f + (float)tw->treeX->value(), -50.0f + (float)tw->treeY->value(), 50.0f + (float)tw->treeZ->value()), (float)tw->treeScale->value());
	drawModelTree(glm::vec3(20.0f + (float)tw->treeX->value(), -50.0f + (float)tw->treeY->value(), -30.0f + (float)tw->treeZ->value()), (float)tw->treeScale->value());
	drawModelTree(glm::vec3(-100.0f + (float)tw->treeX->value(), -50.0f + (float)tw->treeY->value(), -100.0f + (float)tw->treeZ->value()), (float)tw->treeScale->value());
	drawModelTerrain();
	drawModelChair(35);
	drawModelChair(55);
	drawModelChair(75);
	drawFerris();
	//fbos->unbindCurrentFrameBuffer();
	//drawMonitor(0);
	//drawCube();
	//LoadHeightMap();
	t_time += 0.01f;	
}

//************************************************************************
//
// * This sets up both the Projection and the ModelView matrices
//   HOWEVER: it doesn't clear the projection first (the caller handles
//   that) - its important for picking
//========================================================================
void TrainView::
setProjection()
//========================================================================
{
	// Compute the aspect ratio (we'll need it)
	float aspect = static_cast<float>(w()) / static_cast<float>(h());

	// Check whether we use the world camp
	if (tw->worldCam->value())
		arcball.setProjection(false);
	// Or we use the top cam
	else if (tw->topCam->value()) {
		float wi, he;
		if (aspect >= 1) {
			wi = 110;
			he = wi / aspect;
		} 
		else {
			he = 110;
			wi = he * aspect;
		}

		// Set up the top camera drop mode to be orthogonal and set
		// up proper projection matrix
		glMatrixMode(GL_PROJECTION);
		glOrtho(-wi, wi, -he, he, 200, -200);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(-90,1,0,0);
	} 
	// Or do the train view or other view here
	//####################################################################
	// TODO: 
	// put code for train view projection here!	
	//####################################################################
	else {
#ifdef EXAMPLE_SOLUTION
		trainCamView(this,aspect);
#endif
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		gluLookAt(
			point_list[point_index].x, point_list[point_index].y + 20.0f, point_list[point_index].z,
			point_list[(point_index + 1) % point_list.size()].x,
			point_list[(point_index + 1) % point_list.size()].y + 20.0f,
			point_list[(point_index + 1) % point_list.size()].z,
			//orient_list[point_index].x, orient_list[point_index].y, orient_list[point_index].z
			//0.0f, 1.0f, orient_list[point_index].z
			0.0f, 1.0f, 0.0f
			);
		// glLoadMatrixf
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(60, aspect, 1.0, 200.0);
	}
}

//************************************************************************
//
// * this draws all of the stuff in the world
//
//	NOTE: if you're drawing shadows, DO NOT set colors (otherwise, you get 
//       colored shadows). this gets called twice per draw 
//       -- once for the objects, once for the shadows
//########################################################################
// TODO: 
// if you have other objects in the world, make sure to draw them
//########################################################################
//========================================================================
void TrainView::drawStuff(bool doingShadows)
{
	// Draw the control points
	// don't draw the control points if you're driving 
	// (otherwise you get sea-sick as you drive through them)
	if (!tw->trainCam->value()) {
		for(size_t i=0; i<m_pTrack->points.size(); ++i) {
			if (!doingShadows) {
				if ( ((int) i) != selectedCube)
					glColor3ub(240, 60, 60);
				else
					glColor3ub(240, 240, 30);
			}
			m_pTrack->points[i].draw();
		}
	}
	// draw the track
	//####################################################################
	// TODO: 
	// call your own track drawing code
	//####################################################################

	
	
#ifdef EXAMPLE_SOLUTION_TRACK
	drawTrack(doingShadows);
#endif

	// draw the train
	//####################################################################
	// TODO: 
	//	call your own train drawing code
	//####################################################################
	for (int i = 0; i < carAmount; i++) {
		//if (i == carAmount-1)
		//	drawTrain(
		//		doingShadows,
		//		point_list[((point_index + carInterval * i) % point_list.size())],
		//		point_list[((point_index + carInterval * i + 1) % point_list.size())],
		//		orient_list[((point_index + carInterval * i) % point_list.size())],
		//		true
		//	);
		//else 
		//	drawTrain(
		//		doingShadows,
		//		point_list[((point_index + carInterval * i) % point_list.size())],
		//		point_list[((point_index + carInterval * i + 1) % point_list.size())],
		//		orient_list[((point_index + carInterval * i) % point_list.size())],
		//		false
		//	);
	}
}

// 
//************************************************************************
//
// * this tries to see which control point is under the mouse
//	  (for when the mouse is clicked)
//		it uses OpenGL picking - which is always a trick
//########################################################################
// TODO: 
//		if you want to pick things other than control points, or you
//		changed how control points are drawn, you might need to change this
//########################################################################
//========================================================================
void TrainView::
doPick()
//========================================================================
{
	// since we'll need to do some GL stuff so we make this window as 
	// active window
	make_current();		

	// where is the mouse?
	int mx = Fl::event_x(); 
	int my = Fl::event_y();

	// get the viewport - most reliable way to turn mouse coords into GL coords
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Set up the pick matrix on the stack - remember, FlTk is
	// upside down!
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();
	gluPickMatrix((double)mx, (double)(viewport[3]-my), 
						5, 5, viewport);

	// now set up the projection
	setProjection();

	// now draw the objects - but really only see what we hit
	GLuint buf[100];
	glSelectBuffer(100,buf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	// draw the cubes, loading the names as we go
	for(size_t i=0; i<m_pTrack->points.size(); ++i) {
		glLoadName((GLuint) (i+1));
		m_pTrack->points[i].draw();
	}

	// go back to drawing mode, and see how picking did
	int hits = glRenderMode(GL_RENDER);
	if (hits) {
		// warning; this just grabs the first object hit - if there
		// are multiple objects, you really want to pick the closest
		// one - see the OpenGL manual 
		// remember: we load names that are one more than the index
		selectedCube = buf[3]-1;
	} else // nothing hit, nothing selected
		selectedCube = -1;

	printf("Selected Cube %d\n",selectedCube);
}

void TrainView::
drawTrack(bool doingShadow) {
	nonArcCnt = 0;
	POINT_CNT_LEN = 0.0f;
	RAIL_CNT_LEN = 0.0f;
	DIS = 0.0f;
	point_list.clear();
	orient_list.clear();
	int state = tw->splineBrowser->value();
	int arcState = tw->arcLength->value();
	//auto state = tw->splineBrowser->value();
	// 目前選擇的線狀態 tw->splineBrowser->value(); 
	// state : Linear
	if (state == 1) {

		for (size_t i = 0; i < m_pTrack->points.size(); ++i) {
			// p1為當前點，p2為下個點。
			// pos 
			Pnt3f cp_pos_p1 = m_pTrack->points[i].pos;
			Pnt3f cp_pos_p2 = m_pTrack->points[(i + 1) % m_pTrack->points.size()].pos;
			// orient
			Pnt3f cp_orient_p1 = m_pTrack->points[i].orient;
			Pnt3f cp_orient_p2 = m_pTrack->points[(i + 1) % m_pTrack->points.size()].orient;

			// DIVIDE_LINE代表切成幾段
			float percent = 1.0f / DIVIDE_LINE;

			// t 是貝茲曲線的 t
			float t = 0;
			Pnt3f qt = (1 - t) * cp_pos_p1 + t * cp_pos_p2;

			for (size_t j = 0; j < DIVIDE_LINE; j++) {

				Pnt3f orient_t = (1 - t) * cp_orient_p1 + t * cp_orient_p2;
				Pnt3f qt0 = qt;
				t += percent;
				qt = (1 - t) * cp_pos_p1 + t * cp_pos_p2;
				Pnt3f qt1 = qt;
				orient_t.normalize();
				Pnt3f cross_t = (qt1 - qt0) * orient_t;
				cross_t.normalize();
				cross_t = cross_t * 2.5f;

				glLineWidth(3);
				glBegin(GL_LINES);
				glVertex3f(qt0.x + cross_t.x, qt0.y + cross_t.y, qt0.z + cross_t.z);
				glVertex3f(qt1.x + cross_t.x, qt1.y + cross_t.y, qt1.z + cross_t.z);
				glVertex3f(qt0.x - cross_t.x, qt0.y - cross_t.y, qt0.z - cross_t.z);
				glVertex3f(qt1.x - cross_t.x, qt1.y - cross_t.y, qt1.z - cross_t.z);
				glEnd();
				glLineWidth(1);

				DIS = (qt1 - qt0).distance();
				RAIL_CNT_LEN += DIS;
				POINT_CNT_LEN += DIS;
				if (RAIL_CNT_LEN > RAIL_INTERVAL_LEN) {
					Pnt3f cross_tt = cross_t * 2;
					qt1 = qt1 - qt0;
					qt1.x = qt0.x + qt1.x / DIS * RAIL_WIDTH;
					qt1.y = qt0.y + qt1.y / DIS * RAIL_WIDTH;
					qt1.z = qt0.z + qt1.z / DIS * RAIL_WIDTH;
					RAIL_CNT_LEN = 0;

					glBegin(GL_POLYGON);
					glVertex3f(qt0.x + cross_tt.x, qt0.y + cross_tt.y, qt0.z + cross_tt.z);
					glVertex3f(qt0.x - cross_tt.x, qt0.y - cross_tt.y, qt0.z - cross_tt.z);
					glVertex3f(qt1.x - cross_tt.x, qt1.y - cross_tt.y, qt1.z - cross_tt.z);
					glVertex3f(qt1.x + cross_tt.x, qt1.y + cross_tt.y, qt1.z + cross_tt.z);
					glEnd();
				}
				//std::cout << arcState << "\n";
				if (POINT_CNT_LEN > POINT_INTERVAL_LEN) {
					point_list.push_back(qt0);
					orient_list.push_back(cross_t);
					POINT_CNT_LEN = 0;
				}
			}
		}
	}
	// state : Cardinal Cubic
	else {
		for (size_t i = 0; i < m_pTrack->points.size(); ++i) {
			// p1為當前點，p2為下個點。
			// pos 位置
			Pnt3f cp_pos_p1 = m_pTrack->points[i].pos;
			Pnt3f cp_pos_p2 = m_pTrack->points[(i + 1) % m_pTrack->points.size()].pos;
			Pnt3f cp_pos_p3 = m_pTrack->points[(i + 2) % m_pTrack->points.size()].pos;
			Pnt3f cp_pos_p4 = m_pTrack->points[(i + 3) % m_pTrack->points.size()].pos;
			// orient 方向
			Pnt3f cp_orient_p1 = m_pTrack->points[i].orient;
			Pnt3f cp_orient_p2 = m_pTrack->points[(i + 1) % m_pTrack->points.size()].orient;
			Pnt3f cp_orient_p3 = m_pTrack->points[(i + 2) % m_pTrack->points.size()].orient;
			Pnt3f cp_orient_p4 = m_pTrack->points[(i + 3) % m_pTrack->points.size()].orient;

			// DIVIDE_LINE代表切成幾段
			float percent = 1.0f / DIVIDE_LINE;

			// t 是貝茲曲線的 t
			float t = 0;
			Pnt3f qt = curve_function(cp_pos_p1, cp_pos_p2, cp_pos_p3, cp_pos_p4, t, state, true);

			for (size_t j = 0; j < DIVIDE_LINE; j++) {

				Pnt3f orient_t = curve_function(cp_orient_p1, cp_orient_p2, cp_orient_p3, cp_orient_p4, t, state, true); ;
				Pnt3f qt0 = qt;
				t += percent;
				qt = curve_function(cp_pos_p1, cp_pos_p2, cp_pos_p3, cp_pos_p4, t, state, true);
				Pnt3f qt1 = qt;
				orient_t.normalize();
				Pnt3f cross_t = (qt1 - qt0) * orient_t;
				cross_t.normalize();
				cross_t = cross_t * 2.5f;

				glLineWidth(3);
				glBegin(GL_LINES);
				glVertex3f(qt0.x + cross_t.x, qt0.y + cross_t.y, qt0.z + cross_t.z);
				glVertex3f(qt1.x + cross_t.x, qt1.y + cross_t.y, qt1.z + cross_t.z);
				glVertex3f(qt0.x - cross_t.x, qt0.y - cross_t.y, qt0.z - cross_t.z);
				glVertex3f(qt1.x - cross_t.x, qt1.y - cross_t.y, qt1.z - cross_t.z);
				glEnd();
				glLineWidth(1);

				DIS = (qt1 - qt0).distance();
				RAIL_CNT_LEN += DIS;
				POINT_CNT_LEN += DIS;
				
				//draw rail
				if (RAIL_CNT_LEN > RAIL_INTERVAL_LEN ) {
					// record last 2 point 
					if (firstLock)
					{
						firstLock = false;
						lastTwoPoint = qt0;
					}
					else if (secondLock)
					{
						secondLock = false;
						lastPoint = qt0;
					}
					else
					{
						lastVec = (lastTwoPoint - lastPoint);
						nowVec = (lastPoint - qt0);
						//std::cout << lastVec.x <<"\t" << lastVec.y<<"\t" << lastVec.z << "\n";
						//std::cout << nowVec.x << "\t" << nowVec.y << "\t" << nowVec.z << "\n";
						////內積/距離相乘=cos theta
						//std::cout << (lastVec % nowVec) / (lastTwoPoint - lastPoint).distance() / (lastPoint - qt0).distance() << "\n";
						//std::cout << "===================\n";
						//RAIL_CNT_LEN = 0;
						//std::cout<< (lastVec % nowVec) << std::endl;

						if ((lastVec % nowVec) / (lastTwoPoint - lastPoint).distance() / (lastPoint - qt0).distance() < 0.9f|| adaptiveLock)
						{
							// set point of rail
							Pnt3f cross_tt = cross_t * 2;
							qt1 = qt1 - qt0;
							qt1.x = qt0.x + qt1.x / DIS * RAIL_WIDTH;
							qt1.y = qt0.y + qt1.y / DIS * RAIL_WIDTH;
							qt1.z = qt0.z + qt1.z / DIS * RAIL_WIDTH;

							glBegin(GL_POLYGON);
							glVertex3f(qt0.x + cross_tt.x, qt0.y + cross_tt.y, qt0.z + cross_tt.z);
							glVertex3f(qt0.x - cross_tt.x, qt0.y - cross_tt.y, qt0.z - cross_tt.z);
							glVertex3f(qt1.x - cross_tt.x, qt1.y - cross_tt.y, qt1.z - cross_tt.z);
							glVertex3f(qt1.x + cross_tt.x, qt1.y + cross_tt.y, qt1.z + cross_tt.z);
							glEnd();
							adaptiveLock = false;
							//std::cout << "Draw"<<cc << std::endl;
							cc++;
						}
						else
						{
							//std::cout << "noDraw" << cc << std::endl;
							adaptiveLock = true;
							cc++;
						}
						RAIL_CNT_LEN = 0;

						lastTwoPoint = lastPoint;
						lastPoint = qt0;
					}
					
					

					
				}
				//set point
				if (POINT_CNT_LEN > POINT_INTERVAL_LEN) {
					point_list.push_back(qt0);
					orient_list.push_back(cross_t);
					POINT_CNT_LEN = 0;
				}
			}
		}
	}
}
void TrainView::
drawTrain(bool doingShadow, Pnt3f pos0, Pnt3f pos1, Pnt3f ori, bool head) {
	auto TRAIN_HEIGHT = 10.0f;
	auto TRAIN_WIDTH = 5.0f;
	auto TRAIN_DEPTH = 20.0f;

	auto rotationVector = pos1 - pos0;

	auto WIDTH = ori.distance();
	ori.x = ori.x / WIDTH * TRAIN_WIDTH;
	ori.y = ori.y / WIDTH * TRAIN_WIDTH;
	ori.z = ori.z / WIDTH * TRAIN_WIDTH;


	pos1 = pos1 - pos0;
	auto DEPTH = pos1.distance();
	pos1.x = pos0.x + pos1.x / DEPTH * TRAIN_DEPTH;
	pos1.y = pos0.y + pos1.y / DEPTH * TRAIN_DEPTH;
	pos1.z = pos0.z + pos1.z / DEPTH * TRAIN_DEPTH;
	
	Pnt3f vectorA = (pos1 - pos0);
	Pnt3f vectorB = (ori);
	Pnt3f normal = vectorB * vectorA;
	normal.normalize();



	Pnt3f pos0_u;
	pos0_u.x = pos0.x - normal.x * TRAIN_HEIGHT;
	pos0_u.y = pos0.y + normal.y * TRAIN_HEIGHT;
	pos0_u.z = pos0.z - normal.z * TRAIN_HEIGHT;

	vectorA = (ori);
	vectorB = (pos0 - pos1);
	normal = vectorB * vectorA;
	normal.normalize();

	Pnt3f pos1_u;
	pos1_u.x = pos1.x - normal.x * TRAIN_HEIGHT;
	pos1_u.y = pos1.y + normal.y * TRAIN_HEIGHT;
	pos1_u.z = pos1.z - normal.z * TRAIN_HEIGHT;

	//Up
	glBegin(GL_QUADS);
	glNormal3f(0,1,0);
	if (!doingShadow)
		glColor3ub(255,255,255);
	glTranslatef(0.0f, 20.0f, 0.0f);
	glVertex3f(pos0_u.x + ori.x, pos0_u.y + ori.y, pos0_u.z + ori.z);
	glVertex3f(pos0_u.x - ori.x, pos0_u.y - ori.y, pos0_u.z - ori.z);
	glVertex3f(pos1_u.x - ori.x, pos1_u.y - ori.y, pos1_u.z - ori.z);
	glVertex3f(pos1_u.x + ori.x, pos1_u.y + ori.y, pos1_u.z + ori.z);
	glEnd();

	//Down
	glBegin(GL_QUADS);
	glNormal3f(0, -1, 0);
	if (!doingShadow)
		glColor3ub(100, 0, 0);
	glVertex3f(pos0.x + ori.x, pos0.y + ori.y, pos0.z + ori.z);
	glVertex3f(pos0.x - ori.x, pos0.y - ori.y, pos0.z - ori.z);
	glVertex3f(pos1.x - ori.x, pos1.y - ori.y, pos1.z - ori.z);
	glVertex3f(pos1.x + ori.x, pos1.y + ori.y , pos1.z + ori.z);
	glEnd();

	//Left
	glBegin(GL_QUADS);
	glNormal3f(-1, 0, 0);
	if (!doingShadow)
		glColor3ub(100, 0, 0);
	glVertex3f(pos0_u.x - ori.x, pos0_u.y - ori.y, pos0_u.z - ori.z);
	glVertex3f(pos1_u.x - ori.x, pos1_u.y - ori.y, pos1_u.z - ori.z);
	glVertex3f(pos1.x - ori.x, pos1.y - ori.y , pos1.z - ori.z);
	glVertex3f(pos0.x - ori.x, pos0.y - ori.y , pos0.z - ori.z);
	glEnd();

	//Right
	glBegin(GL_QUADS);
	glNormal3f(1, 0, 0);
	if (!doingShadow)
		glColor3ub(100, 0, 0);
	glVertex3f(pos0_u.x + ori.x, pos0_u.y + ori.y, pos0_u.z + ori.z);
	glVertex3f(pos1_u.x + ori.x, pos1_u.y + ori.y, pos1_u.z + ori.z);
	glVertex3f(pos1.x + ori.x, pos1.y + ori.y , pos1.z + ori.z);
	glVertex3f(pos0.x + ori.x, pos0.y + ori.y , pos0.z + ori.z);
	glEnd();

	//Front
	glBegin(GL_QUADS);
	glNormal3f(0, 0, -1);
	if (!doingShadow)
		glColor3ub(100, 0, 0);
	glVertex3f(pos1_u.x - ori.x, pos1_u.y - ori.y, pos1_u.z - ori.z);
	glVertex3f(pos1.x - ori.x, pos1.y - ori.y , pos1.z - ori.z);
	glVertex3f(pos1.x + ori.x, pos1.y + ori.y , pos1.z + ori.z);
	glVertex3f(pos1_u.x + ori.x, pos1_u.y + ori.y, pos1_u.z + ori.z);
	glEnd();

	//Back
	glBegin(GL_QUADS);
	glNormal3f(0, 0, 1);
	if (!doingShadow)
		glColor3ub(100, 0, 0);
	glVertex3f(pos0_u.x + ori.x, pos0_u.y + ori.y, pos0_u.z + ori.z);
	glVertex3f(pos0.x - ori.x, pos0.y - ori.y , pos0.z - ori.z);
	glVertex3f(pos0.x + ori.x, pos0.y + ori.y , pos0.z + ori.z);
	glVertex3f(pos0_u.x - ori.x, pos0_u.y - ori.y, pos0_u.z - ori.z);
	glEnd();


	
	glTranslatef(pos0.x , pos0.y +15, pos0.z );
	rotationMatrix = glm::eulerAngleXYZ(0.0f, 1.0f, 0.0f);
	
	//glMultMatrixf(glm::value_ptr(rotationMatrix));
	drawCube(Pnt3f(1, 2, 1), Pnt3f(1, -2, 1), Pnt3f(-1, -2, 1), Pnt3f(-1, 2, 1));
	drawCube(Pnt3f(2, 2, 1), Pnt3f(2, 4, 1), Pnt3f(-2, 4, 1), Pnt3f(-2, 2, 1));
	drawCube(Pnt3f(1, 0, 1), Pnt3f(1, 1, 1), Pnt3f(3, 2, 1), Pnt3f(3, 1, 1));
	drawCube(Pnt3f(-1, 0, 1), Pnt3f(-1, 1, 1), Pnt3f(-3, 2, 1), Pnt3f(-3, 1, 1));
	drawCube(Pnt3f(0, -2, 1), Pnt3f(-1, -2, 1), Pnt3f(-2, -4, 1), Pnt3f(-1, -4, 1));
	drawCube(Pnt3f(0, -2, 1), Pnt3f(1, -2, 1), Pnt3f(2, -4, 1), Pnt3f(1, -4, 1));

	rotationMatrix = glm::inverse(glm::eulerAngleXYZ(0.0f, 1.0f, 0.0f));

	glTranslatef(-pos0.x , -pos0.y -15, -pos0.z );
	
}


void TrainView::
drawWheel() {
}

void TrainView::drawTunnel() {
	
}

void TrainView::drawCube(Pnt3f p0, Pnt3f p1, Pnt3f p2, Pnt3f p3) {
	float x[4] = { p0.x, p1.x, p2.x, p3.x };
	float y[4] = { p0.y, p1.y, p2.y, p3.y };
	float depth = p0.z;

	glColor3ub(100, 100, 100);
	// front 
	glBegin(GL_QUADS);
	glNormal3f(0, 1, 0);
	for (int i = 0; i < 4; i++) 
		glVertex3f(x[i], y[i], depth);
	glEnd();
	// back
	glBegin(GL_QUADS);
	glNormal3f(0, -1, 0);
	for (int i = 0; i < 4; i++)
		glVertex3f(x[i], y[i], -depth);
	glEnd();
	//other
	glBegin(GL_QUADS);
	for (int i = 0; i < 4; i++) {
		glVertex3f(x[i], y[i], depth);
		glVertex3f(x[(i + 1) % 4], y[(i + 1) % 4], depth);
		glVertex3f(x[(i + 1) % 4], y[(i + 1) % 4], -depth);
		glVertex3f(x[i], y[i], -depth);
	}
	glEnd();
}


/* shader */

void TrainView::
initSkyboxShader()
{
	if (!skyboxShader) {
		this->skyboxShader = new Shader(PROJECT_DIR "/src/shaders/skyboxVS.glsl",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/skyboxFS.glsl");

		GLfloat skyboxVertices[] = {

			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			-1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f
		};
		// skybox VAO
		glGenVertexArrays(1, &skyboxVAO);
		glGenBuffers(1, &skyboxVBO);
		glBindVertexArray(skyboxVAO);
		glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

		// load textures
		vector<std::string> faces;
		faces.push_back(PROJECT_DIR"/Images/skybox/left.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/right.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/top.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/bottom.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/front.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/back.jpg");
		cubemapTexture = loadCubemap(faces);
	}

}
unsigned int TrainView::
loadCubemap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrComponents;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

void TrainView::
drawSkybox()
{
	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
	skyboxShader->Use();
	glUniform1i(glGetUniformLocation(this->skyboxShader->Program, "skybox"), 0);
	glm::mat4 view;
	glm::mat4 projection;

	glGetFloatv(GL_MODELVIEW_MATRIX, &view[0][0]);
	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);
	view = glm::mat4(glm::mat3(view)); // remove translation from the view matrix

	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(this->skyboxShader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(this->skyboxShader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);

	// skybox cube
	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDepthFunc(GL_LESS); // set depth function back to default
	glUseProgram(0);
}

void TrainView::initUBO()
{
	// for處理座標
	if (!this->commom_matrices)
		this->commom_matrices = new UBO();
	this->commom_matrices->size = 2 * sizeof(glm::mat4);
	glGenBuffers(1, &this->commom_matrices->ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, this->commom_matrices->ubo);
	glBufferData(GL_UNIFORM_BUFFER, this->commom_matrices->size, NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void TrainView::setUBO()
{
	float wdt = this->pixel_w();
	float hgt = this->pixel_h();

	glm::mat4 view_matrix;
	glGetFloatv(GL_MODELVIEW_MATRIX, &view_matrix[0][0]);
	//HMatrix view_matrix; 
	//this->arcball.getMatrix(view_matrix);

	glm::mat4 projection_matrix;
	glGetFloatv(GL_PROJECTION_MATRIX, &projection_matrix[0][0]);
	//projection_matrix = glm::perspective(glm::radians(this->arcball.getFoV()), (GLfloat)wdt / (GLfloat)hgt, 0.01f, 1000.0f);

	glBindBuffer(GL_UNIFORM_BUFFER, this->commom_matrices->ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), &projection_matrix[0][0]);
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), &view_matrix[0][0]);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
GLfloat* TrainView::
inverse(GLfloat* m)
{
	GLfloat* inv = new GLfloat[16]();
	float det;

	int i;

	inv[0] = m[5] * m[10] * m[15] -
		m[5] * m[11] * m[14] -
		m[9] * m[6] * m[15] +
		m[9] * m[7] * m[14] +
		m[13] * m[6] * m[11] -
		m[13] * m[7] * m[10];

	inv[4] = -m[4] * m[10] * m[15] +
		m[4] * m[11] * m[14] +
		m[8] * m[6] * m[15] -
		m[8] * m[7] * m[14] -
		m[12] * m[6] * m[11] +
		m[12] * m[7] * m[10];

	inv[8] = m[4] * m[9] * m[15] -
		m[4] * m[11] * m[13] -
		m[8] * m[5] * m[15] +
		m[8] * m[7] * m[13] +
		m[12] * m[5] * m[11] -
		m[12] * m[7] * m[9];

	inv[12] = -m[4] * m[9] * m[14] +
		m[4] * m[10] * m[13] +
		m[8] * m[5] * m[14] -
		m[8] * m[6] * m[13] -
		m[12] * m[5] * m[10] +
		m[12] * m[6] * m[9];

	inv[1] = -m[1] * m[10] * m[15] +
		m[1] * m[11] * m[14] +
		m[9] * m[2] * m[15] -
		m[9] * m[3] * m[14] -
		m[13] * m[2] * m[11] +
		m[13] * m[3] * m[10];

	inv[5] = m[0] * m[10] * m[15] -
		m[0] * m[11] * m[14] -
		m[8] * m[2] * m[15] +
		m[8] * m[3] * m[14] +
		m[12] * m[2] * m[11] -
		m[12] * m[3] * m[10];

	inv[9] = -m[0] * m[9] * m[15] +
		m[0] * m[11] * m[13] +
		m[8] * m[1] * m[15] -
		m[8] * m[3] * m[13] -
		m[12] * m[1] * m[11] +
		m[12] * m[3] * m[9];

	inv[13] = m[0] * m[9] * m[14] -
		m[0] * m[10] * m[13] -
		m[8] * m[1] * m[14] +
		m[8] * m[2] * m[13] +
		m[12] * m[1] * m[10] -
		m[12] * m[2] * m[9];

	inv[2] = m[1] * m[6] * m[15] -
		m[1] * m[7] * m[14] -
		m[5] * m[2] * m[15] +
		m[5] * m[3] * m[14] +
		m[13] * m[2] * m[7] -
		m[13] * m[3] * m[6];

	inv[6] = -m[0] * m[6] * m[15] +
		m[0] * m[7] * m[14] +
		m[4] * m[2] * m[15] -
		m[4] * m[3] * m[14] -
		m[12] * m[2] * m[7] +
		m[12] * m[3] * m[6];

	inv[10] = m[0] * m[5] * m[15] -
		m[0] * m[7] * m[13] -
		m[4] * m[1] * m[15] +
		m[4] * m[3] * m[13] +
		m[12] * m[1] * m[7] -
		m[12] * m[3] * m[5];

	inv[14] = -m[0] * m[5] * m[14] +
		m[0] * m[6] * m[13] +
		m[4] * m[1] * m[14] -
		m[4] * m[2] * m[13] -
		m[12] * m[1] * m[6] +
		m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] +
		m[1] * m[7] * m[10] +
		m[5] * m[2] * m[11] -
		m[5] * m[3] * m[10] -
		m[9] * m[2] * m[7] +
		m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] -
		m[0] * m[7] * m[10] -
		m[4] * m[2] * m[11] +
		m[4] * m[3] * m[10] +
		m[8] * m[2] * m[7] -
		m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] +
		m[0] * m[7] * m[9] +
		m[4] * m[1] * m[11] -
		m[4] * m[3] * m[9] -
		m[8] * m[1] * m[7] +
		m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] -
		m[0] * m[6] * m[9] -
		m[4] * m[1] * m[10] +
		m[4] * m[2] * m[9] +
		m[8] * m[1] * m[6] -
		m[8] * m[2] * m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	if (det == 0)
		return false;

	det = 1.0 / det;

	for (i = 0; i < 16; i++)
		inv[i] = inv[i] * det;

	return inv;
}
void TrainView::
initSineWater()
{
	if (!this->sineWaterShader)
		this->sineWaterShader = new
		Shader(
			PROJECT_DIR "/src/shaders/sineVS.glsl",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/sineFS.glsl");

	if (!this->sineWater) {

		float size = 0.01f;
		unsigned int width = 2.0f / size;
		unsigned int height = 2.0f / size;

		GLfloat* vertices = new GLfloat[width * height * 4 * 3]();
		GLfloat* texture_coordinate = new GLfloat[width * height * 4 * 2]();
		GLuint* element = new GLuint[width * height * 6]();

		/*
		 Vertices
		*2 -- *3
		 | \   |
		 |  \  |
		*1 -- *0
		*/
		for (int i = 0; i < width * height * 4 * 3; i += 12)
		{
			unsigned int h = i / 12 / width;
			unsigned int w = i / 12 % width;
			//point 0
			vertices[i] = w * size - 1.0f + size;
			vertices[i + 1] = WATER_HEIGHT;
			vertices[i + 2] = h * size - 1.0f + size;
			//point 1
			vertices[i + 3] = vertices[i] - size;
			vertices[i + 4] = WATER_HEIGHT;
			vertices[i + 5] = vertices[i + 2];
			//point 2
			vertices[i + 6] = vertices[i + 3];
			vertices[i + 7] = WATER_HEIGHT;
			vertices[i + 8] = vertices[i + 5] - size;
			//point 3
			vertices[i + 9] = vertices[i];
			vertices[i + 10] = WATER_HEIGHT;
			vertices[i + 11] = vertices[i + 8];
		}

		// texture
		for (int i = 0; i < height; i++)
		{
			for (int j = 0; j < width; j++)
			{
				//point 0
				texture_coordinate[i * width * 8 + j * 8 + 0] = (float)(j + 1) / width;
				texture_coordinate[i * width * 8 + j * 8 + 1] = (float)(i + 1) / height;
				//point 1
				texture_coordinate[i * width * 8 + j * 8 + 2] = (float)(j + 0) / width;
				texture_coordinate[i * width * 8 + j * 8 + 3] = (float)(i + 1) / height;
				//point 2
				texture_coordinate[i * width * 8 + j * 8 + 4] = (float)(j + 0) / width;
				texture_coordinate[i * width * 8 + j * 8 + 5] = (float)(i + 0) / height;
				//point 3												 
				texture_coordinate[i * width * 8 + j * 8 + 6] = (float)(j + 1) / width;
				texture_coordinate[i * width * 8 + j * 8 + 7] = (float)(i + 0) / height;
			}
		}

		// element
		/*
		element mesh 0
		   2
		 / |
		0--1

		element mesh 1
		1--0
		| /
		2
		*/
		for (int i = 0, j = 0; i < width * height * 6; i += 6, j += 4)
		{
			element[i] = j + 1;
			element[i + 1] = j;
			element[i + 2] = j + 3;

			element[i + 3] = element[i + 2];
			element[i + 4] = j + 2;
			element[i + 5] = element[i];
		}


		this->sineWater = new VAO;
		this->sineWater->element_amount = width * height * 6;
		glGenVertexArrays(1, &this->sineWater->vao);
		glGenBuffers(3, this->sineWater->vbo);
		glGenBuffers(1, &this->sineWater->ebo);

		glBindVertexArray(this->sineWater->vao);

		// Position attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->sineWater->vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, width * height * 4 * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(0);

		// Texture Coordinate attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->sineWater->vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, width * height * 4 * 2 * sizeof(GLfloat), texture_coordinate, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);

		//Element attribute
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->sineWater->ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, width * height * 6 * sizeof(GLuint), element, GL_STATIC_DRAW);

		// Unbind VAO
		glBindVertexArray(0);
	}
}
void TrainView::
drawSineWater()
{
	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
	glEnable(GL_BLEND);

	this->sineWaterShader->Use();

	glm::mat4 model_matrix = glm::mat4();
	model_matrix = glm::translate(model_matrix, this->source_pos);
	model_matrix = glm::scale(model_matrix, glm::vec3(300, 300, 300));

	glUniformMatrix4fv(
		glGetUniformLocation(this->sineWaterShader->Program, "u_model"), 1, GL_FALSE, &model_matrix[0][0]);

	glUniform3fv(
		glGetUniformLocation(this->sineWaterShader->Program, "u_color"), 1, &glm::vec3(0.0f, 1.0f, 0.0f)[0]);

	//高度
	glUniform1f(glGetUniformLocation(this->sineWaterShader->Program, ("amplitude")), tw->amplitude->value());
	//波長
	glUniform1f(glGetUniformLocation(this->sineWaterShader->Program, ("wavelength")), tw->waveLength->value());
	//時間
	glUniform1f(glGetUniformLocation(this->sineWaterShader->Program, ("time")), t_time);

	//skybox
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glUniform1i(glGetUniformLocation(this->sineWaterShader->Program, "skybox"), 0);

	//周圍的圍牆
	this->fbos->refractionTexture2D.bind(1);
	glUniform1i(glGetUniformLocation(this->sineWaterShader->Program, "refractionTexture"), 1);
	this->fbos->reflectionTexture2D.bind(2);
	glUniform1i(glGetUniformLocation(this->sineWaterShader->Program, "reflectionTexture"), 2);

	//取得相機座標位置
	GLfloat* view_matrix = new GLfloat[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, view_matrix);
	view_matrix = inverse(view_matrix);
	this->cameraPosition = glm::vec3(view_matrix[12], view_matrix[13], view_matrix[14]);
	glUniform3fv(glGetUniformLocation(this->sineWaterShader->Program, "cameraPos"), 1, &glm::vec3(cameraPosition)[0]);

	//bind VAO
	glBindVertexArray(this->sineWater->vao);

	glDrawElements(GL_TRIANGLES, this->sineWater->element_amount, GL_UNSIGNED_INT, 0);

	//unbind VAO
	glBindVertexArray(0);

	//unbind shader(switch to fixed pipeline)
	glUseProgram(0);

	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS); // set depth function back to default
}

void TrainView::
initMonitor()
{
	if (!this->monitorShader)
		this->monitorShader = new
		Shader(
			PROJECT_DIR "/src/shaders/monitorVS.glsl",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/monitorFS.glsl");

	if (!this->monitor) {
		GLfloat  vertices[] = {
			-1.0f, 0.0f,
			-1.0f, -1.0f,
			0.0f, -1.0f,
			0.0f, 0.0f
		};
		GLfloat  texture_coordinate[] = {
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
		};
		GLuint element[] = {
			0, 1, 2,
			2, 3, 0,
		};

		this->monitor = new VAO;
		this->monitor->element_amount = sizeof(element) / sizeof(GLuint);
		glGenVertexArrays(1, &this->monitor->vao);
		glGenBuffers(2, this->monitor->vbo);
		glGenBuffers(1, &this->monitor->ebo);

		glBindVertexArray(this->monitor->vao);

		// Position attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->monitor->vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(0);

		// Texture Coordinate attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->monitor->vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(texture_coordinate), texture_coordinate, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);

		//Element attribute
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->monitor->ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(element), element, GL_STATIC_DRAW);

		// Unbind VAO
		glBindVertexArray(0);
	}
}

void TrainView::
drawMonitor(int mode)
{
	//bind shader
	this->monitorShader->Use();

	if (mode == 1)
		this->fbos->reflectionTexture2D.bind(0);
	else
	{
		this->fbos->refractionTexture2D.bind(0);
	}
	glUniform1i(glGetUniformLocation(this->monitorShader->Program, "u_texture"), 0);

	//bind VAO
	glBindVertexArray(this->monitor->vao);

	glDrawElements(GL_TRIANGLES, this->monitor->element_amount, GL_UNSIGNED_INT, 0);

	//unbind VAO
	glBindVertexArray(0);

	//unbind shader(switch to fixed pipeline)
	glUseProgram(0);
}


void TrainView::
initParticle()
{
	

	if (!this->particleShader)
		this->particleShader = new
		Shader(
			PROJECT_DIR "/build/particle.vert",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/build/particle.frag");

	if (!this->particleVAO)
	{
		for (GLuint i = 0; i < nr_particles; ++i)
			particles.push_back(Particle());
		// elements * 3 rgb * 3 dots * 2 triangles
		GLfloat* vertices = new GLfloat[nr_particles * 3 * 3 *2 ]();
		GLfloat* texture_coordinate = new GLfloat[nr_particles * 2 * 3 * 2]();
		//GLuint* element = new GLuint[nr_particles]();
		//GLfloat* vertices = new GLfloat[nr_particles]();
		for (int i = 0; i < nr_particles * 3 * 3 * 2; i += 3*3*2)
		{
			vertices[i] = 0;
			vertices[i + 1] = 0;
			vertices[i + 2] = 0;
			vertices[i + 3] = 1;
			vertices[i + 4] = 1;
			vertices[i + 5] = 1;
			vertices[i + 6] = 0;
			vertices[i + 7] = 0;
			vertices[i + 8] = 1;
			vertices[i + 9] = 1;
			vertices[i + 10] = 0;
			vertices[i + 11] = 1;
			vertices[i + 12] = 1;
			vertices[i + 13] = 1;
			vertices[i + 14] = 0;
			vertices[i + 15] = 0;
			vertices[i + 16] = 1;
			vertices[i + 17] = 0;
		}
			
		//GLfloat vertices[] = {
		//	0,0,0,1,1,0,0,1,1,1,1,0
		//};
		for (int i = 0; i < nr_particles * 2 * 3 * 2; i++)
			texture_coordinate[i] = 0.0f;
		//for (int i = 0; i < nr_particles; i++)
		//	element[i] = 0.0f;
		
		this->particleVAO = new VAO;
		this->particleVAO->element_amount = nr_particles;
		glGenVertexArrays(1, &this->particleVAO->vao);
		glGenBuffers(3, this->particleVAO->vbo);
		glBindVertexArray(this->particleVAO->vao);

		// Position attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->particleVAO->vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, nr_particles * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(0);

		// Texture Coordinate attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->particleVAO->vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, nr_particles * 2 * sizeof(GLfloat), texture_coordinate, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);

		////Element attribute
		//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->particleVAO->ebo);
		//glBufferData(GL_ELEMENT_ARRAY_BUFFER, nr_particles * sizeof(GLuint), element, GL_STATIC_DRAW);

		// Unbind VAO
		glBindVertexArray(0);
	}


}


void TrainView::
drawParticle()
{
	glm::vec3 offset = glm::vec3(0.0f, 0.0f, 0.0f);
	GLfloat dt = 0.1f;
	GLuint nr_new_particles = 2;
	// Add new particles
	for (GLuint i = 0; i < nr_new_particles; ++i)
	{
		int unusedParticle = FirstUnusedParticle();
		RespawnParticle(particles[unusedParticle], offset);
	}
	// Update all particles
	for (GLuint i = 0; i < nr_particles; ++i)
	{
		Particle& p = particles[i];
		p.Life -= dt; // reduce life
		if (p.Life > 0.0f)
		{   // particle is alive, thus update
			p.Position -= p.Velocity * dt;
			p.Color.a -= dt * 2.5;
		}
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	particleShader->Use();

	glm::mat4 view;
	glm::mat4 projection;

	glGetFloatv(GL_MODELVIEW_MATRIX, &view[0][0]);
	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(this->particleShader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(this->particleShader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);

	for (Particle particle : particles)
	{
		if (particle.Life > 0.0f)
		{
			//cout << "draw\n";
			glUniform3fv(glGetUniformLocation(this->particleShader->Program, ("offset")), 1, &particle.Position[0]);
			glUniform4fv(glGetUniformLocation(this->particleShader->Program, ("color")), 1, &particle.Color[0]);
			
			// from learnOpengl
			//particleShader.SetVector2f("offset", particle.Position);
			//particleShader.SetVector4f("color", particle.Color);
			//particleTexture.Bind();
			glBindVertexArray(this->particleVAO->vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			//glDrawElements(GL_TRIANGLES, this->sineWater->element_amount, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
		}
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

GLuint TrainView::
FirstUnusedParticle()
{
	// Search from last used particle, this will usually return almost instantly
	for (GLuint i = lastUsedParticle; i < nr_particles; ++i) {
		if (particles[i].Life <= 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	// Otherwise, do a linear search
	for (GLuint i = 0; i < lastUsedParticle; ++i) {
		if (particles[i].Life <= 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	// Override first particle if all others are alive
	lastUsedParticle = 0;
	return 0;
}

void TrainView::
RespawnParticle(Particle& particle, glm::vec3 offset)
{
	GLfloat random = ((rand() % 100) - 50) / 10.0f;
	GLfloat rColor = 0.5 + ((rand() % 100) / 100.0f);
	particle.Position = glm::vec3(point_list[point_index].x, point_list[point_index].y, point_list[point_index].z) +random + offset;
	//particle.Position = glm::vec3(0.0f + random, 0.0f + random, 0.0f + random)  + offset;
	particle.Color = glm::vec4(rColor, rColor, rColor, 1.0f);
	particle.Life = tw->particleLife->value();
	particle.Velocity = vec3(1.0f, 1.0f, 1.0f) * 0.1f;
}

void TrainView::
initParticle2D()
{


	if (!this->particleShader2D)
		this->particleShader2D = new
		Shader(
			PROJECT_DIR "/build/particle2D.vert",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/build/particle2D.frag");

	if (!this->particleVAO2D)
	{
		for (GLuint i = 0; i < nr_particles2D; ++i)
			particles2D.push_back(Particle2D());
		// elements * 3 rgb * 2 dots * 2 triangles
		GLfloat* vertices = new GLfloat[nr_particles2D * 3 * 2 * 2]();
		GLfloat* texture_coordinate = new GLfloat[nr_particles2D * 2 * 2 * 2]();
		//GLuint* element = new GLuint[nr_particles]();
		//GLfloat* vertices = new GLfloat[nr_particles]();
		float rainWidth = 3.0f;
		float rainHeight = 1.0f;
		for (int i = 0; i < nr_particles2D * 3 * 2 * 2; i += 3 * 2 * 2)
		{
			vertices[i] = -rainWidth;
			vertices[i + 1] = rainHeight;
			vertices[i + 2] = rainWidth;
			vertices[i + 3] = rainHeight;
			vertices[i + 4] = -rainWidth;
			vertices[i + 5] = -rainHeight;
			vertices[i + 6] = -rainWidth;
			vertices[i + 7] = -rainHeight;
			vertices[i + 8] = rainWidth;
			vertices[i + 9] = rainHeight;
			vertices[i + 10] = rainWidth;
			vertices[i + 11] = -rainHeight;
		}

		for (int i = 0; i < nr_particles2D * 2 * 2 * 2; i++)
			texture_coordinate[i] = 0.0f;
		//for (int i = 0; i < nr_particles; i++)
		//	element[i] = 0.0f;

		this->particleVAO2D = new VAO;
		this->particleVAO2D->element_amount = nr_particles2D;
		glGenVertexArrays(1, &this->particleVAO2D->vao);
		glGenBuffers(3, this->particleVAO2D->vbo);
		glBindVertexArray(this->particleVAO2D->vao);

		// Position attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->particleVAO2D->vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, nr_particles2D * 2 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(0);

		// Texture Coordinate attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->particleVAO2D->vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, nr_particles2D * 2 * sizeof(GLfloat), texture_coordinate, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);

		////Element attribute
		//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->particleVAO->ebo);
		//glBufferData(GL_ELEMENT_ARRAY_BUFFER, nr_particles * sizeof(GLuint), element, GL_STATIC_DRAW);

		// Unbind VAO
		glBindVertexArray(0);
	}


}


void TrainView::
drawParticle2D()
{
	glm::vec2 offset = glm::vec2(-1.0f);
	GLfloat dt = 0.1f;
	GLuint nr_new_particles = 2;
	// Add new particles
	for (GLuint i = 0; i < nr_new_particles; ++i)
	{
		int unusedParticle = FirstUnusedParticle2D();
		RespawnParticle2D(particles2D[unusedParticle], offset);
	}
	// Update all particles
	for (GLuint i = 0; i < nr_particles2D; ++i)
	{
		Particle2D& p = particles2D[i];
		p.Life -= dt; // reduce life
		if (p.Life > 0.0f)
		{   // particle is alive, thus update
			p.Position -= p.Velocity * dt;
			p.Color.a -= dt * 2.5;
		}
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	particleShader2D->Use();

	glm::mat4 view;
	glm::mat4 projection;

	glGetFloatv(GL_MODELVIEW_MATRIX, &view[0][0]);
	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(this->particleShader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(this->particleShader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);

	for (Particle2D particle : particles2D)
	{
		if (particle.Life > 0.0f)
		{
			//cout << "draw\n";
			glUniform2fv(glGetUniformLocation(this->particleShader->Program, ("offset")), 1, &particle.Position[0]);
			glUniform4fv(glGetUniformLocation(this->particleShader->Program, ("color")), 1, &particle.Color[0]);

			// from learnOpengl
			//particleShader.SetVector2f("offset", particle.Position);
			//particleShader.SetVector4f("color", particle.Color);
			//particleTexture.Bind();
			glBindVertexArray(this->particleVAO->vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			//glDrawElements(GL_TRIANGLES, this->sineWater->element_amount, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
		}
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

GLuint TrainView::
FirstUnusedParticle2D()
{
	// Search from last used particle, this will usually return almost instantly
	for (GLuint i = lastUsedParticle; i < nr_particles2D; ++i) {
		if (particles2D[i].Life <= 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	// Otherwise, do a linear search
	for (GLuint i = 0; i < lastUsedParticle; ++i) {
		if (particles2D[i].Life <= 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	// Override first particle if all others are alive
	lastUsedParticle = 0;
	return 0;
}

void TrainView::
RespawnParticle2D(Particle2D& particle, glm::vec2 offset)
{
	GLfloat random = ((rand() % 100) - 50) / 10.0f;
	GLfloat rColor = 0.5 + ((rand() % 100) / 100.0f);
	//particle.Position = glm::vec3(point_list[point_index].x, point_list[point_index].y, point_list[point_index].z) + random + offset;
	particle.Position = glm::vec2(0.0f) + random + offset;
	particle.Color = glm::vec4(rColor, rColor, rColor, 1.0f);

	particle.Life = 3.0f;
	//particle.Life = tw->particleLife2D->value();
	particle.Velocity = glm::vec2(1.0f) * 0.1f;
}

glm::mat4 TrainView::
calRotationXYZ(Pnt3f p0, Pnt3f p1, Pnt3f direction)
{
	Pnt3f vec = p1 - p0;
	Pnt3f newY = direction;
	newY.normalize();
	Pnt3f newZ = (newY * Pnt3f(0, 1, 0));
	newZ.normalize();

	if (direction.x == 0 && direction.z == 0)
	{
		if (direction.y < 0) // rotate 180 degrees
			vec = Pnt3f(-vec.x, -vec.y, vec.z);

		// else if direction.y >= 0, leave `vec` as it is.
	}
	else
	{
		Pnt3f newY = direction;
		newY.normalize();

		Pnt3f newZ = newY * Pnt3f(0, 1, 0);
		newZ.normalize();

		// code below will go here.
	}

	Pnt3f newX = newY * newZ;
	newX.normalize();

	glm::mat3 transform = glm::mat3(
		newX.x, newX.y, newX.z,
		newY.x, newY.y, newY.z, 
		newZ.x, newZ.y, newZ.z
		);

	return glm::mat4(transform);

	//// 往前向量
	//Pnt3f forwardVec = p1 - p0;
	//forwardVec.normalize();
	//// 往上向量
	//Pnt3f p0UpVec = Pnt3f(0, 1, 0);
	//// 前 * 上
	//Pnt3f forwardCrossP0 = forwardVec * p0UpVec;
	//forwardCrossP0.normalize();

	//// 前*上(右) * 前
	//Pnt3f p1UpVec = forwardCrossP0 * forwardVec;

	//glm::mat4 RT = glm::mat4(
	//	forwardVec,
	//	forwardCrossP0,
	//	p1UpVec
	//)

	//return ;

}

void TrainView::
LoadHeightMap()
{
	int width, height, nChannels;
	unsigned char* data = stbi_load(PROJECT_DIR"/Images/heightmap.png",
		&width, &height, &nChannels,
		0);
	std::vector<float> vertices;
	float yScale = 64.0f / 256.0f, yShift = 16.0f;  // apply a scale+shift to the height data
	for (unsigned int i = 0; i < height; i++)
	{
		for (unsigned int j = 0; j < width; j++)
		{
			// retrieve texel for (i,j) tex coord
			unsigned char* texel = data + (j + width * i) * nChannels;
			// raw height at coordinate
			unsigned char y = texel[0];

			// vertex
			vertices.push_back(-height / 2.0f + i);        // v.x
			vertices.push_back((int)y * yScale - yShift); // v.y
			vertices.push_back(-width / 2.0f + j);        // v.z
		}
	}
	stbi_image_free(data);

	std::vector<unsigned int> indices;
	for (unsigned int i = 0; i < height - 1; i++)       // for each row a.k.a. each strip
	{
		for (unsigned int j = 0; j < width; j++)      // for each column
		{
			for (unsigned int k = 0; k < 2; k++)      // for each side of the strip
			{
				indices.push_back(j + width * (i + k));
			}
		}
	}
	const unsigned int NUM_STRIPS = height - 1;
	const unsigned int NUM_VERTS_PER_STRIP = width * 2;

	

	GLuint terrainVAO, terrainVBO, terrainEBO;
	glGenVertexArrays(1, &terrainVAO);
	glBindVertexArray(terrainVAO);

	glGenBuffers(1, &terrainVBO);
	glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
	glBufferData(GL_ARRAY_BUFFER,
		vertices.size() * sizeof(float),       // size of vertices buffer
		&vertices[0],                          // pointer to first element
		GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &terrainEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		indices.size() * sizeof(unsigned int), // size of indices buffer
		&indices[0],                           // pointer to first element
		GL_STATIC_DRAW);

	glBindVertexArray(terrainVAO);
	// render the mesh triangle strip by triangle strip - each row at a time
	for (unsigned int strip = 0; strip < NUM_STRIPS; ++strip)
	{
		glDrawElements(GL_TRIANGLE_STRIP,   // primitive type
			NUM_VERTS_PER_STRIP, // number of indices to render
			GL_UNSIGNED_INT,     // index data type
			(void*)(sizeof(unsigned int)
				* NUM_VERTS_PER_STRIP
				* strip)); // offset to starting index
	}
	
}


void TrainView::
initModel()
{
	if (!modelShader) {
		this->modelShader = new Shader(PROJECT_DIR "/src/shaders/1.model_loading.vs",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/1.model_loading.fs");

		ourModel = new Model(PROJECT_DIR"/src/Thomas/Thomas.obj");
	}

}

void TrainView::
drawModel()
{
	// don't forget to enable shader before setting uniforms
	this->modelShader->Use();

	glm::mat4 model_matrix = glm::mat4();
	model_matrix = glm::translate(model_matrix, glm::vec3(point_list[point_index].x, point_list[point_index].y, point_list[point_index].z));

	//model_matrix = glm::translate(model_matrix, glm::vec3(0, -10, 0));

	// add rotate matrix
	model_matrix = model_matrix * calRotationXYZ(point_list[point_index], point_list[((point_index + 1) % point_list.size())], orient_list[point_index]);

	model_matrix = glm::scale(model_matrix, glm::vec3(5.0f));
	//model_matrix = glm::eulerAngleXYZ(0.0f, 10.0f, 0.0f) * model_matrix;
	model_matrix = glm::rotate(model_matrix, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	model_matrix = glm::rotate(model_matrix, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glUniformMatrix4fv(
		glGetUniformLocation(this->modelShader->Program, "model"), 1, GL_FALSE, &model_matrix[0][0]);

	//cout << ourModel->meshes.size();
	ourModel->Draw(*this->modelShader);

	glUseProgram(0);
}



void TrainView::
initModelTree()
{
	if (!modelTreeShader) {
		this->modelTreeShader = new Shader(PROJECT_DIR "/src/shaders/1.model_loading.vs",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/1.model_loading.fs");
		ourModelTree = new Model(PROJECT_DIR"/src/Palm_Tree/Palm_Tree.obj");
	}
}

void TrainView::
drawModelTree(glm::vec3 trans, float scale)
{
	// don't forget to enable shader before setting uniforms
	this->modelTreeShader->Use();

	glm::mat4 model_matrix = glm::mat4();
	
	model_matrix = glm::translate(model_matrix, trans);
	model_matrix = glm::scale(model_matrix, glm::vec3(scale));
	model_matrix = glm::rotate(model_matrix, (float)glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));

	glUniformMatrix4fv(
		glGetUniformLocation(this->modelTreeShader->Program, "model"), 1, GL_FALSE, &model_matrix[0][0]);

	//cout << ourModel->meshes.size();
	ourModelTree->Draw(*this->modelTreeShader);

	glUseProgram(0);
}


void TrainView::
initModelTerrain()
{
	if (!modelTerrainShader) {
		this->modelTerrainShader = new Shader(PROJECT_DIR "/src/shaders/1.model_loading.vs",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/1.model_loading.fs");

		ourModelTerrain = new Model(PROJECT_DIR"/src/Small Tropical Island/Small Tropical Island.obj");
	}
}

void TrainView::
drawModelTerrain()
{
	// don't forget to enable shader before setting uniforms
	this->modelTerrainShader->Use();

	glm::mat4 model_matrix = glm::mat4();

	model_matrix = glm::translate(model_matrix, glm::vec3(0.0f,-70.0f,0.0f));
	model_matrix = glm::scale(model_matrix, glm::vec3(0.8f, 0.5f, 0.8f));
	//model_matrix = glm::rotate(model_matrix, (float)glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));

	glUniformMatrix4fv(
		glGetUniformLocation(this->modelTerrainShader->Program, "model"), 1, GL_FALSE, &model_matrix[0][0]);

	//cout << ourModel->meshes.size();
	ourModelTerrain->Draw(*this->modelTerrainShader);

	glUseProgram(0);
}


void TrainView::
initModelChair()
{
	if (!modelChairShader) {
		this->modelChairShader = new Shader(PROJECT_DIR "/src/shaders/1.model_loading.vs",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/1.model_loading.fs");

		ourModelChair = new Model(PROJECT_DIR"/src/maps/office chair.obj");
	}
}

void TrainView::
drawModelChair(int interval)
{
	// don't forget to enable shader before setting uniforms
	this->modelChairShader->Use();

	// error hadling
	if ((point_list.size() - interval) < 0)
		return;

	glm::mat4 model_matrix = glm::mat4();
	model_matrix = glm::translate(model_matrix, glm::vec3(
		point_list[(point_list.size() - interval + point_index) % point_list.size()].x,
		point_list[(point_list.size() - interval + point_index) % point_list.size()].y - 5.0f,
		point_list[(point_list.size() - interval + point_index) % point_list.size()].z
	));
	// add rotate matrix
	model_matrix = glm::scale(model_matrix, glm::vec3(20.0f));
	model_matrix = model_matrix * calRotationXYZ(
		point_list[(point_list.size() - interval + point_index) % point_list.size()],
		point_list[(point_list.size() - interval + point_index + 1) % point_list.size()],
		orient_list[(point_list.size() - interval + point_index) % point_list.size()]
	);

	model_matrix = glm::rotate(model_matrix, (float)glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	model_matrix = glm::rotate(model_matrix, (float)glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	glUniformMatrix4fv(
		glGetUniformLocation(this->modelChairShader->Program, "model"), 1, GL_FALSE, &model_matrix[0][0]);

	//cout << ourModel->meshes.size();
	ourModelChair->Draw(*this->modelChairShader);

	glUseProgram(0);
}


#define M_PI 3.1414
#define WHEEL 1
#define CABIN 2
float toRad(float deg)
{
	return (deg * M_PI) / 180;
}
void TrainView::
initFerris()
{
	if (isInitFerris)
		return;
	glNewList(WHEEL, GL_COMPILE);
	glColor3f(0.8, 0, 0.0);
	// Center front
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, 15);
	for (float angle = 0; angle <= 360; angle += 0.1)
	{
		glVertex3f(5 * cos(toRad(angle)), 5 * sin(toRad(angle)), 15);
	}
	glEnd();
	// Center 
	glBegin(GL_QUAD_STRIP);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(5 * cos(toRad(angle)), 5 * sin(toRad(angle)), 15);
		glVertex3f(5 * cos(toRad(angle)), 5 * sin(toRad(angle)), -15);
	}
	glEnd();

	// Center back
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, -15);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(5 * cos(toRad(angle)), 5 * sin(toRad(angle)), -15);
	}
	glEnd();

	// Wheel structure front
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glBegin(GL_POLYGON);
		glVertex3f(30 * cos(toRad(angle)), 30 * sin(toRad(angle)), 4);
		glVertex3f(28 * cos(toRad(angle)), 28 * sin(toRad(angle)), 4);
		glVertex3f(28 * cos(toRad(angle + 0.2)), 28 * sin(toRad(angle + 0.2)), 4);
		glVertex3f(30 * cos(toRad(angle + 0.2)), 30 * sin(toRad(angle + 0.2)), 4);
		glEnd();
	}

	glBegin(GL_QUAD_STRIP);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(30 * cos(toRad(angle)), 30 * sin(toRad(angle)), 4);
		glVertex3f(30 * cos(toRad(angle)), 30 * sin(toRad(angle)), 3.5);
	}
	glEnd();

	glBegin(GL_QUAD_STRIP);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(28 * cos(toRad(angle)), 28 * sin(toRad(angle)), 4);
		glVertex3f(28 * cos(toRad(angle)), 28 * sin(toRad(angle)), 3.5);
	}
	glEnd();

	// Wheel structure front
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glBegin(GL_POLYGON);
		glVertex3f(30 * cos(toRad(angle)), 30 * sin(toRad(angle)), 3.5);
		glVertex3f(28 * cos(toRad(angle)), 28 * sin(toRad(angle)), 3.5);
		glVertex3f(28 * cos(toRad(angle + 0.2)), 28 * sin(toRad(angle + 0.2)), 3.5);
		glVertex3f(30 * cos(toRad(angle + 0.2)), 30 * sin(toRad(angle + 0.2)), 3.5);
		glEnd();
	}



	// Wheel structure back
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glBegin(GL_POLYGON);
		glVertex3f(30 * cos(toRad(angle)), 30 * sin(toRad(angle)), -4);
		glVertex3f(28 * cos(toRad(angle)), 28 * sin(toRad(angle)), -4);
		glVertex3f(28 * cos(toRad(angle + 0.2)), 28 * sin(toRad(angle + 0.2)), -4);
		glVertex3f(30 * cos(toRad(angle + 0.2)), 30 * sin(toRad(angle + 0.2)), -4);
		glEnd();
	}

	glBegin(GL_QUAD_STRIP);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(30 * cos(toRad(angle)), 30 * sin(toRad(angle)), -4);
		glVertex3f(30 * cos(toRad(angle)), 30 * sin(toRad(angle)), -3.5);
	}
	glEnd();

	glBegin(GL_QUAD_STRIP);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(28 * cos(toRad(angle)), 28 * sin(toRad(angle)), -4);
		glVertex3f(28 * cos(toRad(angle)), 28 * sin(toRad(angle)), -3.5);
	}
	glEnd();

	// Wheel structure back
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glBegin(GL_POLYGON);
		glVertex3f(30 * cos(toRad(angle)), 30 * sin(toRad(angle)), -3.5);
		glVertex3f(28 * cos(toRad(angle)), 28 * sin(toRad(angle)), -3.5);
		glVertex3f(28 * cos(toRad(angle + 0.2)), 28 * sin(toRad(angle + 0.2)), -3.5);
		glVertex3f(30 * cos(toRad(angle + 0.2)), 30 * sin(toRad(angle + 0.2)), -3.5);
		glEnd();
	}

	glEndList();

	// CAbin
	glNewList(CABIN, GL_COMPILE);
	glColor3f(0.5, 0, 0);
	// Bar
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, -4);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(0.5 * cos(toRad(angle)), 0.5 * sin(toRad(angle)), -4);
	}
	glEnd();

	glBegin(GL_QUAD_STRIP);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(0.5 * cos(toRad(angle)), 0.5 * sin(toRad(angle)), -3.5);
		glVertex3f(0.5 * cos(toRad(angle)), 0.5 * sin(toRad(angle)), 3.5);
	}
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, 4);
	for (float angle = 0; angle <= 360; angle += 0.1) {
		glVertex3f(0.5 * cos(toRad(angle)), 0.5 * sin(toRad(angle)), 4);
	}
	glEnd();


	// Cono
	glPushMatrix();
	glColor3f(0.95, 0.67, 0.06);
	glTranslatef(0, -5, 0);
	glRotatef(-90, 1, 0, 0);
	//glutSolidCone(2, 5, 50, 50);
	glPopMatrix();

	glEndList();
	isInitFerris = true;
}

void TrainView::
drawFerris()
{
	w_rot += w_speed;
	//cout << w_rot << "\n";
	glPushMatrix();
	glTranslatef(150, 0, 150);
	glPushMatrix();
	glColor3f(0.02, 0.22, 0.02);
	glTranslatef(0, -50, 0);
	glScalef(50, 2, 50);
	drawCube();
	glPopMatrix();

	glPushMatrix();
	glCallList(WHEEL);
	glPopMatrix();

	glPushMatrix();
	glRotatef(w_rot, 0, 0, 1);
	for (int i = 0; i < n_bars; i++) {
		c_radius = (360.0 / n_bars) * i;
		glPushMatrix();
		// Axis
		glPushMatrix();
		glColor3f(0.92, 0.12, 0.12);
		glRotatef(c_radius, 0, 0, 1);
		glRotatef(-11, 1, 0, 0);
		glTranslatef(0, 14.5, 10);
		glScalef(1, 28, 0.5);
		drawCube();
		glPopMatrix();

		glPushMatrix();
		glColor3f(0.92, 0.12, 0.12);
		glRotatef(c_radius, 0, 0, 1);
		glRotatef(11, 1, 0, 0);
		glTranslatef(0, 14.5, -10);
		glScalef(1, 28, 0.5);
		drawCube();
		glPopMatrix();

		glTranslatef(28 * cos(toRad(c_radius)), 28 * sin(toRad(c_radius)), 0);
		glRotatef(-w_rot, 0, 0, 1);
		glCallList(CABIN);
		glPopMatrix();
	}
	glPopMatrix();

	glPushMatrix();
	glColor3f(0.3, 0.3, 0.3);
	glRotatef(-30, 0, 0, 1);
	glTranslatef(0, -25, 12);
	glScalef(3, 40, 2);
	drawCube();
	glPopMatrix();

	glPushMatrix();
	glColor3f(0.3, 0.3, 0.3);
	glRotatef(30, 0, 0, 1);
	glTranslatef(0, -25, 12);
	glScalef(3, 40, 2);
	drawCube();
	glPopMatrix();

	glPushMatrix();
	glColor3f(0.3, 0.3, 0.3);
	glRotatef(-30, 0, 0, 1);
	glTranslatef(0, -25, -12);
	glScalef(3, 40, 2);
	drawCube();
	glPopMatrix();

	glPushMatrix();
	glColor3f(0.3, 0.3, 0.3);
	glRotatef(30, 0, 0, 1);
	glTranslatef(0, -25, -12);
	glScalef(3, 40, 2);
	drawCube();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(20, -30, 30);
	glColor3f(1, 0, 0);
	glPushMatrix();
	glRotatef(-90, 1, 0, 0);
	drawCube();
	glPopMatrix();
	glColor3f(1, 0.5, 0.5);
	drawCube();
	glPushMatrix();
	glTranslatef(0, -2, 0);
	glScalef(0.5, 4, 0.5);
	drawCube();
	glPopMatrix();
	
	glPushMatrix();
	glTranslatef(-0.9, -6, 0);
	glRotatef(-20, 0, 0, 1);
	glScalef(0.5, 5, 0.5);
	drawCube();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(0.9, -6, 0);
	glRotatef(20, 0, 0, 1);
	glScalef(0.5, 5, 0.5);
	drawCube();
	glPopMatrix();
	
	glPushMatrix();
	glTranslatef(-0.9, -2, 0);
	glRotatef(-120, 0, 0, 1);
	glScalef(0.5, 2, 0.5);
	drawCube();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(0.9, -2, 0);
	glRotatef(120, 0, 0, 1);
	glScalef(0.5, 2, 0.5);
	drawCube();
	glPopMatrix();
	glPopMatrix();
	glPopMatrix();
}

void TrainView::
drawCube()
{
	glPushMatrix();
	glBegin(GL_QUADS);                // Begin drawing the color cube with 6 quads
									  // Top face (y = 1.0f)
									  // Define vertices in counter-clockwise (CCW) order with normal pointing out
	glColor3f(0.0f, 1.0f, 0.0f);     // Green
	glVertex3f(1.0f, 1.0f, -1.0f);
	glVertex3f(-1.0f, 1.0f, -1.0f);
	glVertex3f(-1.0f, 1.0f, 1.0f);
	glVertex3f(1.0f, 1.0f, 1.0f);

	// Bottom face (y = -1.0f)
	glColor3f(1.0f, 0.5f, 0.0f);     // Orange
	glVertex3f(1.0f, -1.0f, 1.0f);
	glVertex3f(-1.0f, -1.0f, 1.0f);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(1.0f, -1.0f, -1.0f);

	// Front face  (z = 1.0f)
	glColor3f(1.0f, 0.0f, 0.0f);     // Red
	glVertex3f(1.0f, 1.0f, 1.0f);
	glVertex3f(-1.0f, 1.0f, 1.0f);
	glVertex3f(-1.0f, -1.0f, 1.0f);
	glVertex3f(1.0f, -1.0f, 1.0f);

	// Back face (z = -1.0f)
	glColor3f(1.0f, 1.0f, 0.0f);     // Yellow
	glVertex3f(1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f, 1.0f, -1.0f);
	glVertex3f(1.0f, 1.0f, -1.0f);

	// Left face (x = -1.0f)
	glColor3f(0.0f, 0.0f, 1.0f);     // Blue
	glVertex3f(-1.0f, 1.0f, 1.0f);
	glVertex3f(-1.0f, 1.0f, -1.0f);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f, -1.0f, 1.0f);

	// Right face (x = 1.0f)
	glColor3f(1.0f, 0.0f, 1.0f);     // Magenta
	glVertex3f(1.0f, 1.0f, -1.0f);
	glVertex3f(1.0f, 1.0f, 1.0f);
	glVertex3f(1.0f, -1.0f, 1.0f);
	glVertex3f(1.0f, -1.0f, -1.0f);
	glEnd();  // End of drawing color-cube

	glPopMatrix();
}