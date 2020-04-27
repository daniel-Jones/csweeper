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
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ncurses.h>
#include <string.h>

#define WIDTH 15
#define HEIGHT 15
#define MINECOUNT 31
#define TILEGAP 2
#define DOWN 0
#define UP 1
#define LEFT 2
#define RIGHT 3

enum DEMO_ACTION_TYPE
{
	NONE = 0,
	GOUP,
	GODOWN,
	GOLEFT,
	GORIGHT,
	FLAG,
	REVEAL,
	QUIT,
};

struct demo_header
{
	int width;
	int height;
	int mine_count;
};

struct demo_mine
{
	int x;
	int y;
};

struct demo_action
{
	double action_pre_delay;
	enum DEMO_ACTION_TYPE type;
	int start_x;
	int start_y;
};

struct action_node
{
	struct demo_action *action;
	struct action_node *next;
} *action_head = NULL;

enum STATE
{
	HIDDEN 	= 1 << 0,
	MINE	= 1 << 1,
	FLAGGED	= 1 << 2,
};

struct game
{
	int width;
	int height;
	int minecount;
	int action_count;
	int is_demo;
	int is_recording;
	char demo_filename[512];
} game;

struct tile
{
	enum STATE state;
	int x, y;
	int neighbormines;
} *board = NULL;

struct cursor
{
	int x;
	int y;
} cursor = {0};

WINDOW *window;
int exitgame = 0;

void draw();
int canmove(int dir);
int generateboard();
void drawboard();
int reveal(int x, int y);
void revealmines();
struct tile *getneighbors(struct tile *tile, struct tile **neighbors);
struct tile *gettileat(int x, int y);
int checkwin();
enum DEMO_ACTION_TYPE input();
void free_action_list();
struct action_node *generate_action_node(double delay, enum DEMO_ACTION_TYPE type, int x, int y);
int append_action_node(struct action_node *node);
void save_demo();
int load_demo();
struct action_node *play_demo_action(struct action_node *current_action);

struct tile *getneighbors(struct tile *tile, struct tile **neighbors)
{
	int badup = 0, baddown = 0, badleft = 0, badright = 0;
	if (tile->x-1<0) badleft = 1;
	if (tile->x+1>game.width-1) badright = 1;

	if (tile->y-1<0) badup = 1;
	if (tile->y+1>game.height-1) baddown = 1;

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

	int allowedmines = game.minecount;
	int safetiles = (game.height * game.width) - game.minecount;
	int correctflags = 0;
	int correcttiles = 0;

