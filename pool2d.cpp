// A pool-like game PoC with box2d, public domain
// Tested with Box2D 2.3.0.

#define FRICTION 0.99f
#define RESTITUTION 0.99f
#define LINEAR_DAMPING 0.5f
#define ANGULAR_DAMPING 0.99f

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <time.h>

#define FREEGLUT_STATIC

#include <GL/glut.h>
#ifdef FREEGLUT
#include <GL/freeglut_ext.h>
#endif

#include <Box2D/Box2D.h>

#pragma comment(lib, "box2d.lib")

#define WIDTH 800
#define HEIGHT 600

static GLuint texName[3];
GLubyte texture[256][256][4];

float zoom, rotx, roty, rotz, ofsx, ofsy;
int g_lastx = 0;
int g_lasty = 0;
unsigned char Buttons[3] = { 0 };

int fullscreen = 0;
int timerrate = 20;

const int w = 20;
const int h = 20;

float znear = 1.0f;
float zfar = 3000.0f;

int g_width = WIDTH;
int g_height = HEIGHT;

int debug = 0;

class DebugDraw:public b2Draw {
public:
    void DrawPolygon(const b2Vec2 * vertices, int32 vertexCount,
                     const b2Color & color);
    void DrawSolidPolygon(const b2Vec2 * vertices, int32 vertexCount,
                          const b2Color & color);
    void DrawCircle(const b2Vec2 & center, float32 radius,
                    const b2Color & color);
    void DrawSolidCircle(const b2Vec2 & center, float32 radius,
                         const b2Vec2 & axis, const b2Color & color);
    void DrawSegment(const b2Vec2 & p1, const b2Vec2 & p2,
                     const b2Color & color);
    void DrawTransform(const b2Transform & xf);
    void DrawPoint(const b2Vec2 & p, float32 size, const b2Color & color);
    void DrawString(int x, int y, const char *string, ...);
    void DrawAABB(b2AABB * aabb, const b2Color & color);
};


b2Vec2 g_mousePos;
b2World *g_world;
b2AABB worldAABB;
DebugDraw g_debugDraw;
b2MouseJoint *g_mouseJoint;
b2Body *g_selBody;
b2Body *g_groundBody;

b2Vec2 ConvertScreenToWorld(int32 mouseX, int32 mouseY)
{
    GLdouble modelview[16];
    GLdouble projection[16];
    GLint viewport[4];

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);

    GLdouble sx = mouseX;
    GLdouble sy = viewport[3] - mouseY;

    GLdouble x1, y1, z1;
    GLdouble x2, y2, z2;

    gluUnProject(sx, sy, 0.0, modelview, projection, viewport, &x1, &y1, &z1);
    gluUnProject(sx, sy, 1.0, modelview, projection, viewport, &x2, &y2, &z2);

    GLdouble t = z1 / (z1 - z2);
    float px = float (x1 + (x2 - x1) * t);
    float py = float (y1 + (y2 - y1) * t);

    return b2Vec2(px, py);
}

void DebugDraw::DrawPolygon(const b2Vec2 * vertices, int32 vertexCount,
                            const b2Color & color)
{
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINE_LOOP);
    for (int32 i = 0; i < vertexCount; ++i)
    {
        glVertex2f(vertices[i].x, vertices[i].y);
    }
    glEnd();
}

void DebugDraw::DrawSolidPolygon(const b2Vec2 * vertices, int32 vertexCount,
                                 const b2Color & color)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.5f * color.r, 0.5f * color.g, 0.5f * color.b, 0.5f);
    glBegin(GL_TRIANGLE_FAN);
    for (int32 i = 0; i < vertexCount; ++i)
    {
        glVertex2f(vertices[i].x, vertices[i].y);
    }
    glEnd();
    glDisable(GL_BLEND);

    glColor4f(color.r, color.g, color.b, 1.0f);
    glBegin(GL_LINE_LOOP);
    for (int32 i = 0; i < vertexCount; ++i)
    {
        glVertex2f(vertices[i].x, vertices[i].y);
    }
    glEnd();

#ifdef TEXTURED_POLY
    glBindTexture(GL_TEXTURE_2D, texName[0]);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLE_FAN);
    for (int32 i = 0; i < vertexCount; ++i)
    {
        glTexCoord2f(vertices[i].x, vertices[i].y);

        glVertex2f(vertices[i].x, vertices[i].y);
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
#endif

}

