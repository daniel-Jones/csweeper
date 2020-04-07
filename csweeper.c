/*
 * simple grid-based minesweeper (Daniel Jones daniel@danieljon.es)
 *
 * this program is free software: you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation, either version 3 of the license, or
 * (at your option) any later version.
 * 
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 * 
 * you should have received a copy of the gnu general public license
 * along with this program.  if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define WIDTH 10
#define HEIGHT 10
#define MINECOUNT 10

enum STATE
{
	HIDDEN 	= 1 << 0,
	MINE	= 1 << 1,
	FLAGGED	= 1 << 2,
	BLANK 	= 1 << 3,
};

struct tile
{
	enum STATE state;
	int x, y;
	int neighbormines;
};

struct tile board[WIDTH][HEIGHT] = {0};

void generateboard();
void drawboard();
int reveal(int x, int y);
void revealmines();
struct tile *getneighbors(struct tile *tile, struct tile **neighbors);
struct tile *gettileat(int x, int y);
int checkwin();

struct tile *getneighbors(struct tile *tile, struct tile **neighbors)
{
	int badup = 0, baddown = 0, badleft = 0, badright = 0;
	if (tile->x-1<0) badleft = 1;
	if (tile->x+1>WIDTH-1) badright = 1;

	if (tile->y-1<0) badup = 1;
	if (tile->y+1>HEIGHT-1) baddown = 1;

	if (!badleft && !badup) neighbors[0] = gettileat(tile->x-1, tile->y-1);
	if (!badup) neighbors[1] = gettileat(tile->x, tile->y-1);
	if (!badright && !badup) neighbors[2] = gettileat(tile->x+1, tile->y-1);

	if (!badleft) neighbors[3] = gettileat(tile->x-1, tile->y);
	if (!badright) neighbors[4] = gettileat(tile->x+1, tile->y);

	if (!badleft && !baddown) neighbors[5] = gettileat(tile->x-1, tile->y+1);
	if (!baddown) neighbors[6] = gettileat(tile->x, tile->y+1);
	if (!badright && !baddown) neighbors[7] = gettileat(tile->x+1, tile->y+1);

	return *neighbors;
}

int
checkwin()
{

	int allowedmines = MINECOUNT;
	int safetiles = (HEIGHT * WIDTH) - MINECOUNT;
	int correctflags = 0;
	int correcttiles = 0;

	for (int y = 0; y < HEIGHT; y++)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			struct tile *tile = gettileat(x, y);
			if (tile->state & MINE && tile->state & FLAGGED)
				correctflags++;
			else if (!(tile->state & MINE) && !(tile->state & HIDDEN))
				correcttiles++;
		}
	}

	return (correctflags == allowedmines) || (correcttiles == safetiles);
}

void
generateboard()
{
	srand(time(NULL));

	/* place mines */
	//printf("mines at: ");
	for (int x = 0; x < MINECOUNT; x++)
	{
		int mx, my;
		mx = rand() % WIDTH;
		my = rand() % HEIGHT;
		struct tile *tile = gettileat(mx ,my);
		tile->state |= MINE;
		//printf("%d, %d : ", mx, my);
	}
	puts("");

	/* figure out neighbors */
	for (int y = 0; y < HEIGHT; y++)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			struct tile *tile = gettileat(x, y);
			tile->x = x;
			tile->y = y;
			tile->state |= HIDDEN;
			struct tile *neighbors[8] = {NULL};
			getneighbors(tile, neighbors);
			tile->neighbormines = 0;
			for (int i = 0; i < 8; i++)
				if (neighbors[i] != NULL && neighbors[i]->state & MINE)
					tile->neighbormines += 1;
			//if (tile->neighbormines == 0 && !(tile->state & MINE))
				//printf("%d,%d ", tile->x, tile->y);
		}
	}
	puts("");

}

int
main(void)
{
	int dead = 0;
	generateboard();
	puts("reveal every safe tile or flag every mine to win.\nto (un)flag the tile at 3,7 enter 'f 3 7'\n" \
		"to reveal tile at 5,5 enter '5 5'\n");
	drawboard();
	while (dead == 0)
	{
		char input[512];
		int desx = 0, desy = 0, flagging = 0, xset = 0;
		printf("> ");
		fgets(input, 412, stdin);
		char* token = strtok(input, " ");
		while (token)
		{
			if (strcmp(token, "f") == 0)
			{
				flagging = 1;
			}
			else if (xset == 0)
			{
				desx = atoi(token);
				xset = 1;
			}
			else if (xset == 1)
			{
				desy = atoi(token);
			}
			token = strtok(NULL, " ");
		}
		struct tile *tile = gettileat(desx, desy);
		if (flagging)
		{
			if (tile && tile->state & HIDDEN)
				gettileat(desx, desy)->state ^= FLAGGED; /* toggle flagged flag */
		}
		else
		{
			if (tile && !(tile->state & FLAGGED))
				dead = reveal(tile->x, tile->y);
		}
		printf("\033[15A");
		printf("\033[J");
		drawboard();
		if (checkwin())
		{
			revealmines();
			printf("\033[15A");
			printf("\033[J");
			drawboard();
			printf("you win\n");
			break;
		}
		else if (dead == 1)
		{
			revealmines();
			printf("\033[15A");
			printf("\033[J");
			drawboard();
			printf("you lose\n");
		}
	}
	return 1;
}

struct tile *
gettileat(int x, int y)
{
	if (x < 0 || x > WIDTH-1 || y < 0 || y > HEIGHT-1)
		return NULL;
	return &board[x][y];
}

int
reveal(int x, int y)
{
	struct tile *tile = gettileat(x, y);
	tile->state &= ~HIDDEN;
	if (tile->state & MINE)
		return 1;
	if (tile->neighbormines == 0)
	{
		tile->state &= ~HIDDEN;
		struct tile *neighbors[8] = {NULL};
		getneighbors(tile, neighbors);
		for (int nc = 0; nc < 8; nc++)
		{
			struct tile *neighbor = neighbors[nc];
			if (neighbor != NULL && !(neighbor->state & MINE) && neighbor->state & HIDDEN)
			{
				neighbor->state &= ~HIDDEN;
				if (neighbor->neighbormines == 0)
				{
					reveal(neighbor->x, neighbor->y);
				}
			}
		}
	}
	return 0;
}

void
revealmines()
{	for (int y = 0; y < HEIGHT; y++)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			if (gettileat(x, y)->state & MINE)
				reveal(x, y);
		}
	}
}

void
drawboard()
{ 
	printf("   ");
	for (int i = 0; i < WIDTH; i++)
		printf(" %d ", i);
	puts("");
	printf("   ");
	for (int i = 0; i < WIDTH; i++)
		printf("---");
	puts("");

	for (int y = 0; y < HEIGHT; y++)
	{
		printf("%d |", y);
		for (int x = 0; x < WIDTH; x++)
		{
			struct tile *tile = gettileat(x, y);
			char neighbormines = (char)tile->neighbormines+'0';
			if (neighbormines == '0')
				neighbormines = ' ';
			if (tile->state & FLAGGED)
				printf(" F ");
			else if (tile->state & HIDDEN)
				printf(" . ");
			else
				printf(" %c ", (tile->state & MINE) ? 'M' :neighbormines); 
		}
		printf("| %d\n", y);
	}

	printf("   ");
	for (int i = 0; i < WIDTH; i++)
		printf("---");
	puts("");
	printf("   ");
	for (int i = 0; i < WIDTH; i++)
		printf(" %d ", i);
	puts("");

}