	for (int y = 0; y < game.height; y++)
	{
		for (int x = 0; x < game.width; x++)
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

int
generateboard()
{
	srand(time(NULL));
	if (!game.is_demo)
	{
		board = malloc(sizeof (struct tile) * (game.width * game.height));
		/* place mines */
		int mx, my;
		for (int x = 0; x < game.minecount; x++)
		{
place_mine:
			mx = rand() % game.width;
			my = rand() % game.height;
			/*ensure our tile is not already a mine */
			if (gettileat(mx, my)->state & MINE)
				goto place_mine;
			struct tile *tile = gettileat(mx, my);
			tile->state |= MINE;
		}
	}
	else
	{
		if(!load_demo())
		{
			puts("cannot generate board");
			return 0;
		}
	}

	/* create window here because if we're playing a demo we need the width/height */
	window = newwin(game.height+TILEGAP, (game.width*TILEGAP)+1, 1, 8);
	/* figure out neighbors */
	for (int y = 0; y < game.height; y++)
	{
		for (int x = 0; x < game.width; x++)
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
		}
	}
	return 1;
}

struct tile *
gettileat(int x, int y)
{
	if (x < 0 || x > game.width-1 || y < 0 || y > game.height-1 || board == NULL)
		return NULL;
	/* the board is single dimensional, so we map it as 2d */
	return &board[game.width*x+y];
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
{	for (int y = 0; y < game.height; y++)
	{
		for (int x = 0; x < game.width; x++)
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
	else if (dir == RIGHT && cursor.x >= game.width-1) return 0;
	else if (dir == UP && cursor.y <= 0) return 0;
	else if (dir == DOWN && cursor.y >= game.height-1) return 0;
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
		mvprintw(game.height+3, 0, "The aim of the game is to reveal all non-mine tiles or flag every mine tile");
		mvprintw(game.height+5, 0, "hjkl/wasd to move cursor\nspace to reveal tile\nf to flag tile");
	}
	for (int x = 0; x < game.width; x++)
	{
		for (int y = 0; y < game.height; y++)
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

enum DEMO_ACTION_TYPE
input()
{
	int ch = getch(); /* blocking */
	enum DEMO_ACTION_TYPE type = NONE;
	struct tile *tile = gettileat(cursor.x, cursor.y);
	switch(ch)
	{
		case 'k':
		case 'w':
			if (canmove(UP))
			{
				type = GOUP;
				cursor.y--;
			}
			break;

		case 'j':
		case 's':
			if (canmove(DOWN))
			{
				type = GODOWN;
				cursor.y++;
			}
			break;

		case 'h':
		case 'a':
			if (canmove(LEFT))
			{
				type= GOLEFT;
				cursor.x--;
			}
			break;
		case 'l':
		case 'd':
			if (canmove(RIGHT))
			{
				type = GORIGHT;
				cursor.x++;
			}
			break;
		case 'f':
			{
				if (tile && tile->state & HIDDEN)
				{
					type = FLAG;
					tile->state ^= FLAGGED;
					draw();
				}
				 break;
			}
		case ' ':
			if (tile && !(tile->state & FLAGGED))
			{
				type = REVEAL;
				exitgame = reveal(cursor.x, cursor.y);
			}
			 break;

		case 'q':
			 type = QUIT;
			exitgame = 1;
			break;
		default:
			break;
	}
	return type;
}

void
free_action_list()
{
	struct action_node *node;
	while (action_head)
	{
		node = action_head;
		action_head = action_head->next;
		free(node->action);
		free(node);
	}
}

struct action_node *generate_action_node(double delay, enum DEMO_ACTION_TYPE type, int x, int y)
{
	struct action_node *node = malloc(sizeof(struct action_node));
	struct demo_action  *action = malloc(sizeof(struct demo_action));
	if (!node || !action)
		return NULL;
	action->action_pre_delay = delay;
	action->start_x = x;
	action->start_y = y;
	action->type = type;
	node->action = action;
	node->next = NULL;
	return node;
}

int append_action_node(struct action_node *node)
{
	struct action_node **finger = &action_head;
	while (*finger)
	{
		finger = &(*finger)->next;
	}
	node->next = *finger;
	*finger = node;
	return 1;
}

void print_nodes()
{
	struct action_node *finger = action_head;
	while (finger)
	{
		if (finger->action)
		{
			printf("%d,%d\n", finger->action->start_x, finger->action->start_y);
		}
		finger = finger->next;
	}
}

void
save_demo()
{
	printf("saving demo to %s..\n", game.demo_filename);

	/* open demo file */
	FILE *demo = fopen(game.demo_filename, "wb");
	if (!demo)
	{
		puts("cannot open demo file");
		return;
	}

	/* generate header and write */
	struct demo_header header;
	header.width = game.width;
	header.height = game.height;
	header.mine_count = game.minecount;

	fwrite(&header, sizeof(struct demo_header), 1, demo);

	/* generate mines and write */
	struct demo_mine demo_mines[game.minecount];
	/* loop over every tile and save it if it's a mine */
	int i = 0;
	for (int x = 0; x < game.width; x++)
	{
		for (int y = 0; y < game.height; y++)
		{
			struct tile *tile = gettileat(x, y);
			if (tile->state & MINE)
			{
				demo_mines[i].x = tile->x;
				demo_mines[i].y = tile->y;
				fwrite(&demo_mines[i], sizeof(struct demo_mine), 1, demo);
				i++;
			}
		}
	}

	/* walk action list and write it to file */
	fwrite(&game.action_count, sizeof game.action_count, 1, demo);
	/* skip first node */
	if (!action_head->next)
	{
		puts("cannot write demo file, no actions exist..");
		fclose(demo);
		return;
	}
	struct action_node *finger = action_head->next;
	int x = 0;
	while (finger)
	{
		struct demo_action *action = finger->action;
		fwrite(action, sizeof(struct demo_action), 1, demo);
		finger = finger->next;
		x++;
	}
		printf("saved 0x%x nodes\n", x);
	fclose(demo);
}

int
load_demo()
{
	printf("reading demo %s..\n", game.demo_filename);
	FILE *demo = fopen(game.demo_filename, "rb");
	if (!demo)
	{
		puts("unable to read demo..");
		return 0;
	}

	/* read header and set data */
	struct demo_header header;
	fread(&header, sizeof(struct demo_header), 1, demo);
	game.width = header.width;
	game.height = header.height;
	game.minecount = header.mine_count;
	/* malloc board here because we need the header information from the demo */
	board = malloc(sizeof (struct tile) * (game.width * game.height));
	/* read and set mine data */
	struct demo_mine demo_mine;
	for (int mc = 0; mc < game.minecount; mc++)
	{
		fread(&demo_mine, sizeof(struct demo_mine), 1, demo);
		/* set tile as mine */
		struct tile *tile = gettileat(demo_mine.x, demo_mine.y);
		if (!tile)
		{
			puts("demo corrupt");
			fclose(demo);
			return 0;
		}
		tile->state |= MINE;
	}

	/* read move data and add action to the list */
	int action_count;
	fread(&action_count, sizeof action_count, 1, demo);
	for (int ac = 0; ac < action_count; ac++)
	{
		struct demo_action action;
		fread(&action, sizeof(struct demo_action), 1, demo);
		struct action_node *move = generate_action_node(action.action_pre_delay, action.type, action.start_x, action.start_y);
		append_action_node(move);
	}

	fclose(demo);

	return 1;
}

struct action_node *play_demo_action(struct action_node *current_action)
{

	struct demo_action *action = current_action->action;
	usleep(action->action_pre_delay);
	struct tile *tile = gettileat(action->start_x, action->start_y);
	if (!tile)
		return NULL;
	switch (action->type)
	{
		case GOUP:
			if (canmove(UP))
			{
				cursor.y--;
			}
			break;

		case GODOWN:
			if (canmove(DOWN))
			{
				cursor.y++;
			}
			break;

		case GOLEFT:
			if (canmove(LEFT))
			{
				cursor.x--;
			}
			break;
		case GORIGHT:
			if (canmove(RIGHT))
			{
				cursor.x++;
			}
			break;
		case FLAG:
			{
				if (tile && tile->state & HIDDEN)
				{
					tile->state ^= FLAGGED;
					draw();
				}
				 break;
			}
		case REVEAL:
			if (tile && !(tile->state & FLAGGED))
			{
				exitgame = reveal(action->start_x, action->start_y);
			}
			 break;

		case QUIT:
			exitgame = 1;
			break;
		case NONE:
		default:
			break;

	}
	return current_action->next;
}

int
main(int argc, char **argv)
{
	game.is_demo = 0;
	game.is_recording = 0;
	if ((argc > 1 && argc != 3))
	{
		printf("usage: %s [-record save.dem | -play load.dem]\n", argv[0]);
		goto safe_exit;
	}
	if (argc == 3)
	{
		if (strcmp(argv[1], "-record") == 0)
		{
			game.is_recording = 1;
			strncpy(game.demo_filename, argv[2], 511);
		}
		else if (strcmp(argv[1], "-play") == 0)
		{
			game.is_demo = 1;
			strncpy(game.demo_filename, argv[2], 511);
		}
		else
		{
			printf("usage: %s [-record save.dem | -play load.dem]\n", argv[0]);
			goto safe_exit;
		}
	}
	initscr();
	noecho();
	game.width = WIDTH;
	game.height = HEIGHT;
	game.minecount = MINECOUNT;
	game.action_count = 0;
	action_head = malloc(sizeof(struct action_node));
	action_head->action = NULL;
	action_head->next = NULL;
	if (!action_head)
		goto safe_exit;
	if (!generateboard())
		goto safe_exit;
	struct timespec begin, end;
	double move_us;
	struct action_node *current_action = action_head->next;
	while(!exitgame)
	{
		draw();
		if (game.is_demo)
		{
			if (current_action)
			{
				current_action = play_demo_action(current_action);
			}
			else
				exitgame = 1;
		}
		else
		{
			clock_gettime(CLOCK_MONOTONIC, &begin);
			enum DEMO_ACTION_TYPE type = input();
			clock_gettime(CLOCK_MONOTONIC, &end);
			move_us = (end.tv_sec - begin.tv_sec) * 1000.0;
			move_us += (end.tv_nsec - begin.tv_nsec) / 1000000.0;
			move_us *= 1000;
			struct action_node *move = generate_action_node(move_us, type, cursor.x, cursor.y);
			append_action_node(move);
			game.action_count++;
			//printf("%.3f us elapsed\n", move_us);
		}
		if (checkwin())
		{
			revealmines();
			draw();
			mvprintw(game.height+3, 0, "you won");
			break;
		}
		else if (exitgame)
		{
			revealmines();
			draw();
			mvprintw(game.height+3, 0, "you lost");
			break;
		}
	}
	mvprintw(game.height+4, 0, "press any key to exit..");
	getch();
safe_exit:
	delwin(window);
	endwin();
	if (game.is_recording)
		save_demo();
	free(board);
	free_action_list();
	return 0;
}