void DebugDraw::DrawCircle(const b2Vec2 & center, float32 radius,
                           const b2Color & color)
{
    const float32 k_segments = 16.0f;
    const float32 k_increment = 2.0f * b2_pi / k_segments;
    float32 theta = 0.0f;
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINE_LOOP);
    for (int32 i = 0; i < k_segments; ++i)
    {
        b2Vec2 v = center + radius * b2Vec2(cosf(theta), sinf(theta));
        glVertex2f(v.x, v.y);
        theta += k_increment;
    }
    glEnd();
}

void DebugDraw::DrawSolidCircle(const b2Vec2 & center, float32 radius,
                                const b2Vec2 & axis, const b2Color & color)
{
    const float32 k_segments = 16.0f;
    const float32 k_increment = 2.0f * b2_pi / k_segments;
    float32 theta = 0.0f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.5f * color.r, 0.5f * color.g, 0.5f * color.b, 0.5f);
    glBegin(GL_TRIANGLE_FAN);
    for (int32 i = 0; i < k_segments; ++i)
    {
        b2Vec2 v = center + radius * b2Vec2(cosf(theta), sinf(theta));
        glVertex2f(v.x, v.y);
        theta += k_increment;
    }
    glEnd();
    glDisable(GL_BLEND);

    theta = 0.0f;
    glColor4f(color.r, color.g, color.b, 1.0f);
    glBegin(GL_LINE_LOOP);
    for (int32 i = 0; i < k_segments; ++i)
    {
        b2Vec2 v = center + radius * b2Vec2(cosf(theta), sinf(theta));
        glVertex2f(v.x, v.y);
        theta += k_increment;
    }
    glEnd();

    b2Vec2 p = center + radius * axis;
    glBegin(GL_LINES);
    glVertex2f(center.x, center.y);
    glVertex2f(p.x, p.y);
    glEnd();
}

void DebugDraw::DrawSegment(const b2Vec2 & p1, const b2Vec2 & p2,
                            const b2Color & color)
{
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINES);
    glVertex2f(p1.x, p1.y);
    glVertex2f(p2.x, p2.y);
    glEnd();
}

void DebugDraw::DrawTransform(const b2Transform & xf)
{
    b2Vec2 p1 = xf.p, p2;
    const float32 k_axisScale = 0.4f;
    glBegin(GL_LINES);

    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(p1.x, p1.y);
    p2 = p1 + k_axisScale * xf.q.GetXAxis();
    glVertex2f(p2.x, p2.y);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(p1.x, p1.y);
    p2 = p1 + k_axisScale * xf.q.GetYAxis();
    glVertex2f(p2.x, p2.y);

    glEnd();
}

void DebugDraw::DrawPoint(const b2Vec2 & p, float32 size, const b2Color & color)
{
    glPointSize(size);
    glBegin(GL_POINTS);
    glColor3f(color.r, color.g, color.b);
    glVertex2f(p.x, p.y);
    glEnd();
    glPointSize(1.0f);
}

