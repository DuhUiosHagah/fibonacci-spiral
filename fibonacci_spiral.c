// Fibonacci Spiral Visualizer
// Interactive golden spiral built with C + raylib.
//
// Build (Windows / MinGW):
//   gcc fibonacci_spiral.c -o fibonacci_spiral.exe -lraylib -lopengl32 -lgdi32 -lwinmm
//
// Controls:
//   LEFT / RIGHT  -> fewer / more terms
//   SPACE         -> pause / resume animation
//   R             -> restart animation

#include "raylib.h"
#include <math.h>

#define MAX_TERMS 18
#define MIN_TERMS 2
#define PANEL_W   300
#define WIN_W     1180
#define WIN_H     760
#define PHI       1.6180339887498949

// ---------- Palette ----------
static const Color COL_BG      = {16, 18, 26, 255};
static const Color COL_PANEL   = {24, 27, 38, 255};
static const Color COL_LINE    = {44, 49, 66, 255};
static const Color COL_TEXT    = {232, 235, 245, 255};
static const Color COL_MUTED   = {128, 136, 162, 255};
static const Color COL_GOLD    = {255, 196, 61, 255};
static const Color COL_BTN     = {38, 43, 60, 255};
static const Color COL_BTN_HOV = {52, 59, 82, 255};
static const Color COL_BTN_ON  = {70, 82, 128, 255};

// ---------- Data ----------
typedef struct { double x, y, size; } Square;
typedef struct { double cx, cy, r, a0, a1; } Arc;   // angles in degrees
typedef struct { float scale, ox, oy; } View;

static int    fib[MAX_TERMS];
static Square squares[MAX_TERMS];
static Arc    arcs[MAX_TERMS];
static double minX, minY, maxX, maxY;               // bounding box of all squares
static bool   g_hover = false;                      // any button hovered this frame

// Builds the Fibonacci numbers, the square layout and the spiral arcs.
static void BuildLayout(int n)
{
    fib[0] = 1;
    fib[1] = 1;
    for (int k = 2; k < MAX_TERMS; k++) fib[k] = fib[k - 1] + fib[k - 2];

    minX = 0; minY = 0; maxX = 1; maxY = 1;
    squares[0] = (Square){0, 0, 1};

    // Each new square is glued to one side of the current bounding box,
    // rotating: right -> top -> left -> bottom -> ...
    for (int k = 1; k < n; k++) {
        double s = fib[k];
        double x = 0, y = 0;
        switch ((k - 1) % 4) {
            case 0: x = maxX;     y = minY;     maxX += s; break;  // right
            case 1: x = minX;     y = minY - s; minY -= s; break;  // top
            case 2: x = minX - s; y = minY;     minX -= s; break;  // left
            case 3: x = minX;     y = maxY;     maxY += s; break;  // bottom
        }
        squares[k] = (Square){x, y, s};
    }

    // Quarter-circle arcs. Each arc starts where the previous one ended,
    // with the same tangent, so the curve is perfectly smooth.
    arcs[0] = (Arc){1, 0, 1, 180, 90};
    for (int k = 1; k < n; k++) {
        double ae = arcs[k - 1].a1 * DEG2RAD;
        double px = arcs[k - 1].cx + arcs[k - 1].r * cos(ae);
        double py = arcs[k - 1].cy + arcs[k - 1].r * sin(ae);
        double s  = fib[k];
        arcs[k].cx = px - s * cos(ae);
        arcs[k].cy = py - s * sin(ae);
        arcs[k].r  = s;
        arcs[k].a0 = arcs[k - 1].a1;
        arcs[k].a1 = arcs[k].a0 - 90;
    }
}

// View that fits the whole bounding box inside the drawing area.
static View TargetView(void)
{
    float areaX = PANEL_W + 24, areaY = 24;
    float areaW = WIN_W - PANEL_W - 48, areaH = WIN_H - 48;
    double bw = maxX - minX, bh = maxY - minY;

    View v;
    v.scale = (float)fmin(areaW / bw, areaH / bh) * 0.97f;
    v.ox = areaX + (areaW - (float)bw * v.scale) / 2 - (float)minX * v.scale;
    v.oy = areaY + (areaH - (float)bh * v.scale) / 2 - (float)minY * v.scale;
    return v;
}

static Color SquareColor(int k)
{
    return ColorFromHSV(fmodf(200.0f + k * 24.0f, 360.0f), 0.55f, 0.95f);
}

static float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// ---------- Drawing ----------
static void DrawSquareK(View v, int k, float alpha)
{
    Square s = squares[k];
    Rectangle r = {
        v.ox + (float)s.x * v.scale,
        v.oy + (float)s.y * v.scale,
        (float)s.size * v.scale,
        (float)s.size * v.scale
    };
    Color c = SquareColor(k);
    DrawRectangleRec(r, Fade(c, 0.22f * alpha));
    DrawRectangleLinesEx(r, 1.5f, Fade(c, 0.85f * alpha));

    // Label with the Fibonacci number, only if the square is big enough
    if (r.width > 30) {
        int fs = (int)fminf(r.width * 0.32f, 44.0f);
        const char *t = TextFormat("%d", fib[k]);
        int tw = MeasureText(t, fs);
        DrawText(t, (int)(r.x + r.width / 2 - tw / 2),
                    (int)(r.y + r.height / 2 - fs / 2), fs, Fade(COL_TEXT, 0.8f * alpha));
    }
}

