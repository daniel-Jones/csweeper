/*
 * simple ncurses minesweeper (Daniel Jones daniel@danieljon.es)
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

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ncurses.h>

#define WIDTH 20
#define HEIGHT 20
#define MINECOUNT 70
#define TILEGAP 2
#define DOWN 0
#define UP 1
#define LEFT 2
#define RIGHT 3

enum STATE
{
	HIDDEN 	= 1 << 0,
	MINE	= 1 << 1,
	FLAGGED	= 1 << 2,
};

struct tile
{
	enum STATE state;
	int x, y;
	int neighbormines;
} board[WIDTH][HEIGHT]={0};

struct cursor
{
	int x;
	int y;
} cursor = {0};

WINDOW *window;
int exitgame = 0;

void draw();
int canmove(int dir);
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

int
canmove(int dir)
{
	/* check if cursor inside game region */
	if (dir == LEFT && cursor.x <= 0) return 0;
	else if (dir == RIGHT && cursor.x >= WIDTH-1) return 0;
	else if (dir == UP && cursor.y <= 0) return 0;
	else if (dir == DOWN && cursor.y >= HEIGHT-1) return 0;
	return 1;
}

void
draw()
{
	wclear(window);
	clear();
	box(window, 0, 0);
	if (!exitgame)
	{
		mvprintw(HEIGHT+3, 0, "The aim of the game is to reveal all non-mine tiles or flag every mine tile");
		mvprintw(HEIGHT+5, 0, "hjkl/wasd to move cursor\nspace to reveal tile\nf to flag tile");
	}
	for (int x = 0; x < WIDTH; x++)
	{
		for (int y = 0; y < HEIGHT; y++)
		{
			struct tile *tile = gettileat(x, y);
			char neighbormines = (char)tile->neighbormines+'0';
			if (neighbormines == '0')
				neighbormines = ' ';
			if (tile->state & FLAGGED)
				mvwprintw(window, y+1, (x*TILEGAP)+1, "F");
			else if (tile->state & HIDDEN)
				mvwprintw(window, y+1, (x*TILEGAP)+1, ".");

			else
				mvwprintw(window, y+1, (x*TILEGAP)+1, "%c", (tile->state & MINE) ? 'M' : neighbormines);
		}
	}
	wmove(window, cursor.y+1, (cursor.x*TILEGAP)+1);
	refresh();
	wrefresh(window);
}

int
main(void)
{
	initscr();
	noecho();
	window = newwin(HEIGHT+TILEGAP, (WIDTH*TILEGAP)+1, 1, 8);
	generateboard();
	int ch;
	while(!exitgame)
	{
		draw();
		ch = getch(); /* blocking */
		switch(ch)
		{
			case 'k':
			case 'w':
				if (canmove(UP))
					cursor.y--;
				break;

			case 'j':
			case 's':
				if (canmove(DOWN))
					cursor.y++;
				break;

			case 'h':
			case 'a':
				if (canmove(LEFT))
					cursor.x--;
				break;
			case 'l':
			case 'd':
				if (canmove(RIGHT))
					cursor.x++;
				break;
			case 'f':
				{
					struct tile *tile = gettileat(cursor.x, cursor.y);
					if (tile && tile->state & HIDDEN)
					{
						tile->state ^= FLAGGED;
					}
					 break;
				}
			case ' ':
				exitgame = reveal(cursor.x, cursor.y);
				 break;

			case 'q':
				exitgame = 1;
				break;
		}
		if (checkwin())
		{
			revealmines();
			draw();
			mvprintw(HEIGHT+3, 0, "you won");
			break;
		}
		else if (exitgame)
		{
			revealmines();
			draw();
			mvprintw(HEIGHT+3, 0, "you lost");
			break;
		}
	}
	mvprintw(HEIGHT+4, 0, "press any key to exit..");
	getch();
	endwin();
	return 0;
}
