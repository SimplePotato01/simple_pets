#include "common.h"

int sock;
struct termios orig_term;

void setup_terminal() {
    	struct termios term;
    	tcgetattr(0, &orig_term);
    	term = orig_term;
    	term.c_lflag &= ~(ICANON | ECHO);
    	term.c_cc[VMIN] = 0;
    	term.c_cc[VTIME] = 0;
    	tcsetattr(0, TCSANOW, &term);
}

void restore_terminal() {
    	tcsetattr(0, TCSANOW, &orig_term);
}

int recv_full(int sock, void *buf, int len) {
    	int total = 0;
    	while (total < len) {
        	int n = recv(sock, (char*)buf + total, len - total, 0);
        	if (n <= 0) return n;
        	total += n;
    	}
    	return total;
}

int main(int argc, char *argv[]) {
    	if (argc != 2) {
        	printf("Usage: %s <IP>\n", argv[0]);
        	printf("Example: %s 127.0.0.1\n", argv[0]);
        	return 1;
    	}
    
    	struct sockaddr_in addr;
    	sock = socket(AF_INET, SOCK_STREAM, 0);
    	addr.sin_family = AF_INET;
    	addr.sin_port = htons(PORT);
    	inet_pton(AF_INET, argv[1], &addr.sin_addr);
    
    	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        	perror("connect failed");
        	return 1;
    	}
    
    	int my_id;
    	if (recv_full(sock, &my_id, sizeof(int)) != sizeof(int)) {
        	printf("Error getting player ID\n");
        	return 1;
    	}
    
    	printf("-----\n");
    	printf("TOROIDAL SNAKE GAME\n");
    	printf("You are Player %d\n", my_id+1);
    	printf("Controls: W/A/S/D\n");
    	printf("Toroidal: exit border = appear opposite\n");
    	printf("-----\n\n");
    	printf("Starting in 2 seconds...\n");
    	sleep(2);
    
    	setup_terminal();
    	atexit(restore_terminal);
    	setbuf(stdout, NULL);
    
    	GameState gs;
    	while (1) {
        	if (recv_full(sock, &gs, sizeof(GameState)) != sizeof(GameState))
            	break;
        
        	if (system("clear") == -1) {
            		printf(CLEAR_SCREEN);
        	}
        
        	// Drawing the field
        	for (int y = 0; y <= HEIGHT + 1; y++) {
            		for (int x = 0; x <= WIDTH + 1; x++) {
                		// Bounds
                		if (y == 0 || y == HEIGHT + 1) {
                    			if (x == 0 || x == WIDTH + 1)
                        			printf("+");
                    			else
                        			printf("-");
                    			continue;
                		}
                		if (x == 0 || x == WIDTH + 1) {
                    			printf("|");
                    			continue;
                		}
                
                		// In-field
                		if (gs.food.x == x && gs.food.y == y) {
                    			printf(RED "*" RESET_COLOR);
                    			continue;
                		}
                
                		int is_body = 0;
                		for (int p = 0; p < gs.num_players; p++) {
                    			for (int i = 0; i < gs.snakes[p].length; i++) {
                        			if (gs.snakes[p].body[i].x == x && gs.snakes[p].body[i].y == y) {
                            				if (p == my_id) {
                                				if (i == 0) printf(GREEN "@" RESET_COLOR);
                                				else printf(GREEN "O" RESET_COLOR);
                            				} else {
                                				if (i == 0) printf(BLUE "&" RESET_COLOR);
                                				else printf(BLUE "o" RESET_COLOR);
                            				}
                            				is_body = 1;
                            				break;
                        			}
                    			}
                    			if (is_body) break;
                		}
                		if (!is_body) printf(" ");
            		}
            		printf("\n");
        	}
        
        	// Info plane
        	printf("\n+-----\n");
        	printf("| TOROIDAL MODE - Borders wrap around!\n");
        	printf("+-----\n");
        	printf("| You (Player id: %d): %-3d points\n", my_id+1, gs.snakes[my_id].score);
        	printf("| Enemy (Player id: %d): %-3d points\n", 2-my_id, gs.snakes[1-my_id].score);
        	printf("| Your length: %-3d\n", gs.snakes[my_id].length);
        	printf("| Enemy length: %-3d\n", gs.snakes[1-my_id].length);
        	printf("| Your status: %-8s\n", gs.snakes[my_id].alive ? "ALIVE" : "DEAD");
        	printf("+-----\n");
        	printf("| Controls: W/A/S/D\n");
        	printf("+-----\n");
        
        	if (gs.game_over) {
            		printf("\n\n");
            		printf("+-----\n");
            		if (gs.snakes[my_id].alive) {
                		printf("| YOU WIN!\n");
            		} else {
                		printf("| you lose...\n");
            		}
            		printf("+-----\n");
            		break;
        	}
        
        	// Process input
        	fd_set fds;
        	struct timeval tv = {0, 0};
        	FD_ZERO(&fds);
        	FD_SET(0, &fds);
        
        	if (select(1, &fds, NULL, NULL, &tv) > 0) {
            		char ch;
            		if (read(0, &ch, 1) == 1) {
                		if (ch == 'w' || ch == 's' || ch == 'a' || ch == 'd') {
                    			send(sock, &ch, 1, 0);
                		}
            		}
        	}
        
        	usleep(50000);
    	}
    
    	restore_terminal();
    	close(sock);
    	printf("\nPress Enter to exit...");
    	getchar();
    	return 0;
}