void DebugDraw::DrawString(int x, int y, const char *string, ...)
{
    char buffer[128];

    va_list arg;
    va_start(arg, string);
    vsprintf(buffer, string, arg);
    va_end(arg);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    int w = g_width;
    int h = g_height;
    gluOrtho2D(0, w, h, 0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.9f, 0.6f, 0.6f);
    glRasterPos2i(x, y);
    int32 length = (int32) strlen(buffer);

    for (int32 i = 0; i < length; ++i)
    {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, buffer[i]);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void DebugDraw::DrawAABB(b2AABB * aabb, const b2Color & c)
{
    glColor3f(c.r, c.g, c.b);
    glBegin(GL_LINE_LOOP);
    glVertex2f(aabb->lowerBound.x, aabb->lowerBound.y);
    glVertex2f(aabb->upperBound.x, aabb->lowerBound.y);
    glVertex2f(aabb->upperBound.x, aabb->upperBound.y);
    glVertex2f(aabb->lowerBound.x, aabb->upperBound.y);
    glEnd();
}

void togglefullscreen()
{
    if (fullscreen)
    {
        glutPositionWindow((glutGet(GLUT_SCREEN_WIDTH) - WIDTH) / 2,
                           (glutGet(GLUT_SCREEN_HEIGHT) - HEIGHT) / 2);
        glutReshapeWindow(WIDTH, HEIGHT);
    }
    else
    {
        glutFullScreen();
    }

    glutPostRedisplay();

    fullscreen = !fullscreen;
}

void cleartransform()
{
    zoom = 30.0f;
    rotx = 0.0f;
    roty = 0.0f;
    rotz = 0.0f;
    ofsx = 0.0f;
    ofsy = 0.0f;
}

float randx()
{
    return (float)((rand() % w) - w / 2) * 0.85f;
}

float randy()
{
    return (float)((rand() % h) - h / 2) * 0.85f;
}

void put_cell(int *buf, int w, int h, int x, int y, int cw, int ch, int color)
{
    for (int i = 0; i < ch; i++)
    {
        for (int j = 0; j < cw; j++)
        {
            int tx = (x + j) % w;
            int ty = (y + i) % h;

            buf[ty * w + tx] = color;
        }
    }
}

void gen_checker_texture(int *buf, int w, int h)
{
    int cw = 32;
    int ch = 32;

    for (int i = 0; i < h; i += cw * 2)
    {
        for (int j = 0; j < w; j += ch * 2)
        {
            int a = 0xff, r, g, b;
            r = rand();
            g = rand();
            b = rand();
            int color = (a << 24) | (r << 16) | (g << 8) | b;
            put_cell(buf, w, h, i, j, cw, ch, color);
        }
    }
}


void init()
{
    cleartransform();

    b2Vec2 gravity;
    gravity.Set(0.0f, 0.0f);
    g_mouseJoint = NULL;

    g_world = new b2World(gravity);

//    g_destructionListener.test = this;
//    g_world->SetDestructionListener(&g_destructionListener);
//    g_world->SetContactListener(this);
    g_world->SetDebugDraw(&g_debugDraw);
    b2BodyDef bodyDef;
    g_groundBody = g_world->CreateBody(&bodyDef);

// static walls

    float bbox[] = {
        w / 2, 1, 0, -h / 2,
        w / 2, 1, 0, h / 2,
        1, h / 2, -w / 2, 0,
        1, h / 2, w / 2, 0,
        2, 1, 0, 0
    };

    int count = 5;
    int t = 4;

    for (int i = 0; i < count; ++i)
    {
        float w = bbox[i * t + 0];
        float h = bbox[i * t + 1];
        float x = bbox[i * t + 2];
        float y = bbox[i * t + 3];

        b2PolygonShape sd;
        sd.SetAsBox(w, h);

        b2FixtureDef fd;
        fd.shape = &sd;

        b2BodyDef bd;
        bd.position.Set(x, y);

        b2Body *body = g_world->CreateBody(&bd);
        body->CreateFixture(&fd);
    }

// balls

    {
        b2CircleShape shape;

        b2FixtureDef fd;
        fd.shape = &shape;

        fd.restitution = RESTITUTION;
        fd.friction = FRICTION;

        for (int i = 0; i < 10; ++i)
        {
            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.linearDamping = LINEAR_DAMPING;
            bd.angularDamping = FLT_MAX;
            bd.bullet = true;   // enable CCD
            bd.position.Set(randx(), randy());

            b2Body *body = g_world->CreateBody(&bd);

            bool big = rand() % 2 ? true : false;
            shape.m_radius = big ? 1.0f : 0.5f;
            fd.density = big ? 3.0f : 1.0f;

            body->CreateFixture(&fd);
        }
    }

// rectangle blocks
    {
        b2PolygonShape sd;

        b2FixtureDef fd;

        fd.density = 1.0f;      //won't rotate without a density
        fd.shape = &sd;
        fd.friction = FRICTION;

        sd.SetAsBox(1.0f, 2.0f);

        {
            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.linearDamping = LINEAR_DAMPING;
            bd.angularDamping = ANGULAR_DAMPING;
            bd.position.Set(randx(), randy());
            b2Body *body = g_world->CreateBody(&bd);
            body->CreateFixture(&fd);
        }
    }

// triangle blocks
    {
        b2Vec2 vertices[3];
        vertices[0].Set(0.0f, 0.0f);
        vertices[1].Set(3.0f, 0.0f);
        vertices[2].Set(0.0f, 3.0f);

        b2PolygonShape sd;
        sd.Set(vertices, 3);

        b2FixtureDef fd;
        fd.density = 1.0f;
        fd.shape = &sd;

        {
            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.position.Set(randx(), randy());
            bd.linearDamping = LINEAR_DAMPING;
            bd.angularDamping = ANGULAR_DAMPING;
            bd.angle = (float)rand();

            b2Body *body = g_world->CreateBody(&bd);
            body->CreateFixture(&fd);
        }
    }


    gen_checker_texture((int *)texture, 256, 256);
    glBindTexture(GL_TEXTURE_2D, texName[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, &texture[0][0][0]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}

void timer(int value)
{
    g_world->Step(0.05f, 1, 3);
    glutTimerFunc(timerrate, timer, 1);
    glutPostRedisplay();
}

void drawgrid()
{
    int gw = 20;
    int gh = 20;

    glColor3f(0.25f, 0.25f, 0.25f);

    glBegin(GL_LINES);

    for (int i = -gw / 2; i <= gw / 2; ++i)
    {
        glVertex3f(i*1.0f, -gw / 2.0f, 0.0f);
        glVertex3f(i*1.0f, gw / 2.0f, 0.0f);
    }

    for (int j = -gh / 2; j <= gh / 2; ++j)
    {
        glVertex3f(gh / 2.0f, j*1.0f, 0.0f);
        glVertex3f(-gh / 2.0f, j*1.0f, 0.0f);
    }

    glEnd();
}

void display()
{
    glViewport(0, 0, g_width, g_height);
    glMatrixMode(GL_PROJECTION);

    glLoadIdentity();

    //glOrtho(0, w, h, 0, -1, 1);

    gluPerspective(45, (float)g_width / g_height, znear, zfar);

    glTranslatef(ofsx, ofsy, -zoom);

    glRotatef(rotx, 1, 0, 0);
    glRotatef(roty, 0, 1, 0);
    glRotatef(rotz, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_DEPTH_TEST);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_LIGHTING);

    drawgrid();

    if (!debug)
        g_debugDraw.SetFlags(0 | b2Draw::e_shapeBit | b2Draw::e_jointBit);
    else
        g_debugDraw.SetFlags(0 | b2Draw::e_shapeBit | b2Draw::
                             e_jointBit | b2Draw::e_aabbBit | b2Draw::
                             e_pairBit | b2Draw::e_centerOfMassBit);

//        b2Draw::e_coreShapeBit |
//b2Draw::e_obbBit |
//    g_world->Step(0, 0);    //debug draw

    g_world->DrawDebugData();

    if (g_selBody && Buttons[0])
    {
        b2Vec2 pos = g_selBody->GetPosition();

        g_debugDraw.DrawSegment(g_mousePos, pos, b2Color(1, 1, 1));
    }

    glEnable(GL_DEPTH_TEST);

    glEnable(GL_LIGHTING);

    int x = 10;
    int y = 20;

    g_debugDraw.DrawString(x, y, "R - reset");
    y += 15;

    g_debugDraw.DrawString(x, y, "F - fullscreen");
    y += 15;

    g_debugDraw.DrawString(x, y, "D - debug");

    glutSwapBuffers();
}

void reshape(int w, int h)
{
    if (w == 0)
        h = 1;

    g_width = w;
    g_height = h;

    glutPostRedisplay();
}

class QueryCallback:public b2QueryCallback {
public:
    QueryCallback(const b2Vec2 & point) {
        g_point = point;
        g_fixture = NULL;
    } bool ReportFixture(b2Fixture * fixture) {
        b2Body *body = fixture->GetBody();
        if (body->GetType() == b2_dynamicBody)
        {
            bool inside = fixture->TestPoint(g_point);
            if (inside)
            {
                g_fixture = fixture;

                // We are done, terminate the query.
                return false;
            }
        }

        // Continue the query.
        return true;
    }

    b2Vec2 g_point;
    b2Fixture *g_fixture;
};

void mouseDown(b2Vec2 & p)
{
    g_mousePos = p;

    if (g_mouseJoint != NULL)
    {
        return;
    }

    // Make a small box.
    b2AABB aabb;
    b2Vec2 d;
    d.Set(0.001f, 0.001f);
    aabb.lowerBound = p - d;
    aabb.upperBound = p + d;

    // Query the world for overlapping shapes.
    QueryCallback callback(p);
    g_world->QueryAABB(&callback, aabb);

    if (callback.g_fixture)
    {
        b2Body *body = callback.g_fixture->GetBody();
        b2MouseJointDef md;
        md.bodyA = g_groundBody;
        md.bodyB = body;
        md.target = p;
        md.maxForce = 1000.0f * body->GetMass();
        g_mouseJoint = (b2MouseJoint *) g_world->CreateJoint(&md);
        body->SetAwake(true);
        g_selBody = body;
    }
}

void mouseMove(b2Vec2 & p)
{

    g_mousePos = p;

    if (g_selBody)
    {
        //       g_selBody->WakeUp();

        b2Vec2 pos = g_selBody->GetPosition();

        float angle = atan2(p.y - pos.y, p.x - pos.x);
//        g_selBody->SetXForm(pos, angle);

    }
}

void mouseUp(b2Vec2 & p)
{
    if (g_selBody)
    {
        b2Vec2 pos = g_selBody->GetPosition();

        b2Vec2 force = (pos - p);

        force *= 300.0f * g_selBody->GetMass();

        g_selBody->ApplyForce(force, b2Vec2(0, 0), true);
    }

    g_selBody = NULL;

    if (g_mouseJoint)
    {
        g_world->DestroyJoint(g_mouseJoint);
        g_mouseJoint = NULL;
    }
}

void wheel(int wheel, int direction, int x, int y)
{

    if (direction > 0)
    {
        zoom = b2Max(0.9f * zoom, 0.01f);
    }
    else
    {
        zoom = b2Min(1.1f * zoom, 1000.0f);
    }

    glutPostRedisplay();
}

void motion(int x, int y)
{
    int diffx = x - g_lastx;
    int diffy = y - g_lasty;

    g_lastx = x;
    g_lasty = y;

    b2Vec2 p = ConvertScreenToWorld(x, y);
    mouseMove(p);

    if (g_selBody)
        return;

    if (Buttons[2])             //|| Buttons[0]
    {
        rotx += (float)0.5f *diffy;
        rotz += (float)0.5f *diffx;
    }
    else if (Buttons[1])
    {
        ofsx += (float)0.05f *diffx;
        ofsy -= (float)0.05f *diffy;
    }

    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y)
{
    if (key == 13 && glutGetModifiers() && GLUT_ACTIVE_ALT)
    {
        togglefullscreen();
        return;
    }

    switch (key)
    {
        case '+':
            zoom = b2Max(0.99f * zoom, 1.0f);
            break;

        case '-':
            zoom = b2Min(1.01f * zoom, 1000.0f);
            break;


        case 27:
        case 3:                //esc, ctrl+c
            exit(0);
            break;

        case 'f':
            togglefullscreen();
            break;

        case 'd':
            debug = !debug;
            break;

        case 13:
        case 'r':              //enter
            init();
            break;
    }
}

void special(int key, int x, int y)     // Create Special Function (required for arrow keys)
{
    switch (key)
    {
        case GLUT_KEY_F4:
            if (glutGetModifiers() && GLUT_ACTIVE_SHIFT)
                exit(0);
            break;
    }
}

void mouse(int b, int s, int x, int y)
{
    g_lastx = x;
    g_lasty = y;

    b2Vec2 p = ConvertScreenToWorld(x, y);

    switch (b)
    {
        case GLUT_LEFT_BUTTON:
            Buttons[0] = ((GLUT_DOWN == s) ? 1 : 0);
            if (s == GLUT_DOWN)
                mouseDown(p);
            else
                mouseUp(p);
            break;
        case GLUT_MIDDLE_BUTTON:
            Buttons[1] = ((GLUT_DOWN == s) ? 1 : 0);
            break;

        case GLUT_RIGHT_BUTTON:
            Buttons[2] = ((GLUT_DOWN == s) ? 1 : 0);
            break;

    }

    glutPostRedisplay();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition((glutGet(GLUT_SCREEN_WIDTH) - WIDTH) / 2,
                           (glutGet(GLUT_SCREEN_HEIGHT) - HEIGHT) / 2);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow(argv[0]);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

#ifdef FREEGLUT
    glutMouseWheelFunc(wheel);
#endif

    srand((int)time(0));

    init();

    glutTimerFunc(timerrate, timer, 1);

    glutMainLoop();
    return 0;
}
