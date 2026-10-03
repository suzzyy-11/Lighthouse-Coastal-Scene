/* =====================================================================
   LIGHTHOUSE ISLAND - OpenGL / GLUT  (Code::Blocks 20.03 compatible)

   Scene: irregular tropical island with beach and shallow water,
   striped lighthouse (door, windows, balcony, light room, rotating beam),
   keeper's house with porch + lamp + chimney smoke, path, wooden dock
   with boat / crates / life ring, rescue station, campfire, palms, rocks,
   grass, bushes, flowers, beach details, seagulls, animated sea with
   shore foam, a ship that sails around the island, day/night cycle,
   storm mode (rain + lightning).

   CONTROLS
     D  day mode              N  night mode          A  auto day/night
     L  lighthouse beam       S  ship start/stop     W  waves start/stop
     R  storm mode            B  birds move on/off   F  campfire animation
     G  seagulls show/hide    H  hide/show HUD       SPACE pause
     Q/E storm stronger/weaker (while storm is on)
     1/2/3 camera: orbit / boat / keeper     mouse L-drag orbit, R-drag zoom
     +/- zoom   Z/X time -/+   K lightning   O fog   P shadows   M wire sea
     ESC exit
   ===================================================================== */

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define PI       3.14159265f
#define TWO_PI   6.2831853f
#define RAD2DEG(x) ((x) * 180.0f / PI)

/* ---------------------------------------------------------------- */
/* scene constants                                                  */
/* ---------------------------------------------------------------- */
#define ISL_R     9.0f      /* mean island radius                    */
#define PLATEAU   2.2f      /* flat top height of island             */
#define TOWER_H   7.0f      /* lighthouse tower height               */
#define LAMP_Y    (PLATEAU + TOWER_H + 0.62f)
#define BOAT_R    26.0f     /* ship orbit radius (far from island)   */
#define SEA_N     80        /* sea grid cells per side               */
#define SEA_HALF  40.0f     /* sea half size                         */
#define NW        4         /* number of waves                       */
#define NRAIN     1500
#define NSTARS    350
#define NCLOUD    9
#define NWAKE     90
#define HUT_X     1.85f     /* keeper's house position (on plateau)  */

#define DOCK_A    1.05f     /* direction (angle) of the dock         */
#define DOCK_R0   6.4f      /* where the dock starts (from centre)   */
#define DOCK_L    6.2f      /* dock length                           */
#define DECK_Y    0.62f     /* dock deck height                      */
#define FIRE_X    (-3.0f)   /* campfire position                     */
#define FIRE_Z    (-2.8f)
#define RESCUE_X  0.9f      /* rescue station position               */
#define RESCUE_Z  5.3f
#define RESCUE_ROT 35.0f
#define SIGN_X    (-1.4f)   /* warning sign position                 */
#define SIGN_Z    1.9f

#define NGULL       8
#define NSMOKE      26
#define SMOKE_LIFE  4.0f
#define NPATH       70
#define NTUFT       36
#define NBUSH       10
#define NFLOW       26

/* ---------------------------------------------------------------- */
/* global state                                                     */
/* ---------------------------------------------------------------- */
int   winW = 1100, winH = 700;
int   lastMs = 0;
float fps = 60.0f;
float simTime = 0.0f;        /* general animation time               */
float waveT = 0.0f;          /* wave time (stops with W)             */
float birdT = 0.0f;          /* bird time (stops with B)             */
float fireT = 0.0f;          /* flame time (stops with F)            */
float tod = 15.0f;           /* time of day in hours                 */
float todRate = 0.30f;       /* hours per real second                */
float beamAngle = 0.0f;
float storm = 0.0f;          /* 0 = calm, 1 = violent (smoothed)     */
float stormTarget = 0.0f;
float flash = 0.0f;          /* lightning flash intensity            */

int paused = 0, autoTime = 1, lightningOn = 1, fogOn = 1;
int wireSea = 0, hudOn = 1, shadowsOn = 1, beamOn = 1, hasStencil = 1;
int gullsOn = 1, stormMode = 0, shipOn = 1, wavesOn = 1;
int birdsOn = 1, fireAnim = 1;

int   camMode = 0;
float camYaw = 0.65f, camPitch = 0.32f, camDist = 36.0f;
int   lastX = 0, lastY = 0, dragBtn = -1;

/* environment (filled by updateEnvironment) */
float sunE = 1.0f, dayF = 1.0f, nightF = 0.0f;
float skyCol[3] = { 0.5f, 0.7f, 0.9f };
float ambCol[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
float sunDiff[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float lightDirv[3] = { 0.0f, 1.0f, 0.0f };
float sunDirW[3], moonDirW[3];
float beamDirv[2][3];

/* waves */
float wdx[NW] = { 1.0f, 0.7f, -0.4f, 0.2f };
float wdz[NW] = { 0.3f, 0.7f, 1.0f, -1.0f };
float wlen[NW] = { 14.0f, 8.0f, 5.0f, 3.0f };
float wamp[NW] = { 0.5f, 0.3f, 0.15f, 0.08f };
float wph[NW] = { 0.0f, 1.3f, 2.1f, 4.0f };
float wk[NW], wom[NW];

float seaH[SEA_N + 1][SEA_N + 1];
float seaGX[SEA_N + 1][SEA_N + 1];
float seaGZ[SEA_N + 1][SEA_N + 1];
float seaDamp[SEA_N + 1][SEA_N + 1];   /* wave damping near island   */
float seaShal[SEA_N + 1][SEA_N + 1];   /* 1 = shallow, 0 = deep      */

/* ship */
float boatTheta = 0.0f, boatOmega = 0.07f;
float boatX = 0, boatY = 0, boatZ = 0;
float boatYaw = 0, boatPitch = 0, boatRoll = 0, boatHX = 0, boatHZ = 1;
float wake[NWAKE][2];
int   wakeCount = 0, wakeHead = 0;
float wakeTimer = 0.0f;

/* particles / sky */
float rainP[NRAIN][3];
float starPos[NSTARS][3];
float cloudX[NCLOUD], cloudZ[NCLOUD], cloudS[NCLOUD];
float cloudOff[NCLOUD][5][3];
float bolt[12][3];
float boltLife = 0.0f, boltTimer = 3.0f;

/* seagulls and chimney smoke */
float gullR[NGULL], gullTh[NGULL], gullW[NGULL], gullY[NGULL], gullPh[NGULL];
float smokeAge[NSMOKE], smokeJit[NSMOKE][2];

/* island decoration data */
float pathPts[NPATH][2];
float tuftP[NTUFT][4];      /* x, z, height, rotation   */
float bushP[NBUSH][4];      /* x, z, size, shade        */
float flowP[NFLOW][4];      /* x, z, colour index, -    */

/* GL objects */
GLUquadricObj *quad;
GLuint texStripe, texGround, islandList;
int gShadow = 0;

#define TS 64
GLubyte stripeTex[TS][TS][3];
GLubyte groundTex[TS][TS][3];

/* palm trees: x, z, scale, rotation, lean */
static const float palmTab[4][5] = {
    { -3.8f,  3.2f, 1.00f,  20.0f, 14.0f },
    { -5.2f, -1.0f, 1.15f, 140.0f, 18.0f },
    {  3.6f, -3.4f, 0.90f, 260.0f, 12.0f },
    {  0.5f, -5.2f, 1.05f,  80.0f, 16.0f }
};

/* path control points (dock -> keeper's house -> lighthouse door) */
static const float pathCtl[6][2] = {
    { 3.0f, 5.2f }, { 3.9f, 3.6f }, { 3.0f, 2.1f },
    { 2.05f, 1.1f }, { 1.0f, 1.5f }, { 0.0f, 1.5f }
};

/* ---------------------------------------------------------------- */
/* helpers                                                          */
/* ---------------------------------------------------------------- */
static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float mixf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float frand(void)
{
    return (float)rand() / (float)RAND_MAX;
}

static float smoothf(float a, float b, float x)
{
    float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static void setCol(float r, float g, float b)
{
    if (!gShadow)
        glColor3f(r, g, b);
}

static void setSpec(float s, float shin)
{
    GLfloat sp[4];
    sp[0] = s; sp[1] = s; sp[2] = s; sp[3] = 1.0f;
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, sp);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shin);
}

/* unit cube centred at (x,y,z) scaled to (sx,sy,sz) */
static void drawBox(float x, float y, float z, float sx, float sy, float sz)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);
    glutSolidCube(1.0);
    glPopMatrix();
}

/* cylinder standing upright from (x,y,z), length h */
static void drawPole(float x, float y, float z, float r, float h)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(quad, r, r, h, 8, 1);
    glPopMatrix();
}

/* centred stroke-font text in the x-y plane, scale sc */
static void strokeText(const char *s, float sc)
{
    const char *p;
    float len = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char *)s);

    glPushMatrix();
    glScalef(sc, sc, sc);
    glTranslatef(-len * 0.5f, 0.0f, 0.0f);
    for (p = s; *p; ++p)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    glPopMatrix();
}

/* ---------------------------------------------------------------- */
/* island height field (irregular outline)                          */
/* ---------------------------------------------------------------- */
static float islandR(float a)
{
    return ISL_R * (1.0f + 0.10f * sinf(2.0f * a + 0.6f)
                         + 0.07f * sinf(3.0f * a + 2.0f)
                         + 0.04f * sinf(5.0f * a + 1.0f));
}

static float groundHPolar(float r, float a)
{
    float u = r / islandR(a);
    float t;

    if (u < 0.30f) return PLATEAU;
    t = (u - 0.30f) / 0.70f;
    return PLATEAU - (PLATEAU + 0.8f) * powf(t, 1.3f);
}

/* ground height at world position (x,z) */
static float groundH(float x, float z)
{
    return groundHPolar(sqrtf(x * x + z * z), atan2f(z, x));
}

/* wave damping near the island (calmer water at the beach) */
static float islandDamp(float x, float z)
{
    float u = sqrtf(x * x + z * z) / islandR(atan2f(z, x));
    return mixf(0.35f, 1.0f, smoothf(1.0f, 2.0f, u));
}

/* ---------------------------------------------------------------- */
/* waves                                                            */
/* ---------------------------------------------------------------- */
static float waveScale(void)
{
    return 0.45f + 0.9f * storm;
}

static void initWaves(void)
{
    int i;
    for (i = 0; i < NW; i++)
    {
        float l = sqrtf(wdx[i] * wdx[i] + wdz[i] * wdz[i]);
        wdx[i] /= l;
        wdz[i] /= l;
        wk[i] = TWO_PI / wlen[i];
        wom[i] = sqrtf(9.8f * wk[i]);
    }
}

/* height at (x,z,t) and slope in x and z */
static float waveH(float x, float z, float t, float *gx, float *gz)
{
    int i;
    float h = 0.0f, ax = 0.0f, az = 0.0f;
    float S = waveScale();

    for (i = 0; i < NW; i++)
    {
        float ph = wk[i] * (wdx[i] * x + wdz[i] * z) - wom[i] * t + wph[i];
        float a = wamp[i] * S;
        float c = a * wk[i] * cosf(ph);
        h += a * sinf(ph);
        ax += c * wdx[i];
        az += c * wdz[i];
    }
    if (gx) *gx = ax;
    if (gz) *gz = az;
    return h;
}

/* water surface height as drawn (edge fade + island damping) */
static float seaSurface(float x, float z)
{
    float edge = fabsf(x) > fabsf(z) ? fabsf(x) : fabsf(z);
    float fall = 1.0f - smoothf(28.0f, 40.0f, edge);

    return waveH(x, z, waveT, 0, 0) * fall * islandDamp(x, z);
}

static void initSeaMaps(void)
{
    int i, j;
    float cell = 2.0f * SEA_HALF / SEA_N;

    for (i = 0; i <= SEA_N; i++)
    {
        for (j = 0; j <= SEA_N; j++)
        {
            float x = -SEA_HALF + i * cell;
            float z = -SEA_HALF + j * cell;
            float u = sqrtf(x * x + z * z) / islandR(atan2f(z, x));
            seaDamp[i][j] = mixf(0.35f, 1.0f, smoothf(1.0f, 2.0f, u));
            seaShal[i][j] = 1.0f - smoothf(0.9f, 1.7f, u);
        }
    }
}

static void updateSea(void)
{
    int i, j;
    float cell = 2.0f * SEA_HALF / SEA_N;

    for (i = 0; i <= SEA_N; i++)
    {
        for (j = 0; j <= SEA_N; j++)
        {
            float x = -SEA_HALF + i * cell;
            float z = -SEA_HALF + j * cell;
            float edge = fabsf(x) > fabsf(z) ? fabsf(x) : fabsf(z);
            float fall = (1.0f - smoothf(28.0f, 40.0f, edge)) * seaDamp[i][j];
            float gx, gz;
            float h = waveH(x, z, waveT, &gx, &gz);
            seaH[i][j] = h * fall;
            seaGX[i][j] = gx * fall;
            seaGZ[i][j] = gz * fall;
        }
    }
}

