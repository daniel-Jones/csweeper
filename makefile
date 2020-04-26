all: csweeper ncsweeper

csweeper: csweeper.c
	    cc -g -Wall -Wextra -std=c99 -o csweeper csweeper.c
ncsweeper: ncsweeper.c
	    cc -g -Wall -Wextra -lncurses -o ncsweeper ncsweeper.c
clean:
	@rm -f csweeper ncsweeper
	@rm -f *.o
