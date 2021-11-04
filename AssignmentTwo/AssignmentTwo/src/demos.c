
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include "fonts.h"
#include <graphics.h>
#include "demos.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <nvs_flash.h>
#include <time.h>
#include "input_output.h"

const char *tag="T Display";
// voltage reference calibration for Battery ADC
uint32_t vref;
time_t time_now;
struct tm *tm_info;

/* Known Bugs:
1. Quickly double pressing a button will cause the player to be stuck on that side.
S. Press the button on the side it's stuck once quickly and it will "Release"

2. Blocks pass through the player without ending the game
Sometimes when a block hits the player it wont trigger, def happens when a block (at the start of the game) spawns right on the player. HOWEVER sometimes three blocks can pass through
before it triggers. Suspect that if the s in the forloop doesn't match the block it wont trigger the end game. Seems more an issue with the geese. 

3. Games ends right at the start/ before it appears on screen
Seems to happen when blocks don't trigger the end game, and suddenly the backlog is procesed.

4. Game will end when player and block not close to eachother.
Simlar issues to 3

5. Goose Game endlsy replays and doesn't allow player to quit.
I added the break in the if statement to hopfully stop this, it hasn't happened since but i haven't truly tested

6. Highscore check doesn't work.
Sometimes the HS check will allow the player to play the goose game even when the HS isn't greater than 4000. flipped the the if statement to fix and it works now.
However it worked on Wednesday and Thursday so IDK wtf is going on.

7. Missles hits don't always register.
Same reason as the hits not always ending a game, only if the brick being called is hit will it work.
*/

void showfps() {
    static uint64_t current_time=0;
    static uint64_t last_time=0;
    static int frame=0;
    current_time = esp_timer_get_time();
    if ((frame++ % 20) == 1) {
        printf("FPS:%f %d\n", 1.0e6 / (current_time - last_time),frame);
        vTaskDelay(1);
    }
    last_time=current_time;
}

void instructions_display(){//instructions to player
     set_orientation(LANDSCAPE);
    while(1){
        cls(0);//sets background colour
        gprintf("Hello player!\n");
        gprintf("\n");
        gprintf("The aim of this game is avoid being\nstruck by the falling blocks.\n");//need to find a way to autowrap the text but this will have to do.
        gprintf("Use the buttons at the bottom\nof the LCD to move your player,\nif you are struck you die and the\ngame will reset.\n");

        flip_frame();
        showfps();
        key_type key=get_input();
        if(key==LEFT_DOWN) return;
    }
}

int scoreboard[5];
void highscore_display(){//displays the HS board.

    while(1){
     cls(0);
     setFont(FONT_DEJAVU18);
     setFontColour(255, 0, 0);
     gprintf("High Scores\n");
     gprintf("-------------\n");
     for (int s = 0; s < 5; s++)//display HS board
     {
        setFont(FONT_DEJAVU18);
        setFontColour(255, 255, 255);
        gprintf("%d: %d\n",s + 1, scoreboard[s]);//to be modded when or if i can work out how to add names.
     }

     flip_frame();
     showfps();
     key_type key=get_input();
     if(key==RIGHT_DOWN){
         set_orientation(1-get_orientation());
     }
     if(key==LEFT_DOWN) return;
    }
}

typedef struct obj {//struct for player parameters
    float x;
    float y;
    int w;
    int h;
    float xvel;
    float yvel;
    int health;
    uint16_t colour;
} obj;

typedef struct pos {//For stars
    float x;
    float y;
    float speed;
    int colour;
} pos;

#define NSTARS 100
extern image_header planetwo;//headers for images
extern image_header gooseOne;
extern image_header gooseTwo;
extern image_header gooseThree;
extern image_header gooseFour;

