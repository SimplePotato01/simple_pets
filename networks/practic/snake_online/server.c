#include "common.h"

GameState game;
int client_socks[MAX_PLAYERS];
int num_clients = 0;

// Func for the tor-coord
Point wrap_coord(Point p) {
    	if (p.x < 1) p.x = WIDTH;
    	if (p.x > WIDTH) p.x = 1;
    	if (p.y < 1) p.y = HEIGHT;
    	if (p.y > HEIGHT) p.y = 1;
    	return p;
}

void generate_food();

void init_game() {
    	memset(&game, 0, sizeof(game));
    	game.num_players = 2;
    	game.game_over = 0;
    
    	// Snake No 1 (left-upside)
    	game.snakes[0].length = 3;
    	game.snakes[0].score = 0;
    	game.snakes[0].alive = 1;
    	game.snakes[0].dir = RIGHT;
    	game.snakes[0].body[0] = (Point){8, 8};
    	game.snakes[0].body[1] = (Point){7, 8};
    	game.snakes[0].body[2] = (Point){6, 8};
    
    	// Snake No 2 (right-down)
    	game.snakes[1].length = 3;
    	game.snakes[1].score = 0;
    	game.snakes[1].alive = 1;
    	game.snakes[1].dir = LEFT;
    	game.snakes[1].body[0] = (Point){WIDTH - 7, HEIGHT - 7};
    	game.snakes[1].body[1] = (Point){WIDTH - 6, HEIGHT - 7};
    	game.snakes[1].body[2] = (Point){WIDTH - 5, HEIGHT - 7};
    
    	srand(time(NULL));
    	generate_food();
}

void generate_food() {
    	int ok;
    	do {
        	ok = 1;
        	game.food.x = rand() % WIDTH + 1;
        	game.food.y = rand() % HEIGHT + 1;
        	for (int p = 0; p < 2; p++)
            		for (int i = 0; i < game.snakes[p].length; i++)
                		if (game.snakes[p].body[i].x == game.food.x && game.snakes[p].body[i].y == game.food.y)
                    			ok = 0;
    	} while (!ok);
}

void move_snake(int p) {
    	if (!game.snakes[p].alive) return;
    
    	Point newhead = game.snakes[p].body[0];
    	switch (game.snakes[p].dir) {
        	case UP: newhead.y--; break;
        	case DOWN: newhead.y++; break;
        	case LEFT: newhead.x--; break;
        	case RIGHT: newhead.x++; break;
    	}
    
    	// straching for tor
	newhead = wrap_coord(newhead);
    
    	// Self collision
    	for (int i = 0; i < game.snakes[p].length; i++)
        	if (game.snakes[p].body[i].x == newhead.x && game.snakes[p].body[i].y == newhead.y) {
            		game.snakes[p].alive = 0;
            		printf("Player %d hit self!\n", p+1);
            		return;
        	}
    
    	// Crush of two snakes
    	for (int i = 0; i < game.snakes[1-p].length; i++)
        	if (game.snakes[1-p].body[i].x == newhead.x && game.snakes[1-p].body[i].y == newhead.y) {
            		game.snakes[p].alive = 0;
            		printf("Player %d hit player %d!\n", p+1, 2-p);
            		return;
        	}
    
    	// Movement 
    	for (int i = game.snakes[p].length; i > 0; i--)
        	game.snakes[p].body[i] = game.snakes[p].body[i-1];
    	game.snakes[p].body[0] = newhead;
    
    	// Food
    	if (newhead.x == game.food.x && newhead.y == game.food.y) {
        	game.snakes[p].length++;
        	game.snakes[p].score++;
        	generate_food();
        	printf("Player %d ate food! Score: %d, Length: %d\n", p+1, game.snakes[p].score, game.snakes[p].length);
    	}
    
    	// Checking the winning cond
    	if (!game.snakes[0].alive || !game.snakes[1].alive) {
        	game.game_over = 1;
        	printf("Game over! Winner: Player %d\n", game.snakes[0].alive ? 1 : 2);
    	}
}

void broadcast() {
    	for (int i = 0; i < num_clients; i++) {
        	int n = send(client_socks[i], &game, sizeof(GameState), 0);
        	if (n != sizeof(GameState))
            		perror("send");
    	}
}

void* client_handler(void* arg) {
    	int idx = *(int*)arg;
    	char ch;
    	while (!game.game_over && game.snakes[idx].alive) {
        	if (recv(client_socks[idx], &ch, 1, 0) > 0) {
            		Direction nd;
            		switch (ch) {
                		case 'w': nd = UP; break;
                		case 's': nd = DOWN; break;
                		case 'a': nd = LEFT; break;
                		case 'd': nd = RIGHT; break;
                		default: continue;
            		}
            		if ((game.snakes[idx].dir == UP && nd != DOWN) || 
					(game.snakes[idx].dir == DOWN && nd != UP) ||
                			(game.snakes[idx].dir == LEFT && nd != RIGHT) ||
                			(game.snakes[idx].dir == RIGHT && nd != LEFT))
                			game.snakes[idx].dir = nd;
        	}
        	usleep(10000);
    	}
    	return NULL;
}

int main() {
    	int server_fd, opt = 1;
    	struct sockaddr_in addr;
    	server_fd = socket(AF_INET, SOCK_STREAM, 0);
    	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    	addr.sin_family = AF_INET;
    	addr.sin_addr.s_addr = INADDR_ANY;
    	addr.sin_port = htons(PORT);
    	bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    	listen(server_fd, 2);
    	printf("-----\n");
    	printf("TOROIDAL SNAKE GAME SERVER\n");
    	printf("Port: %d\n", PORT);
    	printf("Field: %dx%d (toroidal)\n", WIDTH, HEIGHT);
    	printf("-----\n\n");
    	printf("Waiting for 2 players...\n");
    
    	while (num_clients < 2) {
        	int s = accept(server_fd, NULL, NULL);
        	client_socks[num_clients] = s;
        	int id = num_clients;
        	send(s, &id, sizeof(int), 0);
        	printf("Player %d connected\n", num_clients+1);
        	num_clients++;
    	}
    
    	printf("\nBoth connected! Starting game in 2 seconds...\n");
    	printf("(Toroidal mode: exiting border wraps around)\n\n");
    	sleep(2);
    	init_game();
    
    	pthread_t th[2];
    	for (int i = 0; i < 2; i++) {
        	int* p = malloc(sizeof(int));
        	*p = i;
        	pthread_create(&th[i], NULL, client_handler, p);
    	}
    
    	printf("GAME STARTED!\n");
    	int tick = 0;
    	while (!game.game_over) {
        	move_snake(0);
        	move_snake(1);
        	broadcast();
        	if (tick % 20 == 0 && tick > 0) {
            		printf("\rGame running... Tick: %d", tick);
            		fflush(stdout);
        	}
        	tick++;
        	usleep(150000); // 150ms
    	}
    
    	printf("\n\n=== GAME OVER ===\n");
    	printf("Final scores:\n");
    	printf("  Player 1: %d\n", game.snakes[0].score);
    	printf("  Player 2: %d\n", game.snakes[1].score);
    	printf("Winner: Player %d\n", game.snakes[0].alive ? 1 : 2);
    	printf("\nServer will shut down in 5 seconds...\n");
    	sleep(5);
    
    	for (int i = 0; i < 2; i++)
        	close(client_socks[i]);
    	close(server_fd);
    	return 0;
}
