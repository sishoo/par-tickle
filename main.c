
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define CELL_SIZE_PIXELS 100
#define NUM_CELLS (SCREEN_HEIGHT * SCREEN_WIDTH / CELL_SIZE_PIXELS / CELL_SIZE_PIXELS)
#define NUM_ROWS (SCREEN_HEIGHT / CELL_SIZE_PIXELS)
#define ROW_SIZE (SCREEN_WIDTH / CELL_SIZE_PIXELS)

#define RAND ((float)rand() / (float)RAND_MAX)

typedef struct
{
        float avg_mass;
        Vector2 velocity;
} Cell;

void cells_init(Cell *const cells_one, Cell *const cells_two)
{
        for (uint32_t i = 0; i < NUM_CELLS; i++)
        {
                cells_one[i].avg_mass = cells_two[i].avg_mass = RAND;
                cells_one[i].velocity = cells_two[i].velocity = Vector2Normalize((Vector2){RAND, RAND});
        }
}

Color cell_get_color(Cell *const c)
{
        /*
                Color is based off avg_mass.
                0 = black
                max possible mass = white
        */

        char color = *(char *)&c->avg_mass;
        return (Color){color, color, color, 1};
}

void cells_draw(Cell *const cells)
{
        uint32_t row_num = 0;
        for (uint32_t i = 0; i < NUM_CELLS; i++)
        {
                row_num += !(bool)(i % ROW_SIZE) && i;

                DrawRectangle(
                    CELL_SIZE_PIXELS * (i % ROW_SIZE),
                    CELL_SIZE_PIXELS * row_num,
                    CELL_SIZE_PIXELS,
                    CELL_SIZE_PIXELS,
                    cell_get_color(cells + i));
        }
}

void cells_update(Cell *const dest, Cell *const src)
{
        for (uint32_t i = 0; i < NUM_CELLS; i++)
        {
                Cell cell = src[i];
                
                if (cell.velocity.x < 0)
                {
                        dest[]
                }
                
        }       
}

int main()
{
        // clang main.c -o main -Ofast -lraylib -Wall

        srand(time(NULL));

        Cell *cells_one = malloc(NUM_CELLS * sizeof(Cell));
        Cell *cells_two = malloc(NUM_CELLS * sizeof(Cell));
        cells_init(cells_one, cells_two);

        InitWindow(800, 600, "particles");

        bool cells_toggle = 1;
        while (!WindowShouldClose())
        {
                BeginDrawing();

                // ClearBackground(BLACK);

                if (cells_toggle)
                {
                        cells_draw(cells_one);
                        cells_update(cells_two, cells_one);
                }
                else
                {
                        cells_draw(cells_two);
                        cells_update(cells_one, cells_two);
                }

                DrawFPS(10, 10);

                EndDrawing();

                cells_toggle = !cells_toggle;
        }

        free(cells_one);
        free(cells_two);
}