//I made this thinking 
void goosecheck(int check){//sometimes this check doesn't work and it will allow it when it shouldn't.
    set_orientation(LANDSCAPE);
    while(1){
        cls(0);
        if(check == 1){
          if(scoreboard[0] >= 4000){
          goosegame();
          break;
          }

          else{
          setFont(FONT_DEJAVU18);
          setFontColour(255, 255, 255);
          gprintf("Please attain a score of\n4000 or more in bricks\nbefore taking on the geese!");
          flip_frame();
          showfps();
          key_type key=get_input();
          if(key==LEFT_DOWN) return;
          }
        }

        if(check == 2){
          if(scoreboard[0] >= 4000){
          brickgamemktwo();
          break;
          }

          else{
          setFont(FONT_DEJAVU18);
          setFontColour(255, 255, 255);
          gprintf("Whoaa!, slow down\ncowboy! you need 4000\npoints or more in bricks\nbefore getting missles!");
          flip_frame();
          showfps();
          key_type key=get_input();
          if(key==LEFT_DOWN) return;
          }

        }
    }
}

void goosegame(){

    static char score_str[256];

    static pos stars[NSTARS];
    static obj blocks[6];
    
    set_orientation(PORTRAIT);

   //starfield for background. 
    for(int i=0;i<NSTARS;i++) {
        stars[i].x=rand()%display_width;
        stars[i].y=rand()%display_height;
        stars[i].speed=(rand()%512+64)/256.0;
        stars[i].colour=0xDEFB;
        stars[i].speed=(rand()%256+64)/124.0;
    }

    obj player;
    player.colour = 125.255;
    player.x = 65;
    player.y = 210; 
    player.w = planetwo.width;
    player.h = planetwo.height;

    //creates the blocks, with random stats for position and size.
    for(int s=0; s<6; s++){
        blocks[s].x=rand()%display_width;
        blocks[s].y=rand()%(124-0+1)+0;
        blocks[s].yvel=-0.85;
    }

    setFont(FONT_UBUNTU16);
    setFontColour(255, 255, 255);
    int keys[2]={1,1};
    int score = 0;
    int play = 1;
    while(play){
        cls(rgbToColour(80,224,224));
        
        //scores Maybe to be moved
        gprintf("Score: %d\n",score);
        setFontColour(100, 100, 155);
        gprintf("HiScore: %d\n",scoreboard[0]);

        //draws the stars
        for(int i=0; i<NSTARS; i++){
            draw_pixel(stars[i].x,stars[i].y,stars[i].colour);
        }

        //draws the rectangles, when the block makes it past the player, it is spawned back into a random X/Y on screen
        for(int s=0; s<7; s++){
            
            //Each type of goose is only generated on certain numbers. Cannot seem to use logic functions to help.
            draw_image(&gooseOne,blocks[0].x,blocks[0].y);
            blocks[0].h = gooseOne.height;
            blocks[0].w = gooseOne.width;//wrong goose

            draw_image(&gooseTwo,blocks[1].x,blocks[1].y);
            blocks[1].h = gooseTwo.height;
            blocks[1].w = gooseTwo.width;

            draw_image(&gooseOne,blocks[2].x,blocks[2].y);
            blocks[2].h = gooseOne.height;
            blocks[2].w = gooseOne.width;

            draw_image(&gooseThree,blocks[3].x,blocks[3].y);
            blocks[3].h = gooseThree.height;
            blocks[3].w = gooseThree.width;

            draw_image(&gooseTwo,blocks[4].x,blocks[4].y);
            blocks[4].h = gooseTwo.height;
            blocks[4].w = gooseTwo.width;

            draw_image(&gooseThree,blocks[5].x,blocks[5].y);
            blocks[5].h = gooseThree.height;
            blocks[5].w = gooseThree.width;

            draw_image(&gooseFour,blocks[6].x,blocks[6].y);
            blocks[6].h = gooseFour.height;
            blocks[6].w = gooseFour.width;
            
            blocks[s].y-= blocks[s].yvel;

            //When block passes player reposition it, add to score + reposition the block
            if(blocks[s].y > 225){
                score = score + 100;
                blocks[s].x=rand()%display_width;
                blocks[s].y=rand()%(125-0+1)+0;//set to 125 so doesn't spawn to close to the player
                blocks[s].yvel = blocks[s].yvel - 0.085;//increases speed slightly everytime the block passes the player.
            }//if

            //if any of the blocks touch any part of the player stop the while loop and exit the game. //Note that this is the box of the image, which may mean sometimes it's a bit off
            if((blocks[s].y+blocks[s].h/2)>player.y && (blocks[s].x>player.x) && (blocks[s].x<player.x+player.w) && (blocks[s].y-blocks[s].h/2)<player.y){
                play = 0;
            }
        }//for

        draw_image(&planetwo,player.x,player.y);

        key_type key=get_input();
        switch(key) {
            case LEFT_DOWN: keys[0]=0;break;
            case LEFT_UP: keys[0]=1;break;
            case RIGHT_DOWN: keys[1]=0;break;
            case RIGHT_UP: keys[1]=1;break;
            case NO_KEY: break;
        }

        //how to move retanlge on small amount rather than full length on button press.
        if(keys[0] == 0){//left
            player.x-=2;
            if(player.x<0) player.x=0;//Stops rectangle at wall of screen.
        }

        if(keys[1]==0){//right
            player.x+=2;
            if(player.x>134-player.w) player.x=(134-20);
        }
        
        flip_frame();
        showfps();
        
    }//while

    setFont(FONT_DEJAVU18);
    setFontColour(255, 0, 0);
    print_xy("Game Over", CENTER, CENTER);
    setFontColour(255, 0, 0);
    snprintf(score_str,64,"Score: %d",score);
    print_xy(score_str,CENTER,LASTY+15);

    if(score > scoreboard[0]){//print on screen if player beats the highscore.
        setFontColour(255, 0, 0);
        sniprintf(score_str,60,"New High Score!");
        print_xy(score_str,CENTER,LASTY+15);
    }

    highscore(score);//pass score to HS calculater
    flip_frame();

    vTaskDelay(500/portTICK_PERIOD_MS);
    while(get_input());
    while(get_input()!=RIGHT_DOWN)
    vTaskDelay(1);
}//Goose