/* deep water = dark blue, shallow water = turquoise */
static void seaColor(float nrm, float shal, float *r, float *g, float *b)
{
    float foam = smoothf(0.78f, 1.0f, nrm) * (0.35f + 0.65f * storm) * (1.0f - 0.7f * shal);
    float dim = 1.0f - 0.40f * storm;
    *r = mixf(0.03f, 0.28f, nrm) * dim;
    *g = mixf(0.20f, 0.60f, nrm) * dim;
    *b = mixf(0.36f, 0.72f, nrm) * dim;
    *r = mixf(*r, 0.20f * dim, shal * 0.85f);
    *g = mixf(*g, 0.78f * dim, shal * 0.85f);
    *b = mixf(*b, 0.74f * dim, shal * 0.85f);
    *r = mixf(*r, 0.92f, foam);
    *g = mixf(*g, 0.96f, foam);
    *b = mixf(*b, 1.00f, foam);
}

static void seaVertex(int i, int j)
{
    float cell = 2.0f * SEA_HALF / SEA_N;
    float x = -SEA_HALF + i * cell;
    float z = -SEA_HALF + j * cell;
    float h = seaH[i][j];
    float nx = -seaGX[i][j], ny = 1.0f, nz = -seaGZ[i][j];
    float l = sqrtf(nx * nx + ny * ny + nz * nz);
    float nrm = clampf(h / (1.03f * waveScale()) * 0.5f + 0.5f, 0.0f, 1.0f);
    float r, g, b;

    seaColor(nrm, seaShal[i][j], &r, &g, &b);
    glColor4f(r, g, b, mixf(0.93f, 0.50f, seaShal[i][j]));
    glNormal3f(nx / l, ny / l, nz / l);
    glVertex3f(x, h, z);
}

