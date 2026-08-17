#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Global Screen Dimensions (Mutable via CLI)
int screen_w = 1280;
int screen_h = 720;
bool fullscreen = false;

// Cube Dimensions
#define TILE_W 48
#define TILE_H 24
#define CUBE_Z 24

// Tunnel Settings
#define TUNNEL_SEGMENTS 32
#define TUNNEL_RINGS 24

// Starfield Settings
#define NUM_STARS 400
#define STAR_SPEED 600.0f
#define MAX_DEPTH 1000.0f

typedef struct {
    float x, y, z;
} Star;

Star stars[NUM_STARS];

// Initialize 3D starfield
void init_stars() {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = ((float)rand() / RAND_MAX - 0.5f) * 3000.0f;
        stars[i].y = ((float)rand() / RAND_MAX - 0.5f) * 3000.0f;
        stars[i].z = ((float)rand() / RAND_MAX) * MAX_DEPTH + 10.0f;
    }
}

// Linear interpolation for color transitions
SDL_Color lerp_color(SDL_Color c1, SDL_Color c2, float t) {
    SDL_Color res;
    res.r = (Uint8)(c1.r + (c2.r - c1.r) * t);
    res.g = (Uint8)(c1.g + (c2.g - c1.g) * t);
    res.b = (Uint8)(c1.b + (c2.b - c1.b) * t);
    res.a = 255;
    return res;
}

// Generate a color from the Black -> Purple -> Grey -> White gradient for cubes
SDL_Color get_gradient_color(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    SDL_Color black  = { 15,  10,  20, 255};
    SDL_Color purple = {130,  30, 200, 255};
    SDL_Color grey   = {160, 160, 170, 255};
    SDL_Color white  = {245, 245, 255, 255};

    if (t < 0.33f) {
        return lerp_color(black, purple, t / 0.33f);
    } else if (t < 0.66f) {
        return lerp_color(purple, grey, (t - 0.33f) / 0.33f);
    } else {
        return lerp_color(grey, white, (t - 0.66f) / 0.34f);
    }
}

// Optimized: Draw the entire shaded isometric cube in a single GPU call
void draw_iso_cube(SDL_Renderer* renderer, float sx, float sy, SDL_Color base) {
    SDL_Color top_col   = {base.r, base.g, base.b, 255};
    SDL_Color left_col  = {(Uint8)(base.r * 0.7f), (Uint8)(base.g * 0.7f), (Uint8)(base.b * 0.7f), 255};
    SDL_Color right_col = {(Uint8)(base.r * 0.4f), (Uint8)(base.g * 0.4f), (Uint8)(base.b * 0.4f), 255};

    float hw = TILE_W / 2.0f;
    float hh = TILE_H / 2.0f;
    float z  = CUBE_Z;

    SDL_Vertex verts[18];

    // --- Top Face ---
    for (int i = 0; i < 6; i++) verts[i].color = top_col;
    verts[0].position.x = sx;           verts[0].position.y = sy;
    verts[1].position.x = sx - hw;      verts[1].position.y = sy + hh;
    verts[2].position.x = sx;           verts[2].position.y = sy + TILE_H;
    verts[3].position.x = sx;           verts[3].position.y = sy;
    verts[4].position.x = sx;           verts[4].position.y = sy + TILE_H;
    verts[5].position.x = sx + hw;      verts[5].position.y = sy + hh;

    // --- Left Face ---
    for (int i = 6; i < 12; i++) verts[i].color = left_col;
    verts[6].position.x = sx - hw;      verts[6].position.y = sy + hh;
    verts[7].position.x = sx;           verts[7].position.y = sy + TILE_H;
    verts[8].position.x = sx;           verts[8].position.y = sy + TILE_H + z;
    verts[9].position.x = sx - hw;      verts[9].position.y = sy + hh;
    verts[10].position.x = sx;          verts[10].position.y = sy + TILE_H + z;
    verts[11].position.x = sx - hw;     verts[11].position.y = sy + hh + z;

    // --- Right Face ---
    for (int i = 12; i < 18; i++) verts[i].color = right_col;
    verts[12].position.x = sx;          verts[12].position.y = sy + TILE_H;
    verts[13].position.x = sx + hw;     verts[13].position.y = sy + hh;
    verts[14].position.x = sx + hw;     verts[14].position.y = sy + hh + z;
    verts[15].position.x = sx;          verts[15].position.y = sy + TILE_H;
    verts[16].position.x = sx + hw;     verts[16].position.y = sy + hh + z;
    verts[17].position.x = sx;          verts[17].position.y = sy + TILE_H + z;

    SDL_RenderGeometry(renderer, NULL, verts, 18, NULL, 0);
}