int BLOCKC = 2;//set block starter amount
void brickgame(){
    static char score_str[256];//how are things being added to this?
    static pos stars[NSTARS];
    static obj blocks[10];//araay of blocks, theroical maxium of blocks to be displayed on screen, good luck if you get that high!
    
    set_orientation(PORTRAIT);

   //starfield for background. 
    for(int i=0;i<NSTARS;i++) {
        stars[i].x=rand()%display_width;
        stars[i].y=rand()%display_height;
        stars[i].speed=(rand()%512+64)/256.0;
        stars[i].colour=rand();
        stars[i].speed=(rand()%512+64)/256.0;
    }
    //player stats
    obj player;
    player.colour = 0xF986;
    player.x = 65;
    player.y = 225; 
    player.w = 25;
    player.h = 10;

    //creates the intional blocks, with random stats for position and size. I also cannot spell so good luck
    for(int s=0; s<BLOCKC; s++){
        blocks[s].x=rand()%display_width;
        blocks[s].y=rand()%(80-0+1)+0;
        blocks[s].w=rand()%(40-10+1)+10;
        blocks[s].h=rand()%(30-5+1)+5;
        blocks[s].colour=rand()%(50000-0)+0;
        blocks[s].yvel=-0.75;
    }

    setFont(FONT_UBUNTU16);
    setFontColour(255, 255, 255);
    int keys[2]={1,1};
    int score = 0;//sets the score for the session
    int play = 1;
    while(play){
        cls(0);
        //scores Maybe to be moved
        gprintf("Score: %d\n",score);//displays the last games score
        setFontColour(100, 100, 155);
        gprintf("HiScore: %d\n",scoreboard[0]);//displays the highscore
        
        //Timer for creating new blocks
        uint64_t time = esp_timer_get_time();
        int sec;
        sec = time/10000;

        if(sec%200 == 0){//if mod = 0 then generate a new block at a random positon
        //printf("BLOCKN %d\n",BLOCKC); //FOR TESTING
        BLOCKC++;
        blocks[BLOCKC].x=rand()%display_width;
        blocks[BLOCKC].y=rand()%(80-0+1)+0;
        blocks[BLOCKC].w=rand()%(40-10+1)+10;
        blocks[BLOCKC].h=rand()%(30-5+1)+5;
        blocks[BLOCKC].colour=rand()%(50000-0)+0;
        blocks[BLOCKC].yvel=-0.75;//intional velocity
        }

        if(sec%400 == 0){//removes a block every so often so the rate of growth doesn't overwelm player
            BLOCKC--;
        }

        //draws the stars
        for(int i=0; i<NSTARS; i++){
            draw_pixel(stars[i].x,stars[i].y,stars[i].colour);
        }

        //draws th rectangles
        for(int s=0; s<BLOCKC; s++){
            draw_rectangle(blocks[s].x,blocks[s].y,blocks[s].w,blocks[s].h,blocks[s].colour);
        }//for
        
        //manages the rectangles
        for(int i=0; i <BLOCKC; i++){
            blocks[i].y-= blocks[i].yvel;//starts moving the blocks

            if(blocks[i].y > 225){//if they block passes the player, add to the score, move the block to a random spot and increase it's value.
            score = score + 100;
            blocks[i].x=rand()%display_width;
            blocks[i].y=rand()%(100-0+1)+0;
            blocks[i].yvel = blocks[i].yvel - 0.095;//increases speed slightly everytime the block passes the player.
            }//if
            //If the block hits the player then stop the game
            if((blocks[i].y+blocks[i].h)>player.y && (blocks[i].x>player.x) && (blocks[i].x<player.x+player.w) && (blocks[i].y-blocks[i].h)<player.y){
            play = 0;
            BLOCKC = 2;//resets the blocks to only be two at the start, or else it will start with the amount of blocks you finished your last game with
            }
        }
       
        draw_rectangle(player.x,player.y,player.w,player.h,player.colour);//draws player rectangle

        key_type key=get_input();
        switch(key) {
            case LEFT_DOWN: keys[0]=0;break;
            case LEFT_UP: keys[0]=1;break;
            case RIGHT_DOWN: keys[1]=0;break;
            case RIGHT_UP: keys[1]=1;break;
            case NO_KEY: break;
        }

        //moves player left and right
        if(keys[0] == 0){//left
            player.x-=2;
            if(player.x<0) player.x=0;//Stops rectangle at wall of screen.
        }

        if(keys[1]==0){//right
            player.x+=2;
            if(player.x>134-player.w) player.x=(134-20);
        }
        
        flip_frame();
        showfps();
        
    }//while
    //once game is finished, display game over, the score attained and pass it to the HS board calculator
    setFont(FONT_DEJAVU18);
    setFontColour(255, 0, 0);
    print_xy("Game Over", CENTER, CENTER);//prints game over and the score in the match
    setFontColour(0,255,0);
    snprintf(score_str,64,"Score: %d",score);
    print_xy(score_str,CENTER,LASTY+15);
    
    if(score > scoreboard[0]){//print on scren if player beats the #1 spot.
        setFontColour(255, 0, 0);
        sniprintf(score_str,60,"New High Score!");
        print_xy(score_str,CENTER,LASTY+15);
    }
    
    highscore(score);//pass score to HS calculater

    flip_frame();
    vTaskDelay(500/portTICK_PERIOD_MS);
    while(get_input());
    while(get_input()!=RIGHT_DOWN)
    vTaskDelay(1);
}//brick

