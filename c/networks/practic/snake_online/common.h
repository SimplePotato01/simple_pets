#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define WIDTH 40
#define HEIGHT 20
#define MAX_SNAKE_LEN 400
#define MAX_PLAYERS 2
#define PORT 8080

typedef enum {
    	UP = 'w',
    	DOWN = 's',
    	LEFT = 'a',
    	RIGHT = 'd'
} Direction;

typedef struct {
    	int x, y;
} Point;

typedef struct {
    	Point body[MAX_SNAKE_LEN];
    	int length;
    	int score;
    	int alive;
    	int dir;
} SnakePacked;

typedef struct {
    	Point food;
    	SnakePacked snakes[MAX_PLAYERS];
    	int num_players;
    	int game_over;
} GameState;

#define RESET_COLOR "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define CLEAR_SCREEN "\033[2J"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

#endif