float hash2d(int x, int y) {
    int n = x * 374761393 + y * 668265263;
    n = (n ^ (n >> 13)) * 1274126177;
    return (float)(n & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

// Render segmented, spinning greyscale gradient tunnel
void render_greyscale_tunnel(SDL_Renderer* renderer, float t, float center_x, float center_y) {
    float max_radius = sqrtf(screen_w * screen_w + screen_h * screen_h) * 0.6f;
    float spin = t * 0.15f; 

    for (int r = TUNNEL_RINGS - 1; r >= 0; r--) {
        float depth_inner = (float)r / (float)TUNNEL_RINGS;
        float depth_outer = (float)(r + 1) / (float)TUNNEL_RINGS;

        float rad_inner = powf(depth_inner, 2.2f) * max_radius;
        float rad_outer = powf(depth_outer, 2.2f) * max_radius;

        float depth_wave = 0.5f + 0.5f * sinf(depth_inner * 18.0f - t * 4.0f);
        float vignette = powf(depth_inner, 0.4f);

        for (int s = 0; s < TUNNEL_SEGMENTS; s++) {
            float angle1 = ((float)s / (float)TUNNEL_SEGMENTS) * 2.0f * (float)M_PI + spin;
            float angle2 = ((float)(s + 1) / (float)TUNNEL_SEGMENTS) * 2.0f * (float)M_PI + spin;

            float cos1 = cosf(angle1), sin1 = sinf(angle1);
            float cos2 = cosf(angle2), sin2 = sinf(angle2);

            SDL_Vertex verts[6];
            float segment_mult = ((s + r) % 2 == 0) ? 1.0f : 0.2f;
            float shade_val = depth_wave * vignette * segment_mult;

            Uint8 lum = (Uint8)(shade_val * 240.0f + 10.0f);
            SDL_Color color = {lum, lum, lum, 255};
            for (int i = 0; i < 6; i++) verts[i].color = color;

            verts[0].position.x = center_x + cos1 * rad_inner; verts[0].position.y = center_y + sin1 * rad_inner;
            verts[1].position.x = center_x + cos2 * rad_inner; verts[1].position.y = center_y + sin2 * rad_inner;
            verts[2].position.x = center_x + cos2 * rad_outer; verts[2].position.y = center_y + sin2 * rad_outer;

            verts[3].position.x = center_x + cos1 * rad_inner; verts[3].position.y = center_y + sin1 * rad_inner;
            verts[4].position.x = center_x + cos2 * rad_outer; verts[4].position.y = center_y + sin2 * rad_outer;
            verts[5].position.x = center_x + cos1 * rad_outer; verts[5].position.y = center_y + sin1 * rad_outer;

            SDL_RenderGeometry(renderer, NULL, verts, 6, NULL, 0);
        }
    }
}

// Optimized: Render the flying stars using hardware-accelerated lines
void render_starfield(SDL_Renderer* renderer, float dt, float center_x, float center_y) {
    float fov = 350.0f; 

    for (int i = 0; i < NUM_STARS; i++) {
        float prev_z = stars[i].z;
        stars[i].z -= STAR_SPEED * dt;

        if (stars[i].z <= 1.0f) {
            stars[i].x = ((float)rand() / RAND_MAX - 0.5f) * 3000.0f;
            stars[i].y = ((float)rand() / RAND_MAX - 0.5f) * 3000.0f;
            stars[i].z = MAX_DEPTH;
            prev_z = MAX_DEPTH;
        }

        int px = (int)((stars[i].x / prev_z) * fov + center_x);
        int py = (int)((stars[i].y / prev_z) * fov + center_y);
        
        int cx = (int)((stars[i].x / stars[i].z) * fov + center_x);
        int cy = (int)((stars[i].y / stars[i].z) * fov + center_y);

        float intensity_norm = 1.0f - (stars[i].z / MAX_DEPTH);
        Uint8 intensity = (Uint8)(255.0f * intensity_norm);
        
        SDL_SetRenderDrawColor(renderer, intensity, (Uint8)(intensity * 0.8f), (Uint8)(intensity * 1.0f), 255);
        SDL_RenderDrawLine(renderer, px, py, cx, cy);
    }
}

// Compact 8x8 ASCII Font (Characters 32 to 95: Space to '_')
const uint8_t font8x8[64][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x18,0x3c,0x3c,0x18,0x18,0x00,0x18,0x00}, {0x6c,0x6c,0x6c,0x00,0x00,0x00,0x00,0x00}, {0x6c,0x6c,0xfe,0x6c,0xfe,0x6c,0x6c,0x00},
    {0x18,0x3e,0x60,0x3c,0x06,0x7c,0x18,0x00}, {0x00,0xc6,0xcc,0x18,0x30,0x66,0xc6,0x00}, {0x38,0x6c,0x6c,0x38,0x6d,0x66,0x3b,0x00}, {0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00},
    {0x0c,0x18,0x30,0x30,0x30,0x18,0x0c,0x00}, {0x30,0x18,0x0c,0x0c,0x0c,0x18,0x30,0x00}, {0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00}, {0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, {0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, {0x06,0x0c,0x18,0x30,0x60,0xc0,0x80,0x00},
    {0x3c,0x66,0x6e,0x76,0x66,0x66,0x3c,0x00}, {0x18,0x38,0x58,0x18,0x18,0x18,0x7e,0x00}, {0x3c,0x66,0x06,0x0c,0x30,0x60,0x7e,0x00}, {0x3c,0x66,0x06,0x1c,0x06,0x66,0x3c,0x00},
    {0x0c,0x1c,0x3c,0x6c,0x7e,0x0c,0x0c,0x00}, {0x7e,0x60,0x7c,0x06,0x06,0x66,0x3c,0x00}, {0x3c,0x66,0x60,0x7c,0x66,0x66,0x3c,0x00}, {0x7e,0x06,0x0c,0x18,0x30,0x30,0x30,0x00},
    {0x3c,0x66,0x66,0x3c,0x66,0x66,0x3c,0x00}, {0x3c,0x66,0x66,0x3e,0x06,0x66,0x3c,0x00}, {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
    {0x06,0x0c,0x18,0x30,0x18,0x0c,0x06,0x00}, {0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00}, {0x60,0x30,0x18,0x0c,0x18,0x30,0x60,0x00}, {0x3c,0x66,0x06,0x0c,0x18,0x00,0x18,0x00},
    {0x3c,0x66,0x6e,0x6e,0x60,0x66,0x3c,0x00}, {0x18,0x3c,0x66,0x66,0x7e,0x66,0x66,0x00}, {0x7c,0x66,0x66,0x7c,0x66,0x66,0x7c,0x00}, {0x3c,0x66,0x60,0x60,0x60,0x66,0x3c,0x00},
    {0x78,0x6c,0x66,0x66,0x66,0x6c,0x78,0x00}, {0x7e,0x60,0x60,0x7c,0x60,0x60,0x7e,0x00}, {0x7e,0x60,0x60,0x7c,0x60,0x60,0x60,0x00}, {0x3c,0x66,0x60,0x6e,0x66,0x66,0x3e,0x00},
    {0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x00}, {0x3c,0x18,0x18,0x18,0x18,0x18,0x3c,0x00}, {0x06,0x06,0x06,0x06,0x06,0x66,0x3c,0x00}, {0x66,0x6c,0x78,0x70,0x78,0x6c,0x66,0x00},
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7e,0x00}, {0x63,0x77,0x7f,0x6b,0x63,0x63,0x63,0x00}, {0x66,0x76,0x7e,0x7e,0x6e,0x66,0x66,0x00}, {0x3c,0x66,0x66,0x66,0x66,0x66,0x3c,0x00},
    {0x7c,0x66,0x66,0x7c,0x60,0x60,0x60,0x00}, {0x3c,0x66,0x66,0x66,0x6a,0x6c,0x36,0x00}, {0x7c,0x66,0x66,0x7c,0x6c,0x66,0x66,0x00}, {0x3c,0x66,0x60,0x3c,0x06,0x66,0x3c,0x00},
    {0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, {0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0x00}, {0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0x00}, {0x63,0x63,0x63,0x6b,0x7f,0x77,0x63,0x00},
    {0x66,0x66,0x3c,0x18,0x3c,0x66,0x66,0x00}, {0x66,0x66,0x66,0x3c,0x18,0x18,0x18,0x00}, {0x7e,0x06,0x0c,0x18,0x30,0x60,0x7e,0x00}, {0x3c,0x30,0x30,0x30,0x30,0x30,0x3c,0x00},
    {0x60,0x30,0x18,0x0c,0x06,0x03,0x01,0x00}, {0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x00}, {0x00,0x00,0x3c,0x66,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00}
};

// Hardware-accelerated sine wave text scroller with copper bar shading
void render_scroller(SDL_Renderer* renderer, float t, const char* text) {
    float scale = 4.0f;                       // Chunkiness of the font
    float base_y = screen_h - 100.0f;         // Float above bottom edge
    float speed = 200.0f;                     // Horizontal scroll speed
    
    int len = strlen(text);
    float char_width = 8.0f * scale;
    float total_width = len * char_width;

    // Calculate continuous seamless scroll offset
    float offset = screen_w - fmodf(t * speed, total_width);

    // We will batch render all the rectangles for maximum speed.
    // Grouping by row lets us easily apply the "Copper Bar" horizontal color effect.
    SDL_Rect rects[400]; 

    for (int row = 0; row < 8; row++) {
        // Amiga Copper Bar effect: Shift RGB based on row height and time
        float phase = row * 0.4f - t * 4.0f;
        Uint8 cr = (Uint8)((sinf(phase + 0.0f) + 1.0f) * 127.5f);
        Uint8 cg = (Uint8)((sinf(phase + 2.0f) + 1.0f) * 127.5f);
        Uint8 cb = (Uint8)((sinf(phase + 4.0f) + 1.0f) * 127.5f);
        SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);
        
        int rect_count = 0;

        // Draw the text string twice back-to-back to ensure it wraps seamlessly on screen
        for (int i = 0; i < len * 2; i++) {
            char c = text[i % len];
            
            // Map character to our 8x8 font array (fallback to space if out of bounds)
            if (c < 32 || c > 95) c = 32; 
            uint8_t glyph_row = font8x8[c - 32][row];
            
            for (int col = 0; col < 8; col++) {
                // If the pixel is filled in the bitmap font
                if (glyph_row & (1 << (7 - col))) {
                    float px = offset + (i * char_width) + (col * scale);
                    
                    // Frustum Culling: Only compute and draw if pixel is actually on screen
                    if (px > -scale && px < screen_w) {
                        // SINE WAVE BENDING: Calculate Y offset per *pixel column*
                        float py = base_y + sinf(px * 0.005f + t * 4.0f) * 45.0f + (row * scale);
                        
                        rects[rect_count].x = (int)px;
                        rects[rect_count].y = (int)py;
                        rects[rect_count].w = (int)scale;
                        rects[rect_count].h = (int)scale;
                        rect_count++;

                        // Flush batch if full (avoids array overflow)
                        if (rect_count == 400) {
                            SDL_RenderFillRects(renderer, rects, rect_count);
                            rect_count = 0;
                        }
                    }
                }
            }
        }
        
        // Render remaining rectangles for this row
        if (rect_count > 0) {
            SDL_RenderFillRects(renderer, rects, rect_count);
        }
    }
}

int main(int argc, char* argv[]) {
    // Parse Command Line Arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            screen_w = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            screen_h = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fullscreen") == 0) {
            fullscreen = true;
        }
    }

    // Initialize SDL2 (Video and Audio)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! Error: %s\n", SDL_GetError());
        return 1;
    }

    // Open the Audio Device
    // Arguments: Frequency (22500Hz), Format, Channels (2 for Stereo), Chunk Size
    if (Mix_OpenAudio(22500, MIX_DEFAULT_FORMAT, 2, 2024) < 0) {
        printf("SDL_mixer could not initialize! Mix_Error: %s\n", Mix_GetError());
        SDL_Quit();
        return 1;
    }

    // Load the MP3 File
    Mix_Music* bgm = Mix_LoadMUS("drone.mp3");
    if (bgm == NULL) {
        printf("Failed to load MP3 file! Mix_Error: %s\n", Mix_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    } else if (bgm != NULL) {
            Mix_FadeInMusic(bgm, -1, 2000);
        } else {
            printf("Warning: Could not load bgm.mp3! SDL_mixer Error: %s\n", Mix_GetError());
    }

    // Setup Window Flags
    Uint32 window_flags = SDL_WINDOW_SHOWN;
    if (fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_Window* window = SDL_CreateWindow("Linux Demoscene - Parallax Hypno Tunnel w/Starfield and Isometric Cube Floor", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          screen_w, screen_h, window_flags);
    
    // If fullscreen, update logic to the actual desktop resolution dynamically
    if (fullscreen) {
        SDL_GetWindowSize(window, &screen_w, &screen_h);
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    init_stars();

    bool running = true;
    SDL_Event event;

    int horizon_y = screen_h / 3;
    int max_depth = (screen_h - horizon_y + 100) / (TILE_H / 2); 
    int cols_w = (screen_w / TILE_W) + 4; 

    float last_time = SDL_GetTicks() / 1000.0f;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            // Press ESC to quit
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false; 
            }
        }

        float current_time = SDL_GetTicks() / 1000.0f; 
        float dt = current_time - last_time;
        last_time = current_time;
        float t = current_time;
        
        float center_x = screen_w / 2.0f;
        float center_y = horizon_y - 20.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // 1. Draw Background Tunnel
        render_greyscale_tunnel(renderer, t, center_x, center_y);

        // 2. Draw 3D Starfield over the tunnel
        render_starfield(renderer, dt, center_x, center_y);

        // 3. Draw Foreground Isometric Floor 
        for (int d = 0; d < max_depth; d++) {
            for (int c = -cols_w / 2; c <= cols_w / 2; c++) {
                
                float sx = c * TILE_W + (screen_w / 2.0f);
                if (d % 2 != 0) sx += TILE_W / 2.0f; 

                float x = (d + c * 2.0f) / 2.0f;
                float y = (d - c * 2.0f) / 2.0f;

                float dist = sqrtf((c * 2.0f) * (c * 2.0f) + (d - max_depth / 2.0f) * (d - max_depth / 2.0f));

                float ripple = sinf(dist * 0.15f - t * 4.0f) * 40.0f;
                float diag = sinf((x + y) * 0.1f + t * 2.0f) * 20.0f;
                float rnd = (hash2d((int)x, (int)y) - 0.5f) * sinf(t * 3.0f + hash2d((int)x, (int)y) * 6.28f) * 10.0f;

                float h = ripple + diag + rnd;
                float sy = horizon_y + d * (TILE_H / 2.0f) - h;

                float grad_t = (sinf((x + y) * 0.15f - t * 1.5f) + 1.0f) * 0.5f;
                SDL_Color current_color = get_gradient_color(grad_t);

                if (sy > -CUBE_Z && sy < screen_h) {
                    draw_iso_cube(renderer, sx, sy, current_color);
                }
            }
        }
        // 4. Draw Amiga Sine Wave Scroller
        const char* msg = " *** AMIGA DEMOSCENE RULES *** THE PIXELS ARE BENDING *** HARDWARE ACCELERATED IN C AND SDL2 *** DRONING IS LOOPING *** THE TUNNEL IS INFINITE *** OUR FATHER *** WHO ART IN SBIN *** INIT IS THY NAME *** THY PID IS 1 *** THY CHILDREN RUN IN USER SPACE *** GIVE US THIS DAY OUR DAILY RAM *** AND FORGIVE US OUR BAD CODE *** AS WE FORGIVE THOSE WHO FORK OUR CODE *** LEAD US NOT INTO SEGMENTATION FAULT *** BUT DELIVER US FROM SIGKILL *** SUDO *** ";
        render_scroller(renderer, t, msg);

        SDL_RenderPresent(renderer);
    }

    // --- NEW FIX: Halt and explicitly free the music BEFORE closing the audio device ---
    if (bgm != NULL) {
        Mix_HaltMusic();
        Mix_FreeMusic(bgm);
        bgm = NULL;
    }

    // Now it is safe to close the audio threads without deadlocking
    Mix_CloseAudio();
    Mix_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}