void brickgamemktwo(){
    static char score_str[256];//how are things being added to this?
    static pos stars[NSTARS];
    static obj blocks[10];//araay of blocks, theroical maxium of blocks to be displayed on screen, good luck if you get that high!
    
    set_orientation(PORTRAIT);

   //starfield for background. 
    for(int i=0;i<NSTARS;i++) {
        stars[i].x=rand()%display_width;
        stars[i].y=rand()%display_height;
        stars[i].speed=(rand()%512+64)/256.0;
        stars[i].colour=rand();
        stars[i].speed=(rand()%512+64)/256.0;
    }

    //player stats
    obj player;
    player.colour = 0xF986;
    player.x = 65;
    player.y = 225; 
    player.w = 25;
    player.h = 10;
    player.health = 100;
    
    obj missle;
    missle.y = 225;
    missle.w = 10;
    missle.h = 25;
    missle.yvel = 0.6;
    missle.colour = 0xF980;

    //creates the intional blocks, with random stats for position and size. I also cannot spell so good luck
    for(int s=0; s<BLOCKC; s++){
        blocks[s].x=rand()%display_width;
        blocks[s].y=rand()%(80-0+1)+0;
        blocks[s].w=rand()%(40-10+1)+10;
        blocks[s].h=rand()%(30-5+1)+5;
        blocks[s].colour=rand()%(50000-0)+0;
        blocks[s].yvel=-0.75;
        blocks[s].health = 100;//blocks[s].h * blocks[s].w;//Health equals the area of the rectangle. Seems to be a little high, trying fixed value.
    }

    setFont(FONT_UBUNTU16);
    setFontColour(255, 255, 255);
    int keys[2]={1,1};
    int score = 0;//sets the score for the session
    int play = 1;
    while(play){
        cls(0);
        //scores Maybe to be moved
        gprintf("Score: %d\n",score);//displays the last games score
        setFontColour(100, 100, 155);//changes size when bang is called but idc
        gprintf("HiScore: %d\n",scoreboard[0]);//displays the highscore
        gprintf("Health: %d",player.health);
        
        //Timer for creating new blocks
        uint64_t time = esp_timer_get_time();
        int sec;
        sec = time/10000;

        if(sec%200 == 0){//if mod = 0 then generate a new block at a random positon
        //printf("BLOCKN %d\n",BLOCKC); //FOR TESTING
        BLOCKC++;
        blocks[BLOCKC].x=rand()%display_width;
        blocks[BLOCKC].y=rand()%(80-0+1)+0;
        blocks[BLOCKC].w=rand()%(40-10+1)+10;
        blocks[BLOCKC].h=rand()%(30-5+1)+5;
        blocks[BLOCKC].colour=rand()%(50000-0)+0;
        blocks[BLOCKC].yvel=-0.75;//intional velocity
        blocks[BLOCKC].health = blocks[BLOCKC].h * blocks[BLOCKC].w;//Health equals the area of the rectangle.
        }

        if(sec%400 == 0){//removes a block every so often so the rate of growth doesn't overwelm player
            BLOCKC--;
        }

        //draws the stars
        for(int i=0; i<NSTARS; i++){
            draw_pixel(stars[i].x,stars[i].y,stars[i].colour);
        }

        //draws th rectangles
        for(int s=0; s<BLOCKC; s++){
            draw_rectangle(blocks[s].x,blocks[s].y,blocks[s].w,blocks[s].h,blocks[s].colour);
        }//for
        
        //manages the rectangles
        for(int i=0; i <BLOCKC; i++){
            blocks[i].y-= blocks[i].yvel;//starts moving the blocks

            if(blocks[i].y > 225){//if they block passes the player, add to the score, move the block to a random spot and increase it's value.
            score = score + 50;
            blocks[i].x=rand()%display_width;
            blocks[i].y=rand()%(100-0+1)+0;
            blocks[BLOCKC].colour=rand()%(50000-0)+0;
            blocks[i].yvel = blocks[i].yvel - 0.095;//increases speed slightly everytime the block passes the player.
            }//if
            //If the block hits the player reduce it's health, and subtract from the score.
            if((blocks[i].y+blocks[i].h)>player.y && (blocks[i].x>player.x) && (blocks[i].x<player.x+player.w) && (blocks[i].y-blocks[i].h)<player.y){
            player.health = player.health - 3;
            //score = score - 50;
            }

            if(player.health <= 0){
                play = 0;
                BLOCKC = 2;//resets the blocks to only be two at the start, or else it will start with the amount of blocks you finished your last game with
            }
        }
       
        draw_rectangle(player.x,player.y,player.w,player.h,player.colour);//draws player rectangle     

        key_type key=get_input();
        switch(key) {
            case LEFT_DOWN: keys[0]=0;break;
            case LEFT_UP: keys[0]=1;break;
            case RIGHT_DOWN: keys[1]=0;break;
            case RIGHT_UP: keys[1]=1;break;
            case NO_KEY: break;
        }

        //moves player left and right
        if(keys[0]==0){//left
            player.x-=2;
            if(player.x<0) player.x=0;//Stops rectangle at wall of screen.
        }

        if(keys[1]==0){//right
            player.x+=2;
            if(player.x>134-player.w) player.x=(134-20);
        }

        if(keys[0]==0 && keys[1]==0){//if both buttons are pressed fire the missle.
            //printf("FIRE!\n");
            draw_rectangle(player.x,missle.y,missle.w,missle.h,missle.colour);
            missle.y-=missle.yvel*13;

            if(time%200 == 0){
              setFont(FONT_DEJAVU18);
              setFontColour(200, 60, 80);
              print_xy("BANG!", player.x, player.y);       
            }

            if(missle.y <= 1){//if the missle goes off screen, "Reload it"
                missle.x = (player.x + 30);
                missle.y = 225;
            }

            for(int i=0; i<BLOCKC; i++){
             //If the missle hits the block, reduce its health
             if((missle.y+missle.h/2)>blocks[i].y && (missle.x>blocks[i].x) && (missle.x<blocks[i].x+blocks[i].w) && (missle.y-missle.h/2)<blocks[i].y){
                  blocks[i].health = blocks[i].health - 100;//missle hits seem to be not very consistant, must be a AIM-9B
                  printf("Block %d Health = %d\n",i,blocks[i].health);

                  if(blocks[i].health <= 0){//if health falls below 0, make it white, move it back to start and add to score.
                  //destroy and add to score.
                  printf("Killed Block: %d\n",i);
                  score = score + 100;
                  blocks[i].x=rand()%display_width;
                  blocks[i].y=rand()%(100-0+1)+0;
                  blocks[i].health = 100;
                 // blocks[i].colour = 0xFFDE;//set white for testing

                  if(BLOCKC == 0){//remove block if there are more than two and the block is "Killed" Chnaged to allow to get to zero, since the first two blocks wont dissappear 
                      BLOCKC = 2;
                    }//ifthree    
                  }//iftwo
                }//ifone
            }//for
        }
          else{//when the player releases one of the buttons, reset the position of the missle, if you don't the missle starts where you left it.
            missle.y=225;//fixed y value since if it's set to the player y it will not move since it is constantly being updated.
            missle.x = (player.x + 30);
          }
      flip_frame();
    }//while

    //once game is finished, display game over, the score attained and pass it to the HS board calculator
    setFont(FONT_DEJAVU18);
    setFontColour(255, 0, 0);
    print_xy("Game Over", CENTER, CENTER);//prints game over and the score in the match
    setFontColour(0,255,0);
    snprintf(score_str,64,"Score: %d",score);
    print_xy(score_str,CENTER,LASTY+15);
    
    if(score > scoreboard[0]){//print on scren if player beats the #1 spot.
        setFontColour(255, 0, 0);
        sniprintf(score_str,60,"New High Score!");
        print_xy(score_str,CENTER,LASTY+15);
    }
    
    highscore(score);//pass score to HS calculater

    flip_frame();
    vTaskDelay(500/portTICK_PERIOD_MS);
    while(get_input());
    while(get_input()!=RIGHT_DOWN)
    vTaskDelay(1);
}

void swap(int *a, int *b){
    int temp = *a;
    *a = *b;
    *b = temp;
}//swap

void highscore(int score){//Sorting for the highscore board, uses a bubble sort so that a score can be place in any of the five spots and all the scores filter down into the right spots
    int value = score;

    bool swapping = true;
    while (swapping){//modified bubble sort......i think, i did do 201
        swapping = false;
        for (int i = 0; i < 4; i++){
            if(value > scoreboard[i]){
                swap(&value,&scoreboard[i]);
                swapping = true;
            }
        }
    }   
}