static void drawSea(void)
{
    int i, j;
    float r, g, b;

    /* far flat deep water (below the island base) so the horizon has no edge */
    seaColor(0.5f, 0.0f, &r, &g, &b);
    setSpec(0.2f, 20.0f);
    glColor4f(r, g, b, 1.0f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex3f(-600.0f, -1.5f, -600.0f);
    glVertex3f(-600.0f, -1.5f, 600.0f);
    glVertex3f(600.0f, -1.5f, 600.0f);
    glVertex3f(600.0f, -1.5f, -600.0f);
    glEnd();

    setSpec(0.8f, 90.0f);
    glPolygonMode(GL_FRONT_AND_BACK, wireSea ? GL_LINE : GL_FILL);
    for (i = 0; i < SEA_N; i++)
    {
        glBegin(GL_TRIANGLE_STRIP);
        for (j = 0; j <= SEA_N; j++)
        {
            seaVertex(i, j);
            seaVertex(i + 1, j);
        }
        glEnd();
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

/* animated foam lines that run towards the shore */
static void drawShoreFoam(void)
{
    int k, i;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glLineWidth(2.0f);

    for (k = 0; k < 3; k++)
    {
        float ph = waveT * 0.18f + k / 3.0f;
        float u, al;

        ph -= floorf(ph);
        u = 1.12f - 0.26f * ph;
        al = sinf(PI * ph) * 0.75f * (0.35f + 0.65f * dayF);
        glColor4f(1.0f, 1.0f, 1.0f, al);
        glBegin(GL_LINE_STRIP);
        for (i = 0; i <= 120; i++)
        {
            float a = TWO_PI * i / 120.0f;
            float uu = u + 0.015f * sinf(a * 9.0f + waveT * 2.0f);
            float rr = islandR(a) * uu;
            float x = rr * cosf(a), z = rr * sinf(a);
            glVertex3f(x, seaSurface(x, z) + 0.05f, z);
        }
        glEnd();
    }

    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

/* ---------------------------------------------------------------- */
/* procedural textures                                              */
/* ---------------------------------------------------------------- */
static void makeTextures(void)
{
    int r, c;

    for (r = 0; r < TS; r++)
    {
        for (c = 0; c < TS; c++)
        {
            int band = (r / 16) % 2;
            int n = rand() % 14 - 7;
            int v;
            float blotch;

            if (band == 0)
            {
                stripeTex[r][c][0] = (GLubyte)(205 + n);
                stripeTex[r][c][1] = (GLubyte)(35 + n / 2);
                stripeTex[r][c][2] = (GLubyte)(35 + n / 2);
            }
            else
            {
                stripeTex[r][c][0] = (GLubyte)(238 + n / 2);
                stripeTex[r][c][1] = (GLubyte)(238 + n / 2);
                stripeTex[r][c][2] = (GLubyte)(232 + n / 2);
            }

            blotch = (sinf(r * 0.45f) * cosf(c * 0.37f) + 1.0f) * 16.0f;
            v = 185 + rand() % 40 + (int)blotch;
            if (v > 255) v = 255;
            groundTex[r][c][0] = (GLubyte)v;
            groundTex[r][c][1] = (GLubyte)v;
            groundTex[r][c][2] = (GLubyte)v;
        }
    }

    glGenTextures(1, &texStripe);
    glBindTexture(GL_TEXTURE_2D, texStripe);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, TS, TS, 0, GL_RGB, GL_UNSIGNED_BYTE, stripeTex);

    glGenTextures(1, &texGround);
    glBindTexture(GL_TEXTURE_2D, texGround);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, TS, TS, 0, GL_RGB, GL_UNSIGNED_BYTE, groundTex);
}

/* ---------------------------------------------------------------- */
/* island mesh: seabed -> wet sand -> beach -> grass -> plateau     */
/* ---------------------------------------------------------------- */
static void islandVertex(float u, float a)
{
    float r = u * islandR(a);
    float x = r * cosf(a), z = r * sinf(a);
    float h = groundHPolar(r, a);
    float e = 0.05f;
    float hx = (groundH(x + e, z) - groundH(x - e, z)) / (2.0f * e);
    float hz = (groundH(x, z + e) - groundH(x, z - e)) / (2.0f * e);
    float l = sqrtf(hx * hx + 1.0f + hz * hz);
    float n = 0.92f + 0.08f * sinf(x * 3.1f) * cosf(z * 2.7f);
    float cr, cg, cb, t;

    if (h < 0.0f)                      /* under water: sand fading to deep blue */
    {
        t = clampf(-h / 1.4f, 0.0f, 1.0f);
        cr = mixf(0.80f, 0.08f, t);
        cg = mixf(0.74f, 0.30f, t);
        cb = mixf(0.54f, 0.42f, t);
    }
    else if (h < 0.50f)                /* beach: wet sand -> dry sand */
    {
        t = h / 0.50f;
        cr = mixf(0.80f, 0.95f, t);
        cg = mixf(0.74f, 0.87f, t);
        cb = mixf(0.54f, 0.64f, t);
    }
    else if (h < 0.85f)                /* sand -> grass */
    {
        t = smoothf(0.50f, 0.85f, h);
        cr = mixf(0.95f, 0.30f, t);
        cg = mixf(0.87f, 0.58f, t);
        cb = mixf(0.64f, 0.22f, t);
    }
    else                               /* grass, a little lighter on top */
    {
        t = smoothf(0.85f, 2.2f, h);
        cr = mixf(0.28f, 0.38f, t);
        cg = mixf(0.56f, 0.62f, t);
        cb = mixf(0.20f, 0.28f, t);
    }

    glColor3f(cr * n, cg * n, cb * n);
    glNormal3f(-hx / l, 1.0f / l, -hz / l);
    glTexCoord2f(x * 0.25f, z * 0.25f);
    glVertex3f(x, h, z);
}

static void buildIsland(void)
{
    int i, j;
    int NR = 36, NS = 96;

    islandList = glGenLists(1);
    glNewList(islandList, GL_COMPILE);
    for (i = 0; i < NR; i++)
    {
        float u0 = 1.2f * i / NR;
        float u1 = 1.2f * (i + 1) / NR;
        glBegin(GL_TRIANGLE_STRIP);
        for (j = 0; j <= NS; j++)
        {
            float a = TWO_PI * j / NS;
            islandVertex(u1, a);
            islandVertex(u0, a);
        }
        glEnd();
    }
    glEndList();
}

/* ---------------------------------------------------------------- */
/* pathway: dock -> keeper's house -> lighthouse                    */
/* ---------------------------------------------------------------- */
static void buildPath(void)
{
    int seg, k, n = 0;

    for (seg = 0; seg < 5; seg++)
    {
        const float *p0 = pathCtl[seg > 0 ? seg - 1 : 0];
        const float *p1 = pathCtl[seg];
        const float *p2 = pathCtl[seg + 1];
        const float *p3 = pathCtl[seg + 2 < 6 ? seg + 2 : 5];

        for (k = 0; k < 14; k++)
        {
            float t = k / 14.0f;
            float t2 = t * t, t3 = t2 * t;
            int c;
            for (c = 0; c < 2; c++)
            {
                pathPts[n][c] = 0.5f * ((2.0f * p1[c]) +
                                (-p0[c] + p2[c]) * t +
                                (2.0f * p0[c] - 5.0f * p1[c] + 4.0f * p2[c] - p3[c]) * t2 +
                                (-p0[c] + 3.0f * p1[c] - 3.0f * p2[c] + p3[c]) * t3);
            }
            n++;
        }
    }
    pathPts[NPATH - 1][0] = pathCtl[5][0];
    pathPts[NPATH - 1][1] = pathCtl[5][1];
}

static float distToPath(float x, float z)
{
    int i;
    float best = 1.0e9f;

    for (i = 0; i < NPATH; i++)
    {
        float dx = x - pathPts[i][0], dz = z - pathPts[i][1];
        float d = dx * dx + dz * dz;
        if (d < best) best = d;
    }
    return sqrtf(best);
}

/* is the spot (x,z) already used by a building / path / tree ? */
static int blocked(float x, float z, float m)
{
    int i;
    float dx, dz;

    if (x * x + z * z < (1.9f + m) * (1.9f + m)) return 1;            /* lighthouse */
    dx = x - HUT_X;
    if (dx * dx + z * z < (1.2f + m) * (1.2f + m)) return 1;          /* keeper's house */
    dx = x - FIRE_X; dz = z - FIRE_Z;
    if (dx * dx + dz * dz < (0.9f + m) * (0.9f + m)) return 1;        /* campfire */
    dx = x - RESCUE_X; dz = z - RESCUE_Z;
    if (dx * dx + dz * dz < (1.3f + m) * (1.3f + m)) return 1;        /* rescue station */
    dx = x - SIGN_X; dz = z - SIGN_Z;
    if (dx * dx + dz * dz < (0.6f + m) * (0.6f + m)) return 1;        /* sign */
    for (i = 0; i < 4; i++)
    {
        dx = x - palmTab[i][0]; dz = z - palmTab[i][1];
        if (dx * dx + dz * dz < (0.6f + m) * (0.6f + m)) return 1;    /* palms */
    }
    if (distToPath(x, z) < 0.45f + m) return 1;                       /* path */
    return 0;
}

/* random grass / bush / flower positions on the grassy part */
static int findSpot(float u0, float u1, float margin, float *ox, float *oz)
{
    int tries;

    for (tries = 0; tries < 200; tries++)
    {
        float a = frand() * TWO_PI;
        float u = u0 + frand() * (u1 - u0);
        float x = islandR(a) * u * cosf(a);
        float z = islandR(a) * u * sinf(a);
        if (!blocked(x, z, margin))
        {
            *ox = x; *oz = z;
            return 1;
        }
    }
    return 0;
}

static void genScatter(void)
{
    int i;
    float x, z;

    for (i = 0; i < NTUFT; i++)
    {
        tuftP[i][2] = 0.0f;
        if (findSpot(0.33f, 0.74f, 0.25f, &x, &z))
        {
            tuftP[i][0] = x; tuftP[i][1] = z;
            tuftP[i][2] = 0.22f + frand() * 0.22f;
            tuftP[i][3] = frand() * TWO_PI;
        }
    }
    for (i = 0; i < NBUSH; i++)
    {
        bushP[i][2] = 0.0f;
        if (findSpot(0.36f, 0.68f, 0.8f, &x, &z))
        {
            bushP[i][0] = x; bushP[i][1] = z;
            bushP[i][2] = 0.28f + frand() * 0.20f;
            bushP[i][3] = frand();
        }
    }
    for (i = 0; i < NFLOW; i++)
    {
        flowP[i][2] = -1.0f;
        if (findSpot(0.34f, 0.70f, 0.35f, &x, &z))
        {
            flowP[i][0] = x; flowP[i][1] = z;
            flowP[i][2] = (float)(rand() % 5);
        }
    }
}

static void drawPath(void)
{
    int i, pass;

    glEnable(GL_POLYGON_OFFSET_FILL);
    glNormal3f(0.0f, 1.0f, 0.0f);
    for (pass = 0; pass < 2; pass++)
    {
        float w = (pass == 0) ? 0.40f : 0.30f;

        glPolygonOffset(-2.0f - 2.0f * pass, -2.0f - 2.0f * pass);
        if (pass == 0) glColor3f(0.42f, 0.32f, 0.22f);     /* dark edge */
        else           glColor3f(0.72f, 0.60f, 0.40f);     /* sandy centre */

        glBegin(GL_TRIANGLE_STRIP);
        for (i = 0; i < NPATH; i++)
        {
            int a = (i > 0) ? i - 1 : 0;
            int b = (i < NPATH - 1) ? i + 1 : NPATH - 1;
            float tx = pathPts[b][0] - pathPts[a][0];
            float tz = pathPts[b][1] - pathPts[a][1];
            float l = sqrtf(tx * tx + tz * tz);
            float nx = -tz / l, nz = tx / l;
            float px = pathPts[i][0], pz = pathPts[i][1];
            float x1 = px + nx * w, z1 = pz + nz * w;
            float x2 = px - nx * w, z2 = pz - nz * w;
            glVertex3f(x1, groundH(x1, z1) + 0.03f, z1);
            glVertex3f(x2, groundH(x2, z2) + 0.03f, z2);
        }
        glEnd();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
}

/* ---------------------------------------------------------------- */
/* grass, bushes, flowers                                           */
/* ---------------------------------------------------------------- */
static void drawGrass(void)
{
    int i, k;
    float sw = 1.0f + storm * 2.0f;

    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    for (i = 0; i < NTUFT; i++)
    {
        float x = tuftP[i][0], z = tuftP[i][1];
        float hgt = tuftP[i][2], rot = tuftP[i][3];
        float y0, sway;

        if (hgt <= 0.0f) continue;
        y0 = groundH(x, z) - 0.02f;
        sway = sinf(simTime * 1.5f + x * 1.3f + z) * 0.06f * sw;

        for (k = 0; k < 5; k++)
        {
            float ang = rot + k * 1.2566f;
            float ca = cosf(ang), sa = sinf(ang);
            float bx = ca * 0.07f, bz = sa * 0.07f;
            float lean = 0.10f + 0.03f * k;
            float px = -sa * 0.03f, pz = ca * 0.03f;
            float th = hgt * (0.80f + 0.10f * k);

            glColor3f(0.10f, 0.34f, 0.08f);
            glVertex3f(x + bx + px, y0, z + bz + pz);
            glVertex3f(x + bx - px, y0, z + bz - pz);
            glColor3f(0.40f + 0.03f * k, 0.72f, 0.20f);
            glVertex3f(x + bx + ca * lean + sway, y0 + th, z + bz + sa * lean);
        }
    }
    glEnd();
}

static void drawBushes(void)
{
    int i;

    for (i = 0; i < NBUSH; i++)
    {
        float x = bushP[i][0], z = bushP[i][1];
        float s = bushP[i][2], sh = bushP[i][3];
        float y;

        if (s <= 0.0f) continue;
        y = groundH(x, z) + 0.12f * s;

        glColor3f(0.10f + 0.08f * sh, 0.38f + 0.12f * sh, 0.12f);
        glPushMatrix();
        glTranslatef(x, y, z);
        glPushMatrix();
        glScalef(s, s * 0.75f, s);
        glutSolidSphere(1.0, 10, 7);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(0.65f * s, -0.05f * s, 0.20f * s);
        glScalef(s * 0.7f, s * 0.55f, s * 0.7f);
        glutSolidSphere(1.0, 10, 7);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(-0.55f * s, -0.02f * s, -0.30f * s);
        glScalef(s * 0.75f, s * 0.6f, s * 0.75f);
        glutSolidSphere(1.0, 10, 7);
        glPopMatrix();
        glPopMatrix();
    }
}

static void drawFlowers(void)
{
    int i;
    static const float fc[5][3] = {
        { 1.00f, 0.85f, 0.10f }, { 0.95f, 0.25f, 0.30f }, { 1.00f, 0.55f, 0.75f },
        { 0.96f, 0.96f, 0.96f }, { 0.70f, 0.40f, 0.90f }
    };

    for (i = 0; i < NFLOW; i++)
    {
        int c;
        float y;

        if (flowP[i][2] < 0.0f) continue;
        c = (int)flowP[i][2];
        y = groundH(flowP[i][0], flowP[i][1]);

        glColor3f(0.15f, 0.50f, 0.15f);                 /* stem */
        glPushMatrix();
        glTranslatef(flowP[i][0], y, flowP[i][1]);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glutSolidCone(0.02, 0.17, 4, 1);
        glPopMatrix();

        glColor3f(fc[c][0], fc[c][1], fc[c][2]);        /* flower head */
        glPushMatrix();
        glTranslatef(flowP[i][0], y + 0.18f, flowP[i][1]);
        glutSolidSphere(0.06, 6, 4);
        glPopMatrix();
    }
}

/* ---------------------------------------------------------------- */
/* rocks (irregular scaled icosahedrons)                            */
/* ---------------------------------------------------------------- */
static void drawRock(float x, float z, float s, float sy, float rot, float shade)
{
    float y = groundH(x, z);

    glPushMatrix();
    glTranslatef(x, y + 0.12f * s, z);
    glRotatef(rot, 0.0f, 1.0f, 0.0f);
    glRotatef(rot * 0.4f, 0.0f, 0.0f, 1.0f);
    glScalef(s, s * sy, s * 0.8f);
    glColor3f(shade, shade * 0.98f, shade * 0.94f);
    glutSolidIcosahedron();
    glPopMatrix();
}

static void drawRocks(void)
{
    int i;
    /* shoreline: angle, u, size, flatness, rotation, shade */
    static const float shore[9][6] = {
        { 0.25f, 0.84f, 0.45f, 0.70f,  10.0f, 0.45f },
        { 0.55f, 0.86f, 0.40f, 0.80f,  60.0f, 0.50f },
        { 1.60f, 0.85f, 0.45f, 0.70f, 100.0f, 0.46f },
        { 2.20f, 0.86f, 0.55f, 0.80f,  50.0f, 0.50f },
        { 2.90f, 0.83f, 0.38f, 0.70f,  80.0f, 0.42f },
        { 3.60f, 0.88f, 0.60f, 0.90f,  20.0f, 0.48f },
        { 4.30f, 0.84f, 0.42f, 0.70f, 120.0f, 0.44f },
        { 5.00f, 0.87f, 0.52f, 0.80f,  70.0f, 0.50f },
        { 5.70f, 0.83f, 0.36f, 0.70f,  30.0f, 0.40f }
    };
    /* inland: near lighthouse, house and path: x, z, size, flatness, rotation, shade */
    static const float inland[10][6] = {
        { -1.9f,  0.9f, 0.42f, 0.80f,  30.0f, 0.50f },
        { -1.7f, -1.8f, 0.34f, 0.70f,  70.0f, 0.46f },
        {  0.7f, -1.95f, 0.28f, 0.70f, 10.0f, 0.52f },
        { -2.6f, -0.3f, 0.25f, 0.90f, 100.0f, 0.55f },
        {  3.15f, -0.7f, 0.34f, 0.80f, 40.0f, 0.48f },
        {  2.8f,  0.95f, 0.20f, 0.80f, 90.0f, 0.50f },
        {  1.0f, -1.5f, 0.22f, 0.80f,  20.0f, 0.47f },
        {  4.5f,  4.3f, 0.40f, 0.80f,  60.0f, 0.47f },
        {  2.3f,  3.8f, 0.30f, 0.70f,  15.0f, 0.52f },
        { -0.9f,  4.3f, 0.30f, 0.80f, 140.0f, 0.45f }
    };

    for (i = 0; i < 9; i++)
    {
        float a = shore[i][0], u = shore[i][1];
        float rr = islandR(a) * u;
        drawRock(rr * cosf(a), rr * sinf(a), shore[i][2], shore[i][3], shore[i][4], shore[i][5]);
    }
    for (i = 0; i < 10; i++)
        drawRock(inland[i][0], inland[i][1], inland[i][2], inland[i][3], inland[i][4], inland[i][5]);
}

/* ---------------------------------------------------------------- */
/* beach details: shells, starfish, driftwood, net, crates          */
/* ---------------------------------------------------------------- */
static void drawCrate(float x, float y, float z, float s, float rot)
{
    float m = 0.30f + 0.70f * dayF;

    glPushMatrix();
    glTranslatef(x, y + s * 0.5f, z);
    glRotatef(rot, 0.0f, 1.0f, 0.0f);
    glColor3f(0.58f, 0.40f, 0.22f);
    glutSolidCube(s);
    glDisable(GL_LIGHTING);
    glColor3f(0.30f * m, 0.19f * m, 0.09f * m);
    glLineWidth(2.0f);
    glutWireCube(s * 1.004f);
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

static void drawStarfish(float x, float z, float rot)
{
    int k;
    float y = groundH(x, z) + 0.03f;

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rot, 0.0f, 1.0f, 0.0f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glColor3f(0.95f, 0.50f, 0.15f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 0.0f, 0.0f);
    for (k = 0; k <= 10; k++)
    {
        float a = TWO_PI * k / 10.0f;
        float r = (k % 2 == 0) ? 0.17f : 0.07f;
        glVertex3f(r * cosf(a), 0.0f, r * sinf(a));
    }
    glEnd();
    glPopMatrix();
}

static void drawBeachDetails(void)
{
    int i, j;
    float x, z, a, rr;
    float m = 0.25f + 0.75f * dayF;
    static const float shells[6][2] = {
        { 3.70f, 0.80f }, { 3.80f, 0.78f }, { 4.00f, 0.82f },
        { 5.40f, 0.79f }, { 0.35f, 0.80f }, { 2.50f, 0.79f }
    };
    static const float stars[3][2] = { { 4.90f, 0.81f }, { 0.10f, 0.78f }, { 3.20f, 0.80f } };

    /* seashells */
    glColor3f(0.96f, 0.88f, 0.82f);
    for (i = 0; i < 6; i++)
    {
        a = shells[i][0];
        rr = islandR(a) * shells[i][1];
        x = rr * cosf(a); z = rr * sinf(a);
        glPushMatrix();
        glTranslatef(x, groundH(x, z) + 0.03f, z);
        glRotatef(i * 57.0f, 0.0f, 1.0f, 0.0f);
        glScalef(0.08f, 0.045f, 0.065f);
        glutSolidSphere(1.0, 8, 6);
        glPopMatrix();
    }

    /* starfish */
    for (i = 0; i < 3; i++)
    {
        a = stars[i][0];
        rr = islandR(a) * stars[i][1];
        drawStarfish(rr * cosf(a), rr * sinf(a), i * 40.0f);
    }

    /* driftwood */
    for (i = 0; i < 2; i++)
    {
        a = (i == 0) ? 4.55f : 2.00f;
        rr = islandR(a) * ((i == 0) ? 0.80f : 0.82f);
        x = rr * cosf(a); z = rr * sinf(a);
        glColor3f(0.58f, 0.52f, 0.43f);
        glPushMatrix();
        glTranslatef(x, groundH(x, z) + 0.07f, z);
        glRotatef((i == 0) ? 70.0f : 20.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(0.0f, 0.0f, -0.55f);
        gluCylinder(quad, 0.06, 0.05, 1.1, 8, 1);
        glTranslatef(0.0f, 0.0f, 0.5f);
        glRotatef(-40.0f, 0.0f, 1.0f, 0.0f);
        gluCylinder(quad, 0.035, 0.02, 0.35, 6, 1);
        glPopMatrix();
    }

    /* fishing net lying on the sand */
    a = 3.45f;
    rr = islandR(a) * 0.77f;
    x = rr * cosf(a); z = rr * sinf(a);
    glDisable(GL_LIGHTING);
    glColor3f(0.75f * m, 0.70f * m, 0.52f * m);
    for (i = 0; i <= 6; i++)
    {
        glBegin(GL_LINE_STRIP);
        for (j = 0; j <= 7; j++)
        {
            float px = x - 0.45f + i * 0.15f;
            float pz = z - 0.35f + j * 0.10f;
            glVertex3f(px, groundH(px, pz) + 0.035f, pz);
        }
        glEnd();
    }
    for (j = 0; j <= 7; j++)
    {
        glBegin(GL_LINE_STRIP);
        for (i = 0; i <= 6; i++)
        {
            float px = x - 0.45f + i * 0.15f;
            float pz = z - 0.35f + j * 0.10f;
            glVertex3f(px, groundH(px, pz) + 0.035f, pz);
        }
        glEnd();
    }
    glEnable(GL_LIGHTING);

    /* wooden crates on the beach */
    a = 5.15f; rr = islandR(a) * 0.78f; x = rr * cosf(a); z = rr * sinf(a);
    drawCrate(x, groundH(x, z) - 0.03f, z, 0.42f, 25.0f);
    a = 2.60f; rr = islandR(a) * 0.79f; x = rr * cosf(a); z = rr * sinf(a);
    drawCrate(x, groundH(x, z) - 0.03f, z, 0.36f, 70.0f);
}

/* ---------------------------------------------------------------- */
/* palm trees                                                       */
/* ---------------------------------------------------------------- */
static void drawFrond(float L, float W, float shade)
{
    int i;

    glNormal3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLE_STRIP);
    for (i = 0; i <= 8; i++)
    {
        float s = i / 8.0f;
        float cx = L * s;
        float cz = L * (0.45f * s - 0.85f * s * s);
        float w = W * (0.8f * sinf(PI * s) + 0.2f * (1.0f - s));

        glColor3f(0.10f + 0.10f * s + shade, 0.42f + 0.16f * s, 0.10f);
        glVertex3f(cx, -w, cz);
        glVertex3f(cx, w, cz);
    }
    glEnd();
}

static void drawPalm(float x, float z, float sc, float rotY, float lean)
{
    int seg, k;
    float y = groundH(x, z) - 0.05f;
    float bend = lean / 6.0f;
    float sway = 2.5f * sinf(simTime * 1.2f + x) * (0.6f + storm * 2.5f);

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glScalef(sc, sc, sc);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);          /* local z = up */

    /* curved trunk from 6 tilted segments */
    glColor3f(0.45f, 0.30f, 0.16f);
    for (seg = 0; seg < 6; seg++)
    {
        float r0 = 0.17f - 0.018f * seg;
        gluCylinder(quad, r0, r0 - 0.018f, 0.62, 10, 1);
        glTranslatef(0.0f, 0.0f, 0.62f);
        glRotatef(bend + sway * 0.12f, 0.0f, 1.0f, 0.0f);
    }

    /* coconuts */
    glColor3f(0.30f, 0.20f, 0.10f);
    glPushMatrix();
    glTranslatef(0.10f, 0.05f, 0.0f);
    glutSolidSphere(0.09, 8, 6);
    glTranslatef(-0.20f, -0.05f, 0.02f);
    glutSolidSphere(0.09, 8, 6);
    glPopMatrix();

    /* crown of fronds */
    for (k = 0; k < 8; k++)
    {
        glPushMatrix();
        glRotatef(k * 45.0f + 10.0f * (k % 2), 0.0f, 0.0f, 1.0f);
        glRotatef(-12.0f * (k % 3), 0.0f, 1.0f, 0.0f);
        drawFrond(1.45f + 0.15f * (k % 3), 0.17f, 0.03f * (k % 2));
        glPopMatrix();
    }
    glPopMatrix();
}

static void drawPalms(void)
{
    int i;
    for (i = 0; i < 4; i++)
        drawPalm(palmTab[i][0], palmTab[i][1], palmTab[i][2], palmTab[i][3], palmTab[i][4]);
}

/* ---------------------------------------------------------------- */
/* lighthouse, keeper's house                                       */
/* ---------------------------------------------------------------- */
static void drawLighthouse(void)
{
    int k;

    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);   /* local +z becomes up, local -y = world +z */

    /* striped tower */
    if (!gShadow)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texStripe);
        glColor3f(1.0f, 1.0f, 1.0f);
        gluQuadricTexture(quad, GL_TRUE);
    }
    gluCylinder(quad, 1.1, 0.72, TOWER_H, 28, 10);
    if (!gShadow)
    {
        glDisable(GL_TEXTURE_2D);
        gluQuadricTexture(quad, GL_FALSE);
    }

    /* door and windows (not part of the shadow) */
    if (!gShadow)
    {
        int w;

        glColor3f(0.30f, 0.16f, 0.08f);                 /* door */
        drawBox(0.0f, -1.03f, 0.85f, 0.52f, 0.14f, 1.0f);
        glColor3f(0.90f, 0.75f, 0.20f);                 /* door knob */
        glPushMatrix();
        glTranslatef(0.15f, -1.11f, 0.80f);
        glutSolidSphere(0.035, 8, 6);
        glPopMatrix();

        glDisable(GL_LIGHTING);                         /* windows glow at night */
        for (w = 0; w < 6; w++)
        {
            float zc = (w < 3) ? 3.0f : 5.0f;
            float ang = (w % 3) * 120.0f + ((w < 3) ? 0.0f : 60.0f);
            float rr = 1.1f - 0.38f * zc / TOWER_H;

            glColor3f(mixf(0.12f, 1.0f, nightF), mixf(0.18f, 0.85f, nightF), mixf(0.30f, 0.35f, nightF));
            glPushMatrix();
            glRotatef(ang, 0.0f, 0.0f, 1.0f);
            glTranslatef(0.0f, -rr, zc);
            glScalef(0.24f, 0.12f, 0.42f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
        glEnable(GL_LIGHTING);
    }

    /* stone plinth */
    setCol(0.50f, 0.50f, 0.53f);
    gluCylinder(quad, 1.35, 1.35, 0.35, 28, 1);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.35f);
    gluDisk(quad, 0.0, 1.35, 28, 1);
    glPopMatrix();

    /* gallery (balcony) */
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, TOWER_H);
    setCol(0.15f, 0.15f, 0.17f);
    gluCylinder(quad, 1.0, 1.0, 0.12, 28, 1);
    glTranslatef(0.0f, 0.0f, 0.12f);
    gluDisk(quad, 0.0, 1.05, 28, 1);
    setCol(0.08f, 0.08f, 0.08f);
    for (k = 0; k < 8; k++)
    {
        glPushMatrix();
        glRotatef(k * 45.0f, 0.0f, 0.0f, 1.0f);
        glTranslatef(0.98f, 0.0f, 0.0f);
        gluCylinder(quad, 0.025, 0.025, 0.45, 6, 1);
        glPopMatrix();
    }
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.45f);
    glutSolidTorus(0.03, 0.98, 6, 28);
    glPopMatrix();
    /* lantern frame posts */
    for (k = 0; k < 4; k++)
    {
        glPushMatrix();
        glRotatef(k * 90.0f + 45.0f, 0.0f, 0.0f, 1.0f);
        glTranslatef(0.55f, 0.0f, 0.0f);
        gluCylinder(quad, 0.03, 0.03, 1.0, 6, 1);
        glPopMatrix();
    }
    glPopMatrix();

    /* roof */
    glTranslatef(0.0f, 0.0f, TOWER_H + 1.12f);
    setCol(0.55f, 0.08f, 0.08f);
    glutSolidCone(0.8, 0.9, 24, 4);
    glTranslatef(0.0f, 0.0f, 0.9f);
    glutSolidSphere(0.09, 10, 8);

    glPopMatrix();
}