static void DrawArcK(View v, int k, float frac, float thick)
{
    if (frac <= 0) return;
    Arc a = arcs[k];
    const int segs = 64;
    double end = a.a0 + (a.a1 - a.a0) * frac;

    Vector2 prev = {0, 0};
    for (int i = 0; i <= segs; i++) {
        double ang = (a.a0 + (end - a.a0) * i / segs) * DEG2RAD;
        Vector2 p = {
            v.ox + (float)(a.cx + a.r * cos(ang)) * v.scale,
            v.oy + (float)(a.cy + a.r * sin(ang)) * v.scale
        };
        if (i > 0) {
            DrawLineEx(prev, p, thick, COL_GOLD);
            DrawCircleV(p, thick / 2, COL_GOLD);   // round joints
        }
        prev = p;
    }
}

// ---------- UI ----------
static bool Button(Rectangle r, const char *label, bool active)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    if (hover) g_hover = true;

    Color col = active ? COL_BTN_ON : (hover ? COL_BTN_HOV : COL_BTN);
    DrawRectangleRounded(r, 0.25f, 8, col);

    int fs = 18;
    int tw = MeasureText(label, fs);
    DrawText(label, (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + r.height / 2 - fs / 2), fs, COL_TEXT);

    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static void Label(const char *text, int x, int y)
{
    DrawText(text, x, y, 13, COL_MUTED);
}

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WIN_W, WIN_H, "Espiral de Fibonacci");
    SetTargetFPS(60);

    int   n = 10;
    bool  paused = false, showSquares = true;
    float progress = 0.0f;           // how many squares are already drawn (animated)
    const float speed = 1.4f;        // squares per second

    BuildLayout(n);
    View view = TargetView();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        g_hover = false;

        // ----- Keyboard -----
        int newN = n;
        if (IsKeyPressed(KEY_RIGHT)) newN++;
        if (IsKeyPressed(KEY_LEFT))  newN--;
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_R))     progress = 0;

        // ----- Animation -----
        if (!paused && progress < n) progress += dt * speed;
        if (progress > n) progress = (float)n;

        // ----- Smooth camera -----
        View target = TargetView();
        float k = 1.0f - expf(-9.0f * dt);
        view.scale += (target.scale - view.scale) * k;
        view.ox    += (target.ox    - view.ox)    * k;
        view.oy    += (target.oy    - view.oy)    * k;

        BeginDrawing();
        ClearBackground(COL_BG);

        // ----- Spiral -----
        for (int i = 0; i < n; i++) {
            float a = Clamp01(progress - i);
            if (a <= 0) break;
            if (showSquares) DrawSquareK(view, i, a);
        }
        for (int i = 0; i < n; i++) {
            float a = Clamp01(progress - i);
            if (a <= 0) break;
            DrawArcK(view, i, a, 3.5f);
        }

        // ----- Side panel -----
        DrawRectangle(0, 0, PANEL_W, WIN_H, COL_PANEL);
        DrawRectangle(PANEL_W - 1, 0, 1, WIN_H, COL_LINE);

        DrawText("Espiral de Fibonacci", 24, 24, 24, COL_TEXT);
        DrawText("Visualizador interativo em C + raylib", 24, 54, 13, COL_MUTED);

        Label("TERMOS", 24, 104);
        if (Button((Rectangle){24, 126, 48, 44}, "-", false)) newN--;
        if (Button((Rectangle){PANEL_W - 24 - 48, 126, 48, 44}, "+", false)) newN++;
        const char *nTxt = TextFormat("%d", n);
        int nw = MeasureText(nTxt, 34);
        DrawText(nTxt, PANEL_W / 2 - nw / 2, 131, 34, COL_GOLD);

        if (Button((Rectangle){24, 192, PANEL_W - 48, 40}, paused ? "Continuar" : "Pausar", paused))
            paused = !paused;
        if (Button((Rectangle){24, 242, PANEL_W - 48, 40}, "Reiniciar animacao", false))
            progress = 0;
        if (Button((Rectangle){24, 292, PANEL_W - 48, 40}, showSquares ? "Quadrados: ligado" : "Quadrados: desligado", showSquares))
            showSquares = !showSquares;

        // Sequence (revealed as the animation advances)
        Label("SEQUENCIA", 24, 360);
        int sx = 24, sy = 384;
        for (int i = 0; i < n && i < (int)progress + 1; i++) {
            const char *t = TextFormat("%d", fib[i]);
            int tw = MeasureText(t, 18);
            if (sx + tw > PANEL_W - 24) { sx = 24; sy += 26; }
            DrawText(t, sx, sy, 18, SquareColor(i));
            sx += tw + 12;
        }

        // Golden ratio info
        double ratio = (double)fib[n - 1] / (double)fib[n - 2];
        Label("RAZAO F(n) / F(n-1)", 24, WIN_H - 190);
        DrawText(TextFormat("%.8f", ratio), 24, WIN_H - 170, 24, COL_TEXT);
        Label("RAZAO AUREA (phi)", 24, WIN_H - 130);
        DrawText(TextFormat("%.8f", PHI), 24, WIN_H - 110, 24, COL_GOLD);
        Label("DIFERENCA", 24, WIN_H - 70);
        DrawText(TextFormat("%.2e", fabs(ratio - PHI)), 24, WIN_H - 52, 18, COL_MUTED);

        EndDrawing();

        // ----- Apply term-count change -----
        if (newN < MIN_TERMS) newN = MIN_TERMS;
        if (newN > MAX_TERMS) newN = MAX_TERMS;
        if (newN != n) {
            n = newN;
            BuildLayout(n);
            if (progress > n) progress = (float)n;
        }

        SetMouseCursor(g_hover ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    }

    CloseWindow();
    return 0;
}