#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <curses.h>
#include <termios.h>
#include <fcntl.h>


#define ROW 10
#define COLUMN 50 


struct Node{
	int x , y; 
	Node( int _x , int _y ) : x( _x ) , y( _y ) {}; 
	Node(){} ; 
} frog ; 

pthread_mutex_t mutex;
pthread_cond_t logSignal[ROW];

int finished_state = -1;
const char finished_msg[3][32] = {"user win", "user lose", "user quit"};

char map[ROW+1][COLUMN];
int logStart[ROW], logEnd[ROW], logSpeedR[ROW];
const int logSpeed = 200000;
const int dir[] = {1, -1}; // left or right

// Determine a keyboard is hit or not. If yes, return 1. If not, return 0. 
int kbhit(void){
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);

	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);

	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);

	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if(ch != EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}
	return 0;
}

int shiftDir(int pos, int row){
	int shiftdir = dir[row%2];
	return (pos + shiftdir + COLUMN - 1)%(COLUMN - 1); // 50 - 1 column is needed, starting from 1.
}

void swap(int &x, int &y){
	int tmp = x;
	x= y;
	y = tmp;
	return;
}

void *logs_move( void *t ){
	int currentRow = *((int *)(&t));

	while (finished_state == -1)
	{
		pthread_mutex_lock(&mutex);
		logStart[currentRow] = shiftDir(logStart[currentRow], currentRow);
		logEnd[currentRow] = shiftDir(logEnd[currentRow], currentRow);

		char kbInput = 0;
		if (kbhit() == 1)
		{
			kbInput = getchar();
		}
		
		// starting from left up
		int frogShiftX; 
		int frogShiftY;
		if (kbInput == 'w' || kbInput == 'W')
		{
			frogShiftX = frog.x - 1;
			frogShiftY = frog.y;
		} else if (kbInput == 'a' || kbInput == 'A')
		{
			frogShiftX = frog.x;
			frogShiftY = frog.y - 1;
		} else if (kbInput == 's' || kbInput == 'S')
		{
			frogShiftX = frog.x + 1;
			frogShiftY = frog.y;
		} else if (kbInput == 'd' || kbInput == 'D')
		{
			frogShiftX = frog.x;
			frogShiftY = frog.y + 1;
		} else if (kbInput == 'q' || kbInput == 'Q')
		{
			frogShiftX = frog.x;
			frogShiftY = frog.y;
			finished_state = 2;
		} else {
			frogShiftX = frog.x;
			frogShiftY = frog.y;
		}

		if (frogShiftX == 0) // gamer win
		{
			finished_state = 0;
		}
		if (frogShiftX > ROW) // outside window, down
		{
			frogShiftX = ROW;
		}
		if (frogShiftY + dir[frogShiftX%2] < 0 || frogShiftY + dir[frogShiftY%2] >= COLUMN) // outside window, left, right
		{
			finished_state = 1;
		}
		

		for (int j = 0; j < COLUMN-1; ++j) // up, down boundary
		{
			map[0][j] = '|';
			map[ROW][j] = '|';
		}
		for (int j = logStart[currentRow]; j != logEnd[currentRow]; j=shiftDir(j, currentRow)) // output log
		{
			map[currentRow][j] = '=';
		}
		for (int j = logEnd[currentRow]; j != logStart[currentRow]; j=shiftDir(j, currentRow)) // erase log
		{
			map[currentRow][j] = ' ';
		}
		
		// move
		swap(frog.x, frogShiftX);
		swap(frog.y, frogShiftY);
		
		if (frog.x != ROW)
		{
			if (frog.x == currentRow)
			{
				if (map[frog.x][frog.y + dir[currentRow%2]] != ' ')// not in river
				{
					frog.y += dir[frog.x % 2];
				} else { // in river
					finished_state = 1;
				}
			}
		}

		map[frog.x][frog.y] = '0';

		printf("\033[2J\033[1;1H"); // clear window
		for (int i = 0; i <= ROW; ++i)
		{
			puts(map[i]);
		}
		
		pthread_mutex_unlock(&mutex);
		usleep(logSpeed);
		
	}
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}

int main( int argc, char *argv[] ){
	pthread_mutex_init(&mutex, NULL);
	// Initialize the river map and frog's starting position
	memset( map , 0, sizeof( map ) ) ;
	int i , j ; 
	for( i = 1; i < ROW; ++i ){	
		for( j = 0; j < COLUMN - 1; ++j )	
			map[i][j] = ' ' ;  
	}	

	// init log pos
	for (i = 1; i < ROW; ++i)
	{
		logStart[i] = rand()%(COLUMN-1);
		logEnd[i] = (logStart[i] - ((2 * (i%2))-1)*17) % (COLUMN-1);
	}

	for( j = 0; j < COLUMN - 1; ++j )	
		map[ROW][j] = map[0][j] = '|' ;

	for( j = 0; j < COLUMN - 1; ++j )	
		map[0][j] = map[0][j] = '|' ;

	frog = Node( ROW, (COLUMN-1) / 2 ) ; 
	map[frog.x][frog.y] = '0' ; 

	//Print the map into screen
	for( i = 0; i <= ROW; ++i)	
		puts( map[i] );

	/*  Create pthreads for wood move and frog control.  */
	pthread_t pthreadMove[ROW-1];
	for (int i = 0; i < ROW-1; ++i)
	{
		int threadCreate = pthread_create(&pthreadMove[i], NULL, logs_move, (void *)(i+1));
	}
	for (int i = 0; i < ROW-1; ++i)
	{
		pthread_join(pthreadMove[i], NULL);
	}
	
	printf("\033[2J\033[1;1H");
	for (int i = 0; i <= ROW; ++i)
	{
		puts(map[i]);
	}
	
	
	/*  Display the output for user: win, lose or quit.  */
	printf("Result for the Game is: %s\n",finished_msg[finished_state]);

	pthread_exit(NULL);

	return 0;

}