static void drawHut(void)   /* origin = centre of hut */
{
    /* walls */
    glPushMatrix();
    glScalef(0.8f, 0.65f, 0.7f);
    setCol(0.55f, 0.40f, 0.25f);
    glutSolidCube(1.0);
    glPopMatrix();

    /* roof */
    glPushMatrix();
    glTranslatef(0.0f, 0.325f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(45.0f, 0.0f, 0.0f, 1.0f);
    setCol(0.45f, 0.12f, 0.10f);
    glutSolidCone(0.62, 0.40, 4, 1);
    glPopMatrix();

    /* chimney */
    setCol(0.40f, 0.18f, 0.14f);
    drawBox(0.22f, 0.50f, 0.12f, 0.12f, 0.45f, 0.12f);

    /* small porch: deck, two posts and a roof */
    setCol(0.50f, 0.36f, 0.20f);
    drawBox(0.10f, -0.30f, 0.52f, 0.70f, 0.05f, 0.30f);
    setCol(0.40f, 0.28f, 0.16f);
    drawBox(-0.22f, -0.05f, 0.64f, 0.05f, 0.50f, 0.05f);
    drawBox(0.42f, -0.05f, 0.64f, 0.05f, 0.50f, 0.05f);
    setCol(0.45f, 0.12f, 0.10f);
    drawBox(0.10f, 0.22f, 0.52f, 0.80f, 0.035f, 0.40f);
    setCol(0.10f, 0.10f, 0.10f);                         /* lamp bracket */
    drawBox(0.40f, 0.19f, 0.60f, 0.03f, 0.05f, 0.03f);

    if (!gShadow)
    {
        float b = mixf(0.35f, 1.0f, nightF);

        glDisable(GL_LIGHTING);

        /* door (front face, +z) */
        glColor3f(0.30f, 0.16f, 0.08f);
        glBegin(GL_QUADS);
        glVertex3f(0.06f, -0.325f, 0.356f);
        glVertex3f(0.28f, -0.325f, 0.356f);
        glVertex3f(0.28f,  0.05f,  0.356f);
        glVertex3f(0.06f,  0.05f,  0.356f);
        glEnd();

        /* two windows glow more at night */
        glColor3f(b, b * 0.85f, b * 0.35f);
        glBegin(GL_QUADS);
        /* front window */
        glVertex3f(-0.30f, -0.08f, 0.356f);
        glVertex3f(-0.08f, -0.08f, 0.356f);
        glVertex3f(-0.08f,  0.14f, 0.356f);
        glVertex3f(-0.30f,  0.14f, 0.356f);
        /* side window (-x face) */
        glVertex3f(-0.402f, -0.08f, -0.12f);
        glVertex3f(-0.402f, -0.08f,  0.12f);
        glVertex3f(-0.402f,  0.14f,  0.12f);
        glVertex3f(-0.402f,  0.14f, -0.12f);
        glEnd();

        glEnable(GL_LIGHTING);
    }
}

static void drawLanternGlassAndLamp(void)
{
    float b = 0.35f + 0.65f * nightF;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(0.0f, PLATEAU + TOWER_H + 0.12f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glColor4f(0.75f, 0.90f, 1.0f, 0.22f);
    gluCylinder(quad, 0.55, 0.55, 1.0, 20, 1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, LAMP_Y, 0.0f);
    glColor4f(1.0f * b, 0.95f * b, 0.60f * b, 1.0f);
    glutSolidSphere(0.30, 14, 10);
    if (beamOn)
    {
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glColor4f(1.0f, 0.9f, 0.5f, 0.12f + 0.25f * nightF);
        glutSolidSphere(0.9, 14, 10);
        glDepthMask(GL_TRUE);
    }
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

/* warning sign near the lighthouse */
static void drawSign(void)
{
    float y = groundH(SIGN_X, SIGN_Z);

    glPushMatrix();
    glTranslatef(SIGN_X, y, SIGN_Z);
    glRotatef(25.0f, 0.0f, 1.0f, 0.0f);
    glColor3f(0.40f, 0.28f, 0.16f);
    drawPole(-0.45f, 0.0f, 0.0f, 0.04f, 0.8f);
    drawPole(0.45f, 0.0f, 0.0f, 0.04f, 0.8f);
    glColor3f(0.95f, 0.80f, 0.10f);
    drawBox(0.0f, 0.62f, 0.0f, 1.15f, 0.40f, 0.05f);
    glDisable(GL_LIGHTING);
    glColor3f(0.05f, 0.05f, 0.05f);
    glLineWidth(2.0f);
    glTranslatef(0.0f, 0.56f, 0.035f);
    strokeText("CAUTION", 0.0022f);
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

/* ---------------------------------------------------------------- */
/* rescue station                                                   */
/* ---------------------------------------------------------------- */
static void drawLifeRing(float x, float y, float z, float rotY)
{
    int k;

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glColor3f(0.90f, 0.15f, 0.12f);
    glutSolidTorus(0.04, 0.14, 8, 18);
    glColor3f(0.96f, 0.96f, 0.96f);
    for (k = 0; k < 4; k++)
    {
        float a = k * PI * 0.5f + PI * 0.25f;
        drawBox(0.14f * cosf(a), 0.14f * sinf(a), 0.0f, 0.07f, 0.07f, 0.09f);
    }
    glPopMatrix();
}

static void drawRescueStation(void)
{
    float y = groundH(RESCUE_X, RESCUE_Z);
    float g = mixf(0.12f, 1.0f, nightF);

    glPushMatrix();
    glTranslatef(RESCUE_X, y + 0.06f, RESCUE_Z);
    glRotatef(RESCUE_ROT, 0.0f, 1.0f, 0.0f);

    /* stone foundation (hides the slope) and red walls */
    glColor3f(0.50f, 0.50f, 0.52f);
    drawBox(0.0f, -0.45f, 0.0f, 1.50f, 1.00f, 1.20f);
    glColor3f(0.78f, 0.14f, 0.12f);
    drawBox(0.0f, 0.45f, 0.0f, 1.30f, 0.80f, 1.00f);
    /* white flat roof */
    glColor3f(0.92f, 0.92f, 0.92f);
    drawBox(0.0f, 0.90f, 0.0f, 1.50f, 0.10f, 1.20f);

    /* door */
    glColor3f(0.93f, 0.93f, 0.90f);
    drawBox(-0.25f, 0.30f, 0.51f, 0.34f, 0.50f, 0.03f);
    /* sign board */
    glColor3f(0.96f, 0.96f, 0.96f);
    drawBox(0.0f, 0.74f, 0.51f, 1.10f, 0.20f, 0.04f);

    /* life rings: front wall and side wall */
    drawLifeRing(0.06f, 0.36f, 0.55f, 0.0f);
    drawLifeRing(0.68f, 0.45f, 0.0f, 90.0f);

    /* rescue equipment: orange box + red extinguisher */
    glColor3f(0.98f, 0.55f, 0.08f);
    drawBox(0.45f, 0.15f, 0.64f, 0.34f, 0.20f, 0.22f);
    glColor3f(0.85f, 0.08f, 0.08f);
    drawPole(0.68f, 0.05f, 0.62f, 0.05f, 0.28f);

    /* radio antenna on the roof */
    glColor3f(0.35f, 0.35f, 0.38f);
    drawPole(0.45f, 0.95f, -0.25f, 0.025f, 1.30f);
    drawBox(0.45f, 1.70f, -0.25f, 0.55f, 0.02f, 0.02f);
    drawBox(0.45f, 1.95f, -0.25f, 0.40f, 0.02f, 0.02f);
    drawBox(0.45f, 2.15f, -0.25f, 0.25f, 0.02f, 0.02f);

    glDisable(GL_LIGHTING);
    /* window glows at night */
    glColor3f(g, g * 0.85f, g * 0.35f);
    drawBox(0.40f, 0.50f, 0.51f, 0.30f, 0.25f, 0.03f);
    /* "RESCUE" text */
    glColor3f(0.80f, 0.10f, 0.08f);
    glLineWidth(2.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.69f, 0.54f);
    strokeText("RESCUE", 0.0016f);
    glPopMatrix();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);

    glPopMatrix();
}

/* ---------------------------------------------------------------- */
/* wooden dock with boat, crates, rope, life ring                   */
/* ---------------------------------------------------------------- */
/* dock frame -> world:  s along the dock, l sideways */
static void dockPoint(float s, float l, float *x, float *z)
{
    float c = cosf(DOCK_A), sn = sinf(DOCK_A);

    *x = DOCK_R0 * c + s * c - l * sn;
    *z = DOCK_R0 * sn + s * sn + l * c;
}

/* small rowing boat tied to the dock (drawn inside the dock frame) */
static void drawDockBoat(void)
{
    float wx, wz, yb, roll;
    GLdouble clip[4] = { 0.0, -1.0, 0.0, 0.14 };    /* keep y <= 0.14 */

    dockPoint(4.3f, 1.7f, &wx, &wz);
    yb = seaSurface(wx, wz) - 0.02f;
    roll = 5.0f * sinf(waveT * 1.4f + 1.0f) * (1.0f + storm * 2.0f);

    glPushMatrix();
    glTranslatef(4.3f, yb, 1.7f);
    glRotatef(roll, 1.0f, 0.0f, 0.0f);
    glRotatef(4.0f * sinf(waveT * 1.1f), 0.0f, 0.0f, 1.0f);

    glClipPlane(GL_CLIP_PLANE0, clip);
    glEnable(GL_CLIP_PLANE0);
    glColor3f(0.18f, 0.42f, 0.68f);
    glPushMatrix();
    glScalef(0.95f, 0.30f, 0.42f);
    glutSolidSphere(1.0, 18, 12);
    glPopMatrix();
    glDisable(GL_CLIP_PLANE0);

    glColor3f(0.72f, 0.57f, 0.36f);                  /* floor and seats */
    drawBox(0.0f, 0.08f, 0.0f, 1.30f, 0.03f, 0.50f);
    drawBox(-0.30f, 0.16f, 0.0f, 0.14f, 0.04f, 0.58f);
    drawBox(0.35f, 0.16f, 0.0f, 0.14f, 0.04f, 0.50f);
    glColor3f(0.95f, 0.95f, 0.95f);                  /* white stripe on the bow */
    drawBox(0.80f, 0.12f, 0.0f, 0.12f, 0.05f, 0.30f);
    glPopMatrix();

    /* mooring rope from post to boat */
    glDisable(GL_LIGHTING);
    glColor3f(0.78f, 0.66f, 0.42f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(4.75f, 0.55f, 0.55f);
    glVertex3f(5.15f, yb + 0.15f, 1.65f);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

static void drawDock(void)
{
    int i, k, side, n;
    float m = 0.30f + 0.70f * dayF;

    glPushMatrix();
    glTranslatef(DOCK_R0 * cosf(DOCK_A), 0.0f, DOCK_R0 * sinf(DOCK_A));
    glRotatef(-RAD2DEG(DOCK_A), 0.0f, 1.0f, 0.0f);   /* local +x = along the dock */

    /* deck planks */
    for (i = 0; i < 22; i++)
    {
        float v = 0.90f + 0.10f * (float)(i % 3);
        glColor3f(0.54f * v, 0.37f * v, 0.21f * v);
        drawBox(0.15f + i * 0.28f, DECK_Y - 0.03f, 0.0f, 0.24f, 0.06f, 1.25f);
    }
    /* beams under the deck */
    glColor3f(0.38f, 0.26f, 0.14f);
    drawBox(DOCK_L * 0.5f, DECK_Y - 0.13f, 0.45f, DOCK_L, 0.10f, 0.10f);
    drawBox(DOCK_L * 0.5f, DECK_Y - 0.13f, -0.45f, DOCK_L, 0.10f, 0.10f);

    /* support poles (and rope posts) */
    glColor3f(0.36f, 0.25f, 0.14f);
    for (k = 0; k < 5; k++)
    {
        for (side = -1; side <= 1; side += 2)
        {
            float px = 0.4f + k * 1.45f;
            glPushMatrix();
            glTranslatef(px, -1.3f, side * 0.55f);
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
            gluCylinder(quad, 0.07, 0.07, 2.3, 8, 1);
            glTranslatef(0.0f, 0.0f, 2.3f);
            gluDisk(quad, 0.0, 0.07, 8, 1);
            glPopMatrix();
        }
    }

    /* rope between the posts */
    glDisable(GL_LIGHTING);
    glColor3f(0.78f * m, 0.66f * m, 0.42f * m);
    glLineWidth(2.5f);
    for (side = -1; side <= 1; side += 2)
    {
        glBegin(GL_LINE_STRIP);
        for (k = 0; k < 4; k++)
        {
            for (n = 0; n <= 8; n++)
            {
                float t = n / 8.0f;
                float px = 0.4f + (k + t) * 1.45f;
                glVertex3f(px, 0.90f - 0.14f * sinf(PI * t), side * 0.55f);
            }
        }
        glEnd();
    }
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);

    /* crates */
    drawCrate(5.35f, DECK_Y, -0.25f, 0.46f, 10.0f);
    drawCrate(5.35f, DECK_Y + 0.46f, -0.22f, 0.32f, 35.0f);
    drawCrate(5.55f, DECK_Y, 0.25f, 0.34f, -15.0f);

    /* life ring hanging on a post */
    drawLifeRing(4.75f, 0.50f, -0.68f, 0.0f);

    /* lantern at the end of the dock (glow is drawn in drawLamps) */
    glColor3f(0.12f, 0.12f, 0.12f);
    drawBox(DOCK_L, 1.04f, -0.55f, 0.10f, 0.08f, 0.10f);

    drawDockBoat();

    glPopMatrix();
}

/* ---------------------------------------------------------------- */
/* campfire                                                         */
/* ---------------------------------------------------------------- */
static void drawCampfireBase(void)
{
    int k;
    float y = groundH(FIRE_X, FIRE_Z);

    glPushMatrix();
    glTranslatef(FIRE_X, y, FIRE_Z);

    /* ring of stones */
    glColor3f(0.45f, 0.45f, 0.47f);
    for (k = 0; k < 8; k++)
    {
        float a = k * TWO_PI / 8.0f;
        glPushMatrix();
        glTranslatef(0.50f * cosf(a), 0.05f, 0.50f * sinf(a));
        glScalef(0.13f, 0.09f, 0.11f);
        glRotatef(k * 40.0f, 0.0f, 1.0f, 0.0f);
        glutSolidIcosahedron();
        glPopMatrix();
    }

    /* four crossed logs */
    glColor3f(0.32f, 0.20f, 0.10f);
    for (k = 0; k < 4; k++)
    {
        glPushMatrix();
        glTranslatef(0.0f, 0.10f, 0.0f);
        glRotatef(k * 45.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(-12.0f, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, 0.0f, -0.40f);
        gluCylinder(quad, 0.06, 0.06, 0.80, 8, 1);
        glPopMatrix();
    }
    glPopMatrix();
}

/* animated flame + glow (blended, drawn after the sea) */
static void drawCampfireFlame(void)
{
    int k;
    float y = groundH(FIRE_X, FIRE_Z);
    float fl = 1.0f + 0.18f * sinf(fireT * 17.0f) + 0.10f * sinf(fireT * 29.0f);
    float glow = 0.30f + 0.70f * nightF;
    static const float rad[3] = { 0.26f, 0.19f, 0.11f };
    static const float hgt[3] = { 0.75f, 0.58f, 0.42f };
    static const float col[3][4] = {
        { 1.00f, 0.22f, 0.04f, 0.70f },
        { 1.00f, 0.55f, 0.10f, 0.80f },
        { 1.00f, 0.92f, 0.45f, 0.90f }
    };

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(FIRE_X, y + 0.12f, FIRE_Z);

    for (k = 0; k < 3; k++)
    {
        float ff = 1.0f + (fl - 1.0f) * (1.0f + 0.5f * k);
        float sx = 0.05f * sinf(fireT * 9.0f + k * 1.7f);
        float sz = 0.05f * cosf(fireT * 7.0f + k * 2.3f);

        glPushMatrix();
        glTranslatef(sx, 0.0f, sz);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glColor4f(col[k][0], col[k][1], col[k][2], col[k][3]);
        glutSolidCone(rad[k] * (0.9f + 0.1f * ff), hgt[k] * ff, 10, 2);
        glPopMatrix();
    }

    /* soft glow (additive) */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPushMatrix();
    glTranslatef(0.0f, 0.35f, 0.0f);
    glColor4f(1.0f, 0.55f, 0.15f, (0.10f + 0.10f * fl) * glow);
    glutSolidSphere(0.9, 14, 10);
    glColor4f(1.0f, 0.45f, 0.10f, 0.05f * glow);
    glutSolidSphere(1.6, 14, 10);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.0f, -0.08f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glColor4f(1.0f, 0.50f, 0.12f, 0.25f * glow * nightF + 0.04f);
    gluDisk(quad, 0.0, 1.9, 24, 1);
    glPopMatrix();

    glPopMatrix();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

/* ---------------------------------------------------------------- */
/* planar shadows (stencil clipped to the plateau)                  */
/* ---------------------------------------------------------------- */
static void makeShadowMatrix(GLfloat *m, const GLfloat *p, const GLfloat *l)
{
    int i, j;
    GLfloat d = p[0] * l[0] + p[1] * l[1] + p[2] * l[2] + p[3] * l[3];

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            m[j * 4 + i] = (i == j ? d : 0.0f) - l[i] * p[j];
}

static void drawShadows(void)
{
    GLfloat m[16];
    GLfloat plane[4];
    GLfloat lp[4];

    plane[0] = 0.0f; plane[1] = 1.0f; plane[2] = 0.0f; plane[3] = -(PLATEAU + 0.012f);
    lp[0] = lightDirv[0]; lp[1] = lightDirv[1]; lp[2] = lightDirv[2]; lp[3] = 0.0f;
    makeShadowMatrix(m, plane, lp);

    glDisable(GL_LIGHTING);
    glEnable(GL_STENCIL_TEST);

    /* 1) mark the plateau in the stencil buffer */
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glStencilFunc(GL_ALWAYS, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glPushMatrix();
    glTranslatef(0.0f, PLATEAU + 0.003f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluDisk(quad, 0.0, 2.2, 32, 1);
    glPopMatrix();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    /* 2) draw projected geometry only where stencil == 1 */
    glStencilFunc(GL_EQUAL, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    gShadow = 1;

    glPushMatrix();
    glMultMatrixf(m);
    glPushMatrix();
    glTranslatef(0.0f, PLATEAU, 0.0f);
    drawLighthouse();
    glPopMatrix();
    glPushMatrix();
    glTranslatef(HUT_X, PLATEAU + 0.325f, 0.0f);
    drawHut();
    glPopMatrix();
    glPopMatrix();

    gShadow = 0;
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

/* ---------------------------------------------------------------- */
/* ship                                                             */
/* ---------------------------------------------------------------- */
static void updateBoat(float dt)
{
    float c, s, yawRad, sideX, sideZ;
    float hF, hB, hS, hO;

    if (shipOn)
        boatTheta += boatOmega * (1.0f - 0.3f * storm) * dt;
    c = cosf(boatTheta);
    s = sinf(boatTheta);
    boatX = BOAT_R * c;
    boatZ = BOAT_R * s;
    boatHX = -s;
    boatHZ = c;

    yawRad = atan2f(-boatHZ, boatHX);
    boatYaw = RAD2DEG(yawRad);
    sideX = sinf(yawRad);
    sideZ = cosf(yawRad);

    /* the ship floats on the waves: height and tilt follow the water */
    hF = waveH(boatX + boatHX * 1.6f, boatZ + boatHZ * 1.6f, waveT, 0, 0);
    hB = waveH(boatX - boatHX * 1.6f, boatZ - boatHZ * 1.6f, waveT, 0, 0);
    hS = waveH(boatX + sideX * 0.65f, boatZ + sideZ * 0.65f, waveT, 0, 0);
    hO = waveH(boatX - sideX * 0.65f, boatZ - sideZ * 0.65f, waveT, 0, 0);

    boatPitch = RAD2DEG(atan2f(hF - hB, 3.2f));
    boatRoll = -RAD2DEG(atan2f(hS - hO, 1.3f));
    boatY = (hF + hB + hS + hO) * 0.25f - 0.10f;

    wakeTimer += dt;
    if (shipOn && wakeTimer > 0.12f)
    {
        wakeTimer = 0.0f;
        wake[wakeHead][0] = boatX;
        wake[wakeHead][1] = boatZ;
        wakeHead = (wakeHead + 1) % NWAKE;
        if (wakeCount < NWAKE) wakeCount++;
    }
}

static void drawBoat(void)
{
    int i;
    GLdouble clip[4] = { 0.0, -1.0, 0.0, 0.25 };   /* keep y <= 0.25 */

    glPushMatrix();
    glTranslatef(boatX, boatY, boatZ);
    glRotatef(boatYaw, 0.0f, 1.0f, 0.0f);
    glRotatef(boatPitch, 0.0f, 0.0f, 1.0f);
    glRotatef(boatRoll, 1.0f, 0.0f, 0.0f);

    /* hull (ellipsoid cut at deck level) */
    glClipPlane(GL_CLIP_PLANE0, clip);
    glEnable(GL_CLIP_PLANE0);
    glColor3f(0.45f, 0.25f, 0.10f);
    glPushMatrix();
    glScalef(1.9f, 0.5f, 0.75f);
    glutSolidSphere(1.0, 22, 14);
    glPopMatrix();
    glDisable(GL_CLIP_PLANE0);

    /* deck */
    glColor3f(0.72f, 0.57f, 0.36f);
    glPushMatrix();
    glTranslatef(0.0f, 0.25f, 0.0f);
    glScalef(3.3f, 0.04f, 1.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    /* cabin */
    glColor3f(0.92f, 0.92f, 0.86f);
    glPushMatrix();
    glTranslatef(-0.7f, 0.55f, 0.0f);
    glScalef(0.9f, 0.55f, 0.7f);
    glutSolidCube(1.0);
    glPopMatrix();
    glColor3f(0.65f, 0.12f, 0.10f);
    glPushMatrix();
    glTranslatef(-0.7f, 0.85f, 0.0f);
    glScalef(1.0f, 0.06f, 0.8f);
    glutSolidCube(1.0);
    glPopMatrix();

    /* mast */
    glColor3f(0.35f, 0.25f, 0.15f);
    glPushMatrix();
    glTranslatef(0.5f, 0.25f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(quad, 0.05, 0.035, 3.4, 8, 1);
    glPopMatrix();

    /* sail (flapping triangle strip) */
    glColor3f(0.96f, 0.96f, 0.92f);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLE_STRIP);
    for (i = 0; i <= 12; i++)
    {
        float s = i / 12.0f;
        float x = 0.55f + 1.5f * s;
        float top = 0.45f + 2.9f * (1.0f - s);
        float z = 0.30f * sinf(PI * s) * (0.7f + 0.3f * sinf(simTime * 2.3f + s * 3.0f));
        glVertex3f(x, 0.45f, z);
        glVertex3f(x, top, z);
    }
    glEnd();

    /* mast-top lamp (visible at night) */
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.9f, 0.5f);
    glPushMatrix();
    glTranslatef(0.5f, 3.68f, 0.0f);
    glutSolidSphere(0.09, 8, 6);
    glPopMatrix();
    glEnable(GL_LIGHTING);

    glPopMatrix();
}

static void drawWake(void)
{
    int k;

    if (wakeCount < 2) return;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(3.0f);
    glBegin(GL_LINE_STRIP);
    for (k = 0; k < wakeCount; k++)
    {
        int idx = (wakeHead - wakeCount + k + NWAKE * 2) % NWAKE;
        float x = wake[idx][0], z = wake[idx][1];
        float h = waveH(x, z, waveT, 0, 0);
        glColor4f(1.0f, 1.0f, 1.0f, 0.65f * (float)k / (float)wakeCount);
        glVertex3f(x, h + 0.07f, z);
    }
    glEnd();
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

/* ---------------------------------------------------------------- */
/* environment: sun, moon, sky colour, lights                       */
/* ---------------------------------------------------------------- */
static void updateEnvironment(void)
{
    int i;
    float a = (tod - 6.0f) / 24.0f * TWO_PI;
    float sx = cosf(a), sy = sinf(a), sz = 0.4f;
    float l = sqrtf(sx * sx + sy * sy + sz * sz);
    float dayC[3] = { 0.47f, 0.72f, 0.96f };
    float nightC[3] = { 0.015f, 0.025f, 0.08f };
    float duskC[3] = { 0.95f, 0.52f, 0.28f };
    float grayC[3] = { 0.30f, 0.32f, 0.35f };
    float duskF, I;

    sunE = sy;
    sunDirW[0] = sx / l; sunDirW[1] = sy / l; sunDirW[2] = sz / l;
    moonDirW[0] = -sunDirW[0]; moonDirW[1] = -sunDirW[1]; moonDirW[2] = -sunDirW[2];
    dayF = smoothf(-0.08f, 0.30f, sunE);
    nightF = 1.0f - dayF;
    duskF = clampf(1.0f - fabsf(sunE) / 0.28f, 0.0f, 1.0f) * 0.75f;

    for (i = 0; i < 3; i++)
    {
        float c = mixf(nightC[i], dayC[i], dayF);
        c = mixf(c, duskC[i], duskF);
        c = mixf(c, grayC[i] * (0.15f + 0.85f * dayF), 0.8f * storm);
        c = mixf(c, 1.0f, flash * 0.85f);
        skyCol[i] = c;
    }

    if (sunE >= -0.02f)
    {
        float warm = clampf(sunE * 3.0f, 0.0f, 1.0f);
        I = clampf(sunE * 1.6f + 0.12f, 0.0f, 1.0f) * (1.0f - 0.55f * storm);
        lightDirv[0] = sunDirW[0]; lightDirv[1] = sunDirW[1]; lightDirv[2] = sunDirW[2];
        sunDiff[0] = 1.0f * I;
        sunDiff[1] = mixf(0.55f, 0.97f, warm) * I;
        sunDiff[2] = mixf(0.30f, 0.90f, warm) * I;
    }
    else
    {
        I = clampf(-sunE * 1.4f, 0.0f, 1.0f) * 0.30f;
        lightDirv[0] = moonDirW[0]; lightDirv[1] = moonDirW[1]; lightDirv[2] = moonDirW[2];
        sunDiff[0] = 0.55f * I;
        sunDiff[1] = 0.65f * I;
        sunDiff[2] = 0.95f * I;
    }
    sunDiff[3] = 1.0f;

    for (i = 0; i < 3; i++)
    {
        float dayA[3] = { 0.34f, 0.37f, 0.43f };
        float nightA[3] = { 0.05f, 0.065f, 0.12f };
        float c = mixf(nightA[i], dayA[i], dayF) * (1.0f - 0.35f * storm);
        ambCol[i] = clampf(c + flash * 0.55f, 0.0f, 1.0f);
    }
    ambCol[3] = 1.0f;
}

static void updateBeamDirs(void)
{
    int b;
    for (b = 0; b < 2; b++)
    {
        float ang = beamAngle + b * PI;
        float dx = cosf(ang), dy = -0.18f, dz = sinf(ang);
        float l = sqrtf(dx * dx + dy * dy + dz * dz);
        beamDirv[b][0] = dx / l;
        beamDirv[b][1] = dy / l;
        beamDirv[b][2] = dz / l;
    }
}

/* rotating light beams (transparent cones) */
static void drawBeamCones(void)
{
    int b, k;
    float vis;

    if (!beamOn) return;
    vis = (0.04f + nightF * (0.55f + 0.6f * storm)) * 0.12f;

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    for (b = 0; b < 2; b++)
    {
        float dx = beamDirv[b][0], dy = beamDirv[b][1], dz = beamDirv[b][2];
        float yaw = RAD2DEG(atan2f(dx, dz));
        float tilt = RAD2DEG(-asinf(dy));

        glPushMatrix();
        glTranslatef(0.0f, LAMP_Y, 0.0f);
        glRotatef(yaw, 0.0f, 1.0f, 0.0f);
        glRotatef(tilt, 1.0f, 0.0f, 0.0f);
        for (k = 0; k < 3; k++)
        {
            float rs = 1.0f - 0.3f * k;
            glColor4f(1.0f, 0.95f, 0.65f, vis * (1.0f + 0.6f * k));
            gluCylinder(quad, 0.12 * rs, 5.5 * rs, 50.0, 18, 1);
        }
        glPopMatrix();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

/* ---------------------------------------------------------------- */
/* sky objects, clouds, rain, lightning                             */
/* ---------------------------------------------------------------- */
static void drawSkyObjects(float ex, float ey, float ez)
{
    int i;
    float sa = nightF * (1.0f - storm * 0.9f) * (1.0f - flash);

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(ex, ey, ez);

    if (sa > 0.02f)
    {
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        glColor4f(1.0f, 1.0f, 1.0f, sa);
        for (i = 0; i < NSTARS; i++)
            glVertex3f(starPos[i][0], starPos[i][1], starPos[i][2]);
        glEnd();
    }

    if (sunE > -0.2f)
    {
        float al = 1.0f - 0.85f * storm;
        glPushMatrix();
        glTranslatef(sunDirW[0] * 150.0f, sunDirW[1] * 150.0f, sunDirW[2] * 150.0f);
        glColor4f(1.0f, 0.85f, 0.4f, 0.20f * al);
        glutSolidSphere(13.0, 16, 12);
        glColor4f(1.0f, 0.93f, 0.60f, al);
        glutSolidSphere(7.0, 16, 12);
        glPopMatrix();
    }
    if (sunE < 0.2f)
    {
        float al = 1.0f - 0.85f * storm;
        glPushMatrix();
        glTranslatef(moonDirW[0] * 150.0f, moonDirW[1] * 150.0f, moonDirW[2] * 150.0f);
        glColor4f(0.85f, 0.9f, 1.0f, al);
        glutSolidSphere(4.5, 16, 12);
        glPopMatrix();
    }

    glPopMatrix();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

static void drawClouds(void)
{
    int c, k;
    float base = mixf(0.12f, 0.97f, dayF);
    float g = mixf(base, 0.22f * (0.3f + 0.7f * dayF), 0.8f * storm);
    g = clampf(g + flash * 0.5f, 0.0f, 1.0f);

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (c = 0; c < NCLOUD; c++)
    {
        for (k = 0; k < 5; k++)
        {
            float shade = g * (0.92f + 0.08f * k / 4.0f);
            glPushMatrix();
            glTranslatef(cloudX[c] + cloudOff[c][k][0] * cloudS[c],
                         24.0f - 3.0f * storm + cloudOff[c][k][1],
                         cloudZ[c] + cloudOff[c][k][2] * cloudS[c]);
            glScalef(1.6f, 0.45f, 1.1f);
            glColor4f(shade, shade, shade * 1.03f, 0.82f);
            glutSolidSphere(4.0 * cloudS[c], 14, 8);
            glPopMatrix();
        }
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void drawRain(void)
{
    int i, n;

    if (storm < 0.05f) return;
    n = (int)(NRAIN * clampf(storm / 0.8f, 0.15f, 1.0f));

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(0.75f, 0.85f, 1.0f, 0.45f);
    glBegin(GL_LINES);
    for (i = 0; i < n; i++)
    {
        glVertex3f(rainP[i][0], rainP[i][1], rainP[i][2]);
        glVertex3f(rainP[i][0] - 0.12f, rainP[i][1] - 0.9f, rainP[i][2]);
    }
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void genBolt(void)
{
    int k;
    float sx = (frand() - 0.5f) * 50.0f, sz = (frand() - 0.5f) * 50.0f;
    float ex = sx + (frand() - 0.5f) * 8.0f, ez = sz + (frand() - 0.5f) * 8.0f;

    for (k = 0; k < 12; k++)
    {
        float t = k / 11.0f;
        float jx = (k > 0 && k < 11) ? (frand() - 0.5f) * 3.0f : 0.0f;
        float jz = (k > 0 && k < 11) ? (frand() - 0.5f) * 3.0f : 0.0f;
        bolt[k][0] = mixf(sx, ex, t) + jx;
        bolt[k][1] = mixf(26.0f, 0.2f, t);
        bolt[k][2] = mixf(sz, ez, t) + jz;
    }
}

static void drawBolt(void)
{
    int k;
    float a;

    if (boltLife <= 0.0f) return;
    a = (frand() > 0.3f) ? 1.0f : 0.4f;

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(7.0f);
    glColor4f(0.6f, 0.7f, 1.0f, 0.30f * a);
    glBegin(GL_LINE_STRIP);
    for (k = 0; k < 12; k++) glVertex3f(bolt[k][0], bolt[k][1], bolt[k][2]);
    glEnd();
    glLineWidth(2.0f);
    glColor4f(1.0f, 1.0f, 1.0f, a);
    glBegin(GL_LINE_STRIP);
    for (k = 0; k < 12; k++) glVertex3f(bolt[k][0], bolt[k][1], bolt[k][2]);
    glEnd();
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

/* ---------------------------------------------------------------- */
/* seagulls, chimney smoke, lamps                                   */
/* ---------------------------------------------------------------- */

/* one seagull: body forward = +x, up = +y, wings along z */
static void drawGull(float flap, float al)
{
    int s;

    /* body */
    glColor4f(0.96f, 0.96f, 0.96f, al);
    glPushMatrix();
    glScalef(0.42f, 0.13f, 0.15f);
    glutSolidSphere(1.0, 12, 8);
    glPopMatrix();

    /* head */
    glPushMatrix();
    glTranslatef(0.40f, 0.06f, 0.0f);
    glutSolidSphere(0.075, 8, 6);
    glPopMatrix();

    /* beak */
    glColor4f(1.0f, 0.65f, 0.10f, al);
    glPushMatrix();
    glTranslatef(0.46f, 0.055f, 0.0f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glutSolidCone(0.022, 0.12, 6, 1);
    glPopMatrix();

    /* tail */
    glColor4f(0.85f, 0.85f, 0.85f, al);
    glBegin(GL_TRIANGLES);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-0.38f, 0.02f, 0.0f);
    glVertex3f(-0.65f, 0.0f, 0.10f);
    glVertex3f(-0.65f, 0.0f, -0.10f);
    glEnd();

    /* wings: inner part (white) + outer tip (dark grey), mirrored left/right */
    for (s = -1; s <= 1; s += 2)
    {
        float sg = (float)s;

        glPushMatrix();
        glTranslatef(0.0f, 0.05f, sg * 0.10f);
        glRotatef(sg * flap, 1.0f, 0.0f, 0.0f);

        glColor4f(0.95f, 0.95f, 0.95f, al);
        glBegin(GL_QUADS);
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0.18f, 0.0f, 0.0f);
        glVertex3f(-0.22f, 0.0f, 0.0f);
        glVertex3f(-0.32f, 0.0f, sg * 0.55f);
        glVertex3f(0.02f, 0.0f, sg * 0.55f);
        glEnd();

        glTranslatef(0.0f, 0.0f, sg * 0.55f);
        glRotatef(sg * flap * 0.7f, 1.0f, 0.0f, 0.0f);

        glColor4f(0.25f, 0.27f, 0.30f, al);
        glBegin(GL_TRIANGLES);
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0.02f, 0.0f, 0.0f);
        glVertex3f(-0.32f, 0.0f, 0.0f);
        glVertex3f(-0.50f, 0.0f, sg * 0.55f);
        glEnd();

        glPopMatrix();
    }
}

static void drawGulls(void)
{
    int i, n;
    /* fades out at dusk, completely gone at night */
    float al = smoothf(0.20f, 0.60f, dayF);

    if (!gullsOn || al < 0.02f) return;

    /* fewer gulls fly when the storm is strong */
    n = (int)(NGULL * (1.0f - 0.7f * storm) + 0.5f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setSpec(0.1f, 10.0f);

    for (i = 0; i < n; i++)
    {
        float th = gullTh[i];
        float cs = cosf(th), sn = sinf(th);
        float d  = (gullW[i] > 0.0f) ? 1.0f : -1.0f;
        float hx = -sn * d, hz = cs * d;                 /* heading */
        float rad = gullR[i] + 2.5f * sinf(birdT * 0.10f + gullPh[i]);
        float x = rad * cs;
        float z = rad * sn;
        /* slow height changes: birds appear at different heights */
        float y = gullY[i] + 0.8f * sinf(birdT * 0.7f + gullPh[i])
                           + 2.5f * sinf(birdT * 0.15f + gullPh[i] * 3.0f);
        float amp  = 20.0f + 18.0f * sinf(birdT * 0.35f + gullPh[i] * 2.0f);
        float flap = amp * sinf(birdT * (7.0f + 3.0f * storm) + gullPh[i]);

        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(RAD2DEG(atan2f(-hz, hx)), 0.0f, 1.0f, 0.0f);
        glRotatef(12.0f * d, 1.0f, 0.0f, 0.0f);          /* bank into the turn */
        glScalef(1.5f, 1.5f, 1.5f);
        drawGull(flap, al);
        glPopMatrix();
    }

    glDisable(GL_BLEND);
}

/* chimney smoke: drifts with the wind (-x), grows and fades */
static void drawSmoke(void)
{
    int i;
    float g = mixf(0.22f, 0.70f, dayF);
    float wind = 0.6f + 1.8f * storm;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (i = 0; i < NSMOKE; i++)
    {
        float t = smokeAge[i] / SMOKE_LIFE;             /* 0..1 */
        float x = HUT_X + 0.22f - wind * smokeAge[i] * 0.6f + smokeJit[i][0] * t;
        float y = PLATEAU + 1.08f + smokeAge[i] * 0.55f;
        float z = 0.12f + smokeJit[i][1] * t;

        glPushMatrix();
        glTranslatef(x, y, z);
        glColor4f(g, g, g, 0.45f * (1.0f - t));
        glutSolidSphere(0.07f + 0.28f * t, 8, 6);
        glPopMatrix();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

/* glowing lamp: solid bulb + soft additive halo (k = brightness 0..1) */
static void glowSpot(float x, float y, float z, float r,
                     float cr, float cg, float cb, float k)
{
    float b = 0.30f + 0.70f * k;

    glPushMatrix();
    glTranslatef(x, y, z);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(cr * b, cg * b, cb * b, 1.0f);
    glutSolidSphere(r, 10, 8);
    if (k > 0.05f)
    {
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glColor4f(cr, cg, cb, 0.22f * k);
        glutSolidSphere(r * 3.2f, 12, 8);
        glColor4f(cr, cg, cb, 0.10f * k);
        glutSolidSphere(r * 5.5f, 12, 8);
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glPopMatrix();
}

/* outdoor lamps: house porch, dock end, rescue-station antenna beacon */
static void drawLamps(void)
{
    float k = nightF;
    float dx, dz, c, s, rx, rz, blink;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* keeper's house porch lamp */
    glowSpot(HUT_X + 0.40f, PLATEAU + 0.325f + 0.14f, 0.60f, 0.045f, 1.0f, 0.85f, 0.45f, k);

    /* lantern at the end of the dock */
    dockPoint(DOCK_L, -0.55f, &dx, &dz);
    glowSpot(dx, 1.14f, dz, 0.06f, 1.0f, 0.85f, 0.45f, k);

    /* red blinking beacon on the rescue antenna */
    c = cosf(RESCUE_ROT * PI / 180.0f);
    s = sinf(RESCUE_ROT * PI / 180.0f);
    rx = RESCUE_X + 0.45f * c + (-0.25f) * s;
    rz = RESCUE_Z - 0.45f * s + (-0.25f) * c;
    blink = (sinf(simTime * 4.0f) > 0.0f) ? 1.0f : 0.15f;
    glowSpot(rx, groundH(RESCUE_X, RESCUE_Z) + 0.06f + 2.28f, rz, 0.05f,
             1.0f, 0.10f, 0.10f, (0.35f + 0.65f * k) * blink);

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void updateAmbientLife(float dt)
{
    int i;

    if (birdsOn)
    {
        birdT += dt;
        for (i = 0; i < NGULL; i++)
            gullTh[i] += gullW[i] * (1.0f + 0.6f * storm) * dt;
    }
    if (fireAnim)
        fireT += dt;

    for (i = 0; i < NSMOKE; i++)
    {
        smokeAge[i] += dt;
        if (smokeAge[i] > SMOKE_LIFE)
        {
            smokeAge[i] -= SMOKE_LIFE;
            smokeJit[i][0] = (frand() - 0.5f) * 0.4f;
            smokeJit[i][1] = (frand() - 0.5f) * 0.4f;
        }
    }
}

/* ---------------------------------------------------------------- */
/* HUD                                                              */
/* ---------------------------------------------------------------- */
static void drawText(float x, float y, const char *s)
{
    glRasterPos2f(x, y);
    for (; *s; ++s)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s);
}

static void drawBigText(float x, float y, const char *s)
{
    glRasterPos2f(x, y);
    for (; *s; ++s)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s);
}

static const char *onoff(int v)
{
    return v ? "ON" : "off";
}

static void drawHUD(void)
{
    char buf[256];
    int hh = (int)tod, mm = (int)((tod - hh) * 60.0f);
    const char *camNames[3] = { "Orbit", "Boat follow", "Keeper" };

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    /* title + status bar (top) and controls box (bottom-left corner) */
    glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
    glBegin(GL_QUADS);
    glVertex2f(8, winH - 56); glVertex2f(winW - 8, winH - 56);
    glVertex2f(winW - 8, winH - 6); glVertex2f(8, winH - 6);
    glVertex2f(8, 6); glVertex2f(560, 6);
    glVertex2f(560, 76); glVertex2f(8, 76);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawBigText(20, winH - 28, "LIGHTHOUSE ISLAND - OpenGL / GLUT");

    sprintf(buf, "Time %02d:%02d (%s)%s  Weather: %s %d%%  Ship %s  Waves %s  Beam %s  Birds %s  Fire anim %s  Camera: %s  FPS %.0f",
            hh, mm, dayF > 0.5f ? "day" : "night", autoTime ? " auto" : "",
            stormMode ? "STORM" : "calm", (int)(storm * 100.0f + 0.5f),
            onoff(shipOn), onoff(wavesOn), onoff(beamOn), onoff(birdsOn),
            onoff(fireAnim), camNames[camMode], fps);
    glColor3f(0.85f, 0.95f, 1.0f);
    drawText(20, winH - 47, buf);

    glColor3f(0.8f, 0.9f, 1.0f);
    drawText(16, 62, "D Day  N Night  A Auto cycle  L Beam  S Ship  W Waves");
    drawText(16, 48, "R Storm  B Birds  F Campfire anim  G Gulls  H HUD  ESC Exit");
    drawText(16, 34, "Q/E storm +/-  1/2/3 camera  mouse L-drag orbit, R-drag zoom");
    drawText(16, 20, "K lightning  O fog  P shadows  M wire sea  Z/X time  SPACE pause");
    if (paused)
    {
        glColor3f(1.0f, 0.8f, 0.3f);
        drawText(winW - 90, winH - 28, "PAUSED");
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

/* ---------------------------------------------------------------- */
/* display                                                          */
/* ---------------------------------------------------------------- */
static void display(void)
{
    float ex, ey, ez, tx, ty, tz;
    GLfloat lpos[4], bpos[4], bdir[3], bdif[4];
    GLfloat fogc[4], fpos[4], fdif[4];
    float fl, fint;
    int b;

    glClearColor(skyCol[0], skyCol[1], skyCol[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(55.0, (double)winW / (double)winH, 0.2, 400.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* camera */
    if (camMode == 0)
    {
        tx = 0.0f; ty = 3.5f; tz = 0.0f;
        ex = tx + camDist * cosf(camPitch) * sinf(camYaw);
        ey = ty + camDist * sinf(camPitch);
        ez = tz + camDist * cosf(camPitch) * cosf(camYaw);
    }
    else if (camMode == 1)
    {
        ex = boatX - boatHX * 11.0f;
        ey = boatY + 4.5f;
        ez = boatZ - boatHZ * 11.0f;
        tx = boatX + boatHX * 3.0f;
        ty = boatY + 1.5f;
        tz = boatZ + boatHZ * 3.0f;
    }
    else
    {
        float hl = sqrtf(beamDirv[0][0] * beamDirv[0][0] + beamDirv[0][2] * beamDirv[0][2]);
        float hx = beamDirv[0][0] / hl, hz = beamDirv[0][2] / hl;
        ex = -hx * 1.0f;
        ey = LAMP_Y + 0.3f;
        ez = -hz * 1.0f;
        tx = ex + hx * 20.0f;
        ty = ey - 4.0f;
        tz = ez + hz * 20.0f;
    }
    gluLookAt(ex, ey, ez, tx, ty, tz, 0.0, 1.0, 0.0);

    /* lights (positions are set after the camera transform) */
    lpos[0] = lightDirv[0]; lpos[1] = lightDirv[1]; lpos[2] = lightDirv[2]; lpos[3] = 0.0f;
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDiff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, sunDiff);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambCol);

    for (b = 0; b < 2; b++)
    {
        float inten = beamOn ? (0.45f + 0.9f * nightF) : 0.0f;
        GLenum L = (b == 0) ? GL_LIGHT1 : GL_LIGHT2;
        bpos[0] = 0.0f; bpos[1] = LAMP_Y; bpos[2] = 0.0f; bpos[3] = 1.0f;
        bdir[0] = beamDirv[b][0]; bdir[1] = beamDirv[b][1]; bdir[2] = beamDirv[b][2];
        bdif[0] = 1.0f * inten; bdif[1] = 0.92f * inten; bdif[2] = 0.60f * inten; bdif[3] = 1.0f;
        glLightfv(L, GL_POSITION, bpos);
        glLightfv(L, GL_SPOT_DIRECTION, bdir);
        glLightfv(L, GL_DIFFUSE, bdif);
        glLightfv(L, GL_SPECULAR, bdif);
    }

    /* campfire light: warm point light, strongest at night, flickering */
    fl = 0.85f + 0.15f * sinf(fireT * 17.0f) + 0.08f * sinf(fireT * 31.0f);
    fint = (0.25f + 0.85f * nightF) * fl;
    fpos[0] = FIRE_X; fpos[1] = groundH(FIRE_X, FIRE_Z) + 0.5f; fpos[2] = FIRE_Z; fpos[3] = 1.0f;
    fdif[0] = 1.0f * fint; fdif[1] = 0.50f * fint; fdif[2] = 0.18f * fint; fdif[3] = 1.0f;
    glLightfv(GL_LIGHT3, GL_POSITION, fpos);
    glLightfv(GL_LIGHT3, GL_DIFFUSE, fdif);

    /* sky dome objects */
    drawSkyObjects(ex, ey, ez);

    /* common state */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);
    glEnable(GL_LIGHT3);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    if (fogOn)
    {
        fogc[0] = skyCol[0]; fogc[1] = skyCol[1]; fogc[2] = skyCol[2]; fogc[3] = 1.0f;
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_EXP2);
        glFogfv(GL_FOG_COLOR, fogc);
        glFogf(GL_FOG_DENSITY, 0.006f + 0.022f * storm + (storm > 0.05f ? 0.003f : 0.0f));
    }
    else
        glDisable(GL_FOG);

    /* island (seabed, beach, grass) */
    setSpec(0.05f, 10.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGround);
    glCallList(islandList);
    glDisable(GL_TEXTURE_2D);

    /* pathway, vegetation, rocks, beach objects */
    drawPath();
    drawGrass();
    drawBushes();
    drawFlowers();
    drawRocks();
    drawBeachDetails();
    drawPalms();

    /* lighthouse + keeper's house + sign */
    gShadow = 0;
    setSpec(0.25f, 25.0f);
    glPushMatrix();
    glTranslatef(0.0f, PLATEAU, 0.0f);
    drawLighthouse();
    glPopMatrix();
    glPushMatrix();
    glTranslatef(HUT_X, PLATEAU + 0.325f, 0.0f);
    drawHut();
    glPopMatrix();
    setSpec(0.05f, 10.0f);
    drawSign();

    /* rescue station, dock (with boat) and campfire logs */
    drawRescueStation();
    drawDock();
    drawCampfireBase();

    /* shadows */
    if (shadowsOn && hasStencil && lightDirv[1] > 0.12f)
        drawShadows();

    /* ship */
    setSpec(0.15f, 20.0f);
    drawBoat();

    /* sea (translucent, drawn after opaque objects) */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawSea();
    glDisable(GL_BLEND);
    setSpec(0.05f, 10.0f);

    /* translucent / unlit effects */
    drawShoreFoam();
    drawLanternGlassAndLamp();
    drawWake();
    drawCampfireFlame();
    drawLamps();
    drawSmoke();
    drawGulls();
    drawBeamCones();
    drawClouds();
    drawRain();
    drawBolt();

    if (hudOn) drawHUD();

    glutSwapBuffers();
}

static void reshape(int w, int h)
{
    winW = w;
    winH = (h > 0) ? h : 1;
    glViewport(0, 0, winW, winH);
}

/* ---------------------------------------------------------------- */
/* update (called from the GLUT timer)                              */
/* ---------------------------------------------------------------- */
static void update(float dt)
{
    int i;

    if (!paused)
    {
        simTime += dt;
        if (wavesOn) waveT += dt;
        if (autoTime)
        {
            tod += todRate * dt;
            if (tod >= 24.0f) tod -= 24.0f;
        }
        beamAngle += 1.1f * dt;
        if (beamAngle > TWO_PI) beamAngle -= TWO_PI;

        /* storm builds up / fades out smoothly */
        storm += (stormTarget - storm) * clampf(dt * 1.5f, 0.0f, 1.0f);
        if (storm < 0.002f && stormTarget <= 0.0f) storm = 0.0f;

        updateBoat(dt);
        updateAmbientLife(dt);

        for (i = 0; i < NRAIN; i++)
        {
            rainP[i][1] -= 26.0f * dt;
            rainP[i][0] -= 3.0f * dt;
            if (rainP[i][1] < 0.0f)
            {
                rainP[i][0] = (frand() - 0.5f) * 70.0f;
                rainP[i][1] = 28.0f;
                rainP[i][2] = (frand() - 0.5f) * 70.0f;
            }
        }

        for (i = 0; i < NCLOUD; i++)
        {
            cloudX[i] += dt * (1.0f + 5.0f * storm);
            if (cloudX[i] > 110.0f) cloudX[i] = -110.0f;
        }

        /* lightning (only in storm mode) */
        if (lightningOn && storm > 0.35f)
        {
            boltTimer -= dt;
            if (boltTimer <= 0.0f)
            {
                genBolt();
                flash = 1.0f;
                boltLife = 0.18f;
                boltTimer = 1.5f + frand() * 6.0f * (1.3f - storm);
            }
        }
        flash -= dt * 4.0f;
        if (flash < 0.0f) flash = 0.0f;
        boltLife -= dt;
    }

    if (tod < 0.0f) tod += 24.0f;
    if (tod >= 24.0f) tod -= 24.0f;

    updateSea();
    updateBeamDirs();
    updateEnvironment();
}

static void timerFunc(int v)
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastMs) / 1000.0f;

    (void)v;
    lastMs = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    if (dt > 0.0001f) fps = mixf(fps, 1.0f / dt, 0.1f);

    update(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timerFunc, 0);
}

/* ---------------------------------------------------------------- */
/* input                                                            */
/* ---------------------------------------------------------------- */
static void keyboard(unsigned char key, int x, int y)
{
    (void)x; (void)y;
    switch (key)
    {
    case 27: exit(0); break;
    case ' ': paused = !paused; break;

    case 'd': case 'D': tod = 12.0f; autoTime = 0; break;      /* day mode   */
    case 'n': case 'N': tod = 22.0f; autoTime = 0; break;      /* night mode */
    case 'a': case 'A': autoTime = !autoTime; break;           /* auto cycle */
    case 'l': case 'L': beamOn = !beamOn; break;               /* beam       */
    case 's': case 'S': shipOn = !shipOn; break;               /* ship moves */
    case 'w': case 'W': wavesOn = !wavesOn; break;             /* waves      */
    case 'r': case 'R':                                        /* storm mode */
        stormMode = !stormMode;
        stormTarget = stormMode ? 0.85f : 0.0f;
        break;
    case 'q': case 'Q': if (stormMode) stormTarget = clampf(stormTarget + 0.1f, 0.3f, 1.0f); break;
    case 'e': case 'E': if (stormMode) stormTarget = clampf(stormTarget - 0.1f, 0.3f, 1.0f); break;
    case 'b': case 'B': birdsOn = !birdsOn; break;             /* birds move */
    case 'f': case 'F': fireAnim = !fireAnim; break;           /* fire anim  */
    case 'g': case 'G': gullsOn = !gullsOn; break;             /* show gulls */

    case 'k': case 'K': lightningOn = !lightningOn; break;
    case 'o': case 'O': fogOn = !fogOn; break;
    case 'p': case 'P': if (hasStencil) shadowsOn = !shadowsOn; break;
    case 'm': case 'M': wireSea = !wireSea; break;
    case 'h': case 'H': hudOn = !hudOn; break;
    case 'z': case 'Z': tod -= 0.5f; break;
    case 'x': case 'X': tod += 0.5f; break;
    case '1': camMode = 0; break;
    case '2': camMode = 1; break;
    case '3': camMode = 2; break;
    case '+': case '=': camDist = clampf(camDist - 2.0f, 8.0f, 90.0f); break;
    case '-': case '_': camDist = clampf(camDist + 2.0f, 8.0f, 90.0f); break;
    }
    if (tod < 0.0f) tod += 24.0f;
    if (tod >= 24.0f) tod -= 24.0f;
}

static void mouse(int button, int state, int x, int y)
{
    dragBtn = (state == GLUT_DOWN) ? button : -1;
    lastX = x;
    lastY = y;
}

static void motion(int x, int y)
{
    int dx = x - lastX, dy = y - lastY;

    if (dragBtn == GLUT_LEFT_BUTTON)
    {
        camYaw -= dx * 0.008f;
        camPitch = clampf(camPitch + dy * 0.006f, 0.05f, 1.45f);
    }
    else if (dragBtn == GLUT_RIGHT_BUTTON)
    {
        camDist = clampf(camDist + dy * 0.12f, 8.0f, 90.0f);
    }
    lastX = x;
    lastY = y;
}

/* ---------------------------------------------------------------- */
/* init                                                             */
/* ---------------------------------------------------------------- */
static void initScene(void)
{
    int i, k;
    GLint bits = 0;
    GLfloat white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glClearStencil(0);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glGetIntegerv(GL_STENCIL_BITS, &bits);
    hasStencil = (bits > 0);

    /* beam spotlights */
    for (i = 0; i < 2; i++)
    {
        GLenum L = (i == 0) ? GL_LIGHT1 : GL_LIGHT2;
        glLightf(L, GL_SPOT_CUTOFF, 18.0f);
        glLightf(L, GL_SPOT_EXPONENT, 8.0f);
        glLightf(L, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(L, GL_LINEAR_ATTENUATION, 0.01f);
    }
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);

    /* campfire point light */
    glLightf(GL_LIGHT3, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT3, GL_LINEAR_ATTENUATION, 0.25f);
    glLightf(GL_LIGHT3, GL_QUADRATIC_ATTENUATION, 0.08f);

    quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);

    initWaves();
    initSeaMaps();
    makeTextures();
    buildIsland();
    buildPath();
    genScatter();

    for (i = 0; i < NSTARS; i++)
    {
        float az = frand() * TWO_PI;
        float el = asinf(frand() * 0.98f + 0.02f);
        starPos[i][0] = 190.0f * cosf(el) * cosf(az);
        starPos[i][1] = 190.0f * sinf(el);
        starPos[i][2] = 190.0f * cosf(el) * sinf(az);
    }
    for (i = 0; i < NRAIN; i++)
    {
        rainP[i][0] = (frand() - 0.5f) * 70.0f;
        rainP[i][1] = frand() * 28.0f;
        rainP[i][2] = (frand() - 0.5f) * 70.0f;
    }
    for (i = 0; i < NCLOUD; i++)
    {
        cloudX[i] = (frand() - 0.5f) * 200.0f;
        cloudZ[i] = (frand() - 0.5f) * 160.0f;
        cloudS[i] = 1.0f + frand() * 1.2f;
        for (k = 0; k < 5; k++)
        {
            cloudOff[i][k][0] = (frand() - 0.5f) * 9.0f;
            cloudOff[i][k][1] = (frand() - 0.5f) * 1.5f;
            cloudOff[i][k][2] = (frand() - 0.5f) * 5.0f;
        }
    }

    /* seagulls */
    for (i = 0; i < NGULL; i++)
    {
        float v = 3.0f + frand() * 2.5f;                 /* linear speed */
        gullR[i]  = 8.0f + frand() * 12.0f;
        gullTh[i] = frand() * TWO_PI;
        gullW[i]  = (v / gullR[i]) * (frand() > 0.5f ? 1.0f : -1.0f);
        gullY[i]  = 8.0f + frand() * 8.0f;
        gullPh[i] = frand() * TWO_PI;
    }
    /* chimney smoke */
    for (i = 0; i < NSMOKE; i++)
    {
        smokeAge[i]    = frand() * SMOKE_LIFE;
        smokeJit[i][0] = (frand() - 0.5f) * 0.4f;
        smokeJit[i][1] = (frand() - 0.5f) * 0.4f;
    }

    updateBeamDirs();
    updateEnvironment();
    updateSea();
    updateBoat(0.0f);
}

int main(int argc, char **argv)
{
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    glutInitWindowSize(winW, winH);
    glutInitWindowPosition(60, 30);
    glutCreateWindow("Lighthouse Island - OpenGL/GLUT");

    initScene();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    lastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timerFunc, 0);

    glutMainLoop();
    return 0;
}
