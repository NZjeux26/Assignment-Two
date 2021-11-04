
void highscore_display();
void instructions_display();
void goosegame();
void goosecheck(int check);
void highscore(int score);
void brickgame();
void brickgamemktwo();

extern time_t time_now;
extern struct tm *tm_info;
extern int is_emulator;

extern const char *tag;
int obtain_time(void);