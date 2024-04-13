#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "seed.c"

#define VA_ARGS(...) , ##__VA_ARGS__
#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define array_count(static_array) (sizeof(static_array) / sizeof((static_array)[0]))
#define rect_fields \
  float w;          \
  float h
#define point_fields \
  float x;           \
  float y
#define velocity_fields \
  float velocity_x;     \
  float velocity_y
#define direction_fields \
  float direction_x;     \
  float direction_y
#define prev_point_fields \
  float prev_x;           \
  float prev_y
#define print(format, ...)            \
  printf(format "\n", ##__VA_ARGS__); \
  fflush(stdout)

typedef struct Spring
{
  float target;
  float current;
  float velocity;
  float acceleration;
  float friction;
} Spring;

typedef struct
{
  SDL_Texture *texture;
  rect_fields;
  point_fields;
  prev_point_fields;
  velocity_fields;
  direction_fields;
  float speed;
  int layer;
  bool selected;
  char *name;
} Entity;

typedef struct
{
  SDL_Texture *texture;
  rect_fields;
  point_fields;
  prev_point_fields;
  velocity_fields;
  float zoom;
  Spring zoom_spring;
  Spring pan_spring_x;
  Spring pan_spring_y;
} Camera;

typedef struct
{
  int current_time;
  int last_update_time;
  float speed;
  float prev_speed;
  float delta_time;
  float animated_time;
  SDL_Renderer *renderer;
  SDL_Surface *screen_surface;
  SDL_Surface *text_surface;
  SDL_Window *window;
  SDL_Color background_color;
  Camera camera;
  TTF_Font **fonts;
  float fps;
  float render_zoom;
  float render_camera_x;
  float render_camera_y;
} RenderContext;

typedef struct
{
  int prev_state;
  int state;
  int button;
  point_fields;
  prev_point_fields;
  int clicks;
} MouseState;

typedef struct
{
  int w;
  int h;
} Rect;

void render_entity(RenderContext *render_context, Entity *entity);

SDL_FRect render_rect(RenderContext *render_context, SDL_FRect *source_rect)
{
  int window_w;
  int window_h;
  SDL_GetWindowSizeInPixels(render_context->window, &window_w, &window_h);

  SDL_FRect rect = {
      .w = source_rect->w * render_context->render_zoom,
      .h = source_rect->h * render_context->render_zoom,
      .x = ((source_rect->x - render_context->render_camera_x) * render_context->render_zoom + window_w / 2),
      .y = ((source_rect->y - render_context->render_camera_y) * render_context->render_zoom + window_h / 2),
  };

  return rect;
};

Entity Entity__create(char *image_path, SDL_Renderer *renderer, char *name)
{
  SDL_Surface *image = SDL_LoadBMP(image_path);
  assert(image);

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
  assert(texture);

  float width = 100.0f;
  float scale = width / image->w;
  float height = (float)(image->h * scale);

  Entity entity = {
      .direction_x = ((float)(rand() % 200) - 100) / 100,
      .direction_y = ((float)(rand() % 200) - 100) / 100,
      .x = (float)(rand() % 2000) - 1000,
      .y = (float)(rand() % 2000) - 1000,
      .texture = texture,
      .h = height,
      .w = width,
      .selected = false,
      .name = name,
  };

  // Surface no longer needed after the texture is created
  SDL_FreeSurface(image);

  return entity;
}

void draw_texture(RenderContext *render_context, Entity *entity)
{
  SDL_FRect source_rect = {
      .x = entity->x,
      .y = entity->y,
      .w = entity->w,
      .h = entity->h,
  };
  SDL_FRect texture_rect = render_rect(render_context, &source_rect);

  int copy_result = SDL_RenderCopyF(render_context->renderer, entity->texture, NULL, &texture_rect);
  if (copy_result != 0)
  {
    printf("Failed to render copy: %s\n", SDL_GetError());
    return;
  }
}

void draw_entity_name(RenderContext *render_context, Entity *entity)
{
  int window_w;
  int window_h;
  SDL_GetWindowSizeInPixels(render_context->window, &window_w, &window_h);

  TTF_Font *font = render_context->fonts[1];
  SDL_Color White = {255, 255, 255};
  SDL_Surface *text_surface = TTF_RenderText_Blended(font, entity->name, White);
  if (!text_surface)
  {
    fprintf(stderr, "could not create text surface: %s\n", SDL_GetError());
  }

  SDL_Texture *text_texture = SDL_CreateTextureFromSurface(render_context->renderer, text_surface);
  if (!text_texture)
  {
    fprintf(stderr, "could not create text texture: %s\n", SDL_GetError());
  }

  // ((source_rect->x - render_context->render_camera_x) * render_context->render_zoom + window_w / 2)
  float diff = ((entity->w * render_context->render_zoom) - text_surface->w) / 2;
  float x = (((entity->x - render_context->render_camera_x) * render_context->render_zoom) + diff) + window_w / 2;
  // float y = entity->y + text_surface->h - entity->h;

  SDL_FRect text_rect = {
      .w = (float)text_surface->w,
      .h = (float)text_surface->h,
      .x = x,
      .y = (entity->y - render_context->render_camera_y - (45.0f / render_context->render_zoom)) * render_context->render_zoom + window_h / 2,
  };

  SDL_RenderCopyF(render_context->renderer, text_texture, NULL, &text_rect);
  SDL_FreeSurface(text_surface);
  SDL_DestroyTexture(text_texture);
}

void draw_debug_text(RenderContext *render_context, int index, char *str, ...)
{
  char text_buffer[128];
  va_list args;
  va_start(args, str);
  int chars_written = vsnprintf(text_buffer, sizeof(text_buffer), str, args);
  assert(chars_written > 0);
  va_end(args);

  int window_w;
  int window_h;
  SDL_GetWindowSizeInPixels(render_context->window, &window_w, &window_h);

  TTF_Font *font = render_context->fonts[0];
  SDL_Color White = {255, 255, 255};
  SDL_Surface *text_surface = TTF_RenderText_Blended(font, text_buffer, White);
  if (!text_surface)
  {
    fprintf(stderr, "could not create text surface: %s\n", SDL_GetError());
  }

  SDL_FRect text_rect = {
      .w = (float)text_surface->w,
      .h = (float)text_surface->h,
      .x = 10.0f,
      .y = 10.0f + (32.0f * index),
  };

  SDL_Texture *text_texture = SDL_CreateTextureFromSurface(render_context->renderer, text_surface);
  if (!text_texture)
  {
    fprintf(stderr, "could not create text texture: %s\n", SDL_GetError());
  }

  SDL_RenderCopyF(render_context->renderer, text_texture, NULL, &text_rect);
  SDL_FreeSurface(text_surface);
  SDL_DestroyTexture(text_texture);
}

void render_debug_info(RenderContext *render_context, MouseState *mouse_state)
{
  int index = 0;
  draw_debug_text(render_context, index++, "fps: %.2f", render_context->fps);
  draw_debug_text(render_context, index++, "mouse state: %d, button: %d, clicks: %d", mouse_state->state, mouse_state->button, mouse_state->clicks);
  draw_debug_text(render_context, index++, "prev mouse state: %d", mouse_state->prev_state);
  draw_debug_text(render_context, index++, "camera zoom: %.1f", render_context->camera.zoom);
  draw_debug_text(render_context, index++, "game speed: %.1f", render_context->speed);
}

float Spring__update(Spring *spring, float target)
{
  spring->target = target;
  spring->velocity += (target - spring->current) * spring->acceleration;
  spring->velocity *= spring->friction;
  return spring->current += spring->velocity;
}
void render(RenderContext *render_context, Entity *entities, int entities_count)
{
  int window_w;
  int window_h;
  SDL_GetWindowSizeInPixels(render_context->window, &window_w, &window_h);

  for (int entity_i = 0; entity_i < entities_count; entity_i++)
  {
    if (entities[entity_i].texture)
    {
      render_entity(render_context, &entities[entity_i]);
    }
  }

  SDL_RenderPresent(render_context->renderer);
}

void render_entity(RenderContext *render_context, Entity *entity)
{
  entity->x += entity->direction_x * (render_context->delta_time * render_context->speed);
  entity->y += entity->direction_y * (render_context->delta_time * render_context->speed);

  draw_texture(render_context, entity);

  if (entity->selected)
  {
    SDL_FRect source_rect = {
        .w = entity->w + 20.0f,
        .h = entity->h + 20.0f,
        .x = entity->x - 10.0f,
        .y = entity->y - 10.0f,
    };
    SDL_FRect rect = render_rect(render_context, &source_rect);

    SDL_SetRenderDrawColor(render_context->renderer,
                           255, 255, 255, 255);
    SDL_RenderDrawRectF(render_context->renderer, &rect);

    // SDL_RenderSetClipRect(render_context.renderer, NULL);
    // int result = SDL_RenderFillRect(render_context.renderer, NULL);
    // assert(result == 0);
  }

  draw_entity_name(render_context, entity);
}

TTF_Font *load_font(const char *font_file_path, int font_size)
{
  TTF_Font *font = TTF_OpenFont(font_file_path, font_size);
  assert(font);

  return font;
}

int main(int argc, char *args[])
{
  srand(create_seed("ATHANO_LOVES_CHAT_OWO"));

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
    return 1;
  }

  if (TTF_Init() == -1)
  {
    fprintf(stderr, "could not initialize ttf: %s\n", TTF_GetError());
    return 1;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

  SDL_Window *window = SDL_CreateWindow(
      "Cultivation Sim",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      SCREEN_WIDTH, SCREEN_HEIGHT,
      SDL_WINDOW_SHOWN);
  if (!window)
  {
    fprintf(stderr, "could not create window: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
  {
    fprintf(stderr, "could not create renderer: %s\n", SDL_GetError());
    return 1;
  }

  Entity entities[] = {
      Entity__create("assets/lamb2.bmp", renderer, "pushqrdx"),
      Entity__create("assets/stone.bmp", renderer, "Athano"),
      Entity__create("assets/lamb.bmp", renderer, "AshenHobs"),
      Entity__create("assets/stone.bmp", renderer, "adrian_learns"),
      Entity__create("assets/lamb.bmp", renderer, "RVerite"),
      Entity__create("assets/stone.bmp", renderer, "Orshy"),
      Entity__create("assets/lamb2.bmp", renderer, "ruggs888"),
      Entity__create("assets/fish.bmp", renderer, "Xent12"),
      Entity__create("assets/fish.bmp", renderer, "nuke_bird"),
      Entity__create("assets/stone.bmp", renderer, "Kasper_573"),
      Entity__create("assets/fish.bmp", renderer, "SturdyPose"),
      Entity__create("assets/stone.bmp", renderer, "coffee_lava"),
      Entity__create("assets/stone.bmp", renderer, "goudacheeseburgers"),
      Entity__create("assets/stone.bmp", renderer, "ikiwixz"),
      Entity__create("assets/lamb2.bmp", renderer, "NixAurvandil"),
      Entity__create("assets/lamb2.bmp", renderer, "smilingbig"),
      Entity__create("assets/lamb.bmp", renderer, "tk_dev"),
      Entity__create("assets/lamb2.bmp", renderer, "realSuperku"),
      Entity__create("assets/stone.bmp", renderer, "Hoby2000"),
      Entity__create("assets/stone.bmp", renderer, "CuteMath"),
      Entity__create("assets/stone.bmp", renderer, "forodor"),
      Entity__create("assets/stone.bmp", renderer, "Azenris"),
      Entity__create("assets/stone.bmp", renderer, "collector_of_stuff"),
      Entity__create("assets/stone.bmp", renderer, "EvanMMO"),
      Entity__create("assets/stone.bmp", renderer, "thechaosbean"),
      Entity__create("assets/stone.bmp", renderer, "Lutf1sk"),
      Entity__create("assets/stone.bmp", renderer, "BauBas9883"),
      Entity__create("assets/stone.bmp", renderer, "physbuzz"),
      Entity__create("assets/stone.bmp", renderer, "rizoma0x00"),
      Entity__create("assets/stone.bmp", renderer, "Tkap1"),
      Entity__create("assets/stone.bmp", renderer, "GavinsAwfulStream"),
      Entity__create("assets/stone.bmp", renderer, "Resist_0"),
      Entity__create("assets/stone.bmp", renderer, "b1k4sh"),
  };

  TTF_Font *fonts[] = {
      load_font("assets/OpenSans-Regular.ttf", 32),
      load_font("assets/OpenSans-Regular.ttf", 24),
      load_font("assets/OpenSans-Regular.ttf", 16),
  };

  RenderContext render_context = {
      .renderer = renderer,
      .current_time = 0,
      .last_update_time = 0,
      .animated_time = 0,
      .speed = 200.0f,
      .delta_time = 0,
      .window = window,
      .background_color = {45, 125, 2, 255},
      .camera = {
          .x = 0,
          .y = 0,
          .zoom = 1.0f,
          .pan_spring_x = {
              .target = 1.0f,
              .current = 1.0f,
              .velocity = 0.0f,
              .acceleration = 0.5f,
              .friction = 0.1f,
          },
          .pan_spring_y = {
              .target = 1.0f,
              .current = 1.0f,
              .velocity = 0.0f,
              .acceleration = 0.5f,
              .friction = 0.1f,
          },
          .zoom_spring = {
              .target = 1.0f,
              .current = 1.0f,
              .velocity = 0.0f,
              .acceleration = 0.4f,
              .friction = 0.1f,
          }},
      .fonts = fonts,
  };

  int game_is_still_running = 1;
  unsigned int start_ticks = SDL_GetTicks();
  int frame_count = 0;

  MouseState mouse_state = {
      .prev_state = 0,
      .state = 0,
      .button = 0,
      .clicks = 0,
      .prev_x = 0,
      .prev_y = 0,
      .x = 0,
      .y = 0,
  };

  while (game_is_still_running)
  {
    frame_count++;
    if (SDL_GetTicks() - start_ticks >= 1000)
    {
      render_context.fps = (float)frame_count;
      frame_count = 0;
      start_ticks = SDL_GetTicks();
    }

    render_context.current_time = SDL_GetTicks();
    render_context.delta_time = (float)(render_context.current_time - render_context.last_update_time) / 1000;
    render_context.last_update_time = render_context.current_time;
    render_context.animated_time = fmodf(render_context.animated_time + render_context.delta_time * 0.5f, 1);
    render_context.render_zoom = Spring__update(&render_context.camera.zoom_spring, render_context.camera.zoom);

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
      {
        mouse_state.prev_state = mouse_state.state;
        mouse_state.state = event.button.state;
        mouse_state.button = event.button.button;
        mouse_state.clicks = event.button.clicks;
      }
      if (event.type == SDL_MOUSEMOTION)
      {
        mouse_state.prev_state = mouse_state.state;
        mouse_state.prev_x = mouse_state.x;
        mouse_state.prev_y = mouse_state.y;
        mouse_state.x = (float)event.motion.x;
        mouse_state.y = (float)event.motion.y;
      }
      if (event.type == SDL_MOUSEWHEEL)
      {
        if (event.wheel.y > 0)
        {
          // zoom in
          render_context.camera.zoom = SDL_min(render_context.camera.zoom + 0.1f, 2.0f);
        }
        else if (event.wheel.y < 0)
        {
          // zoom out
          render_context.camera.zoom = SDL_max(render_context.camera.zoom - 0.1f, 0.1f);
        }
      }
      if (event.type == SDL_KEYDOWN)
      {
        switch (event.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          game_is_still_running = 0;
          break;

        case SDLK_UP:
          render_context.speed += 100.0f;
          break;

        case SDLK_DOWN:
          render_context.speed = SDL_max(render_context.speed - 100.0f, 0);
          break;

        case SDLK_SPACE:
          if (render_context.prev_speed > 0)
          {
            render_context.speed = render_context.prev_speed;
            render_context.prev_speed = 0;
          }
          else
          {
            render_context.prev_speed = render_context.speed;
            render_context.speed = 0;
          }

        default:
          break;
        }
      }
      else if (event.type == SDL_QUIT)
      {
        game_is_still_running = 0;
      }

      if (mouse_state.state == SDL_PRESSED && mouse_state.prev_state == SDL_RELEASED)
      {
        for (int entity_i = 0; entity_i < array_count(entities); entity_i++)
        {
          SDL_FRect source_rect = {
              .w = entities[entity_i].w,
              .h = entities[entity_i].h,
              .x = entities[entity_i].x,
              .y = entities[entity_i].y,
          };
          SDL_FRect rect = render_rect(&render_context, &source_rect);
          SDL_FPoint point = {
              .x = mouse_state.x,
              .y = mouse_state.y,
          };

          if (SDL_PointInFRect(&point, &rect))
          {
            entities[entity_i].selected = !entities[entity_i].selected;
            break;
          }
        }
      }
    }

    if (mouse_state.state == SDL_PRESSED)
    {
      if (mouse_state.button == SDL_BUTTON_RIGHT)
      {
        if (mouse_state.prev_x != mouse_state.x || mouse_state.prev_y != mouse_state.y)
        {
          float delta_x = mouse_state.x - mouse_state.prev_x;
          float delta_y = mouse_state.y - mouse_state.prev_y;
          mouse_state.prev_x = mouse_state.x;
          mouse_state.prev_y = mouse_state.y;

          render_context.camera.x -= delta_x / render_context.render_zoom;
          render_context.camera.y -= delta_y / render_context.render_zoom;
        }
      }
    }

    {
      float camera_keyboard_movement_speed = 5.0f;
      const Uint8 *keyboard = SDL_GetKeyboardState(NULL);
      if (keyboard[SDL_GetScancodeFromKey(SDLK_w)])
      {
        render_context.camera.y -= camera_keyboard_movement_speed / render_context.render_zoom;
      }
      if (keyboard[SDL_GetScancodeFromKey(SDLK_s)])
      {
        render_context.camera.y += camera_keyboard_movement_speed / render_context.render_zoom;
      }
      if (keyboard[SDL_GetScancodeFromKey(SDLK_a)])
      {
        render_context.camera.x -= camera_keyboard_movement_speed / render_context.render_zoom;
      }
      if (keyboard[SDL_GetScancodeFromKey(SDLK_d)])
      {
        render_context.camera.x += camera_keyboard_movement_speed / render_context.render_zoom;
      }
    }

    SDL_SetRenderDrawColor(render_context.renderer,
                           render_context.background_color.r, render_context.background_color.g, render_context.background_color.b, 255);
    int result = SDL_RenderFillRect(render_context.renderer, NULL);
    assert(result == 0);

    { // Set the camera to follow an entity, if only one entity is selected
      int selected_count = 0;
      int selected_entity_index = 0;
      for (int entity_i = 0; entity_i < array_count(entities); entity_i++)
      {
        if (entities[entity_i].selected)
        {
          selected_count++;
          selected_entity_index = entity_i;
          if (selected_count > 1)
          {
            break;
          }
        }
      }
      if (selected_count == 1)
      {
        render_context.camera.x = entities[selected_entity_index].x;
        render_context.camera.y = entities[selected_entity_index].y;
      }
    }

    { // Spring the camera position
      render_context.render_camera_x = Spring__update(&render_context.camera.pan_spring_x, render_context.camera.x);
      render_context.render_camera_y = Spring__update(&render_context.camera.pan_spring_y, render_context.camera.y);
    }

    render_debug_info(&render_context, &mouse_state);

    render(&render_context, entities, array_count(entities));
  }

  SDL_DestroyWindow(window);
  return 0;
}