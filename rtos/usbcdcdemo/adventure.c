/* From https://github.com/jefftranter/6502
 * 
 * The Abandoned Farm House Adventure
 *
 * Jeff Tranter <tranter@pobox.com>
 *
 * Written in standard C but designed to run on the Apple Replica 1
 * or Apple II using the CC65 6502 assembler.
 *
 * Copyright 2012-2015 Jeff Tranter
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Revision History:
 *
 * Version  Date         Comments
 * -------  ----         --------
 * 0.0      13 Mar 2012  First alpha version
 * 0.1      18 Mar 2012  First beta version
 * 0.9      19 Mar 2012  First public release
 * 1.0      06 Sep 2015  Lower case and other Apple II improvements.
 * 1.1      17 Aug 2017  Moditifed to run on stm32 (Warren Gay)
 */

#include <FreeRTOS.h>
#include <task.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <strings.h> 

#include "usbcdc.h"
#include "common.h"

/* Maximum number of items user can carry */
#define MAXITEMS 5

/* Number of locations */
#define NUMLOCATIONS 32

/* Directions */
typedef enum {
    North,
    South,
    East,
    West,
    Up,
    Down
} Direction_t;

/* Items */
typedef enum {
    NoItem,
    Key,
    Pitchfork,
    Flashlight,
    Lamp,
    Oil,
    Candybar,
    Bottle,
    Doll,
    ToyCar,
    Matches,
    GoldCoin,
    SilverCoin,
    StaleMeat,
    Book,
    Cheese,
    OldRadio,
    LastItem=OldRadio
} Item_t;

/* Locations */
typedef enum {
    NoLocation,
    Driveway1,
    Driveway2,
    Driveway3,
    Driveway4,
    Driveway5,
    Garage,
    WorkRoom,
    Hayloft,
    Kitchen,
    DiningRoom,
    BottomStairs,
    DrawingRoom,
    Study,
    TopStairs,
    BoysBedroom,
    GirlsBedroom,
    MasterBedroom,
    ServantsQuarters,
    LaundryRoom,
    FurnaceRoom,
    VacantRoom,
    Cistern,
    Tunnel,
    Woods24,
    Woods25,
    Woods26,
    WolfTree,
    Woods28,
    Woods29,
    Woods30,
    Woods31,
} Location_t;

/* TABLES */

/* Names of directions */
char *DescriptionOfDirection[] = {
    "north", "south", "east", "west", "up", "down"
};

/* Names of items */
char *DescriptionOfItem[LastItem+1] = {
    "",
    "key",
    "pitchfork",
    "flashlight",
    "lamp",
    "oil",
    "candybar",
    "bottle",
    "doll",
    "toy car",
    "matches",
    "gold coin",
    "silver coin",
    "stale meat",
    "book",
    "cheese",
    "old radio",
};

/* Names of locations */
char *DescriptionOfLocation[NUMLOCATIONS] = {
    "",
    "in the driveway near your car",
    "in the driveway",
    "in front of the garage",
    "in front of the barn",
    "at the door to the house",
    "in the garage",
    "in the workroom of the barn",
    "in the hayloft of the barn",
    "in the kitchen",
    "in the dining room",
    "at the bottom of the stairs",
    "in the drawing room",
    "in the study",
    "at the top of the stairs",
    "in a boy's bedroom",
    "in a girl's bedroom",
    "in the master bedroom next to\na bookcase",
    "in the servant's quarters",
    "in the basement laundry room",
    "in the furnace room",
    "in a vacant room next to a\nlocked door",
    "in the cistern",
    "in an underground tunnel. There are rats here",
    "in the woods near a trapdoor",
    "in the woods",
    "in the woods",
    "in the woods next to a tree",
    "in the woods",
    "in the woods",
    "in the woods",
    "in the woods",
};

/* DATA */

/* Inventory of what player is carrying */
Item_t Inventory[MAXITEMS];

/* Location of each item. Index is the item number, returns the location. 0 if item is gone */
Location_t locationOfItem[LastItem+1];

/* Map. Given a location and a direction to move, returns the location it connects to, or 0 if not a valid move. Map can change during game play. */
Direction_t Move[NUMLOCATIONS][6] = {
    /* N  S  E  W  U  D */
    {  0, 0, 0, 0, 0, 0 }, /* 0 */
    {  2, 0, 0, 0, 0, 0 }, /* 1 */
    {  4, 1, 3, 5, 0, 0 }, /* 2 */
    {  0, 0, 6, 2, 0, 0 }, /* 3 */
    {  7, 2, 0, 0, 0, 0 }, /* 4 */
    {  0, 0, 2, 9, 0, 0 }, /* 5 */
    {  0, 0, 0, 3, 0, 0 }, /* 6 */
    {  0, 4, 0, 0, 8, 0 }, /* 7 */
    {  0, 0, 0, 0, 0, 7 }, /* 8 */
    {  0,10, 5, 0, 0,19 }, /* 9 */
    {  9, 0, 0,11, 0, 0 }, /* 10 */
    {  0, 0,10,12,14, 0 }, /* 11 */
    { 13, 0,11, 0, 0, 0 }, /* 12 */
    {  0,12, 0, 0, 0, 0 }, /* 13 */
    { 16, 0,15,17, 0,11 }, /* 14 */
    {  0, 0, 0,14, 0, 0 }, /* 15 */
    {  0,14, 0, 0, 0, 0 }, /* 16 */
    {  0, 0,14, 0, 0, 0 }, /* 17 */
    {  0, 0, 0, 0, 0,13 }, /* 18 */
    {  0, 0, 0,20, 9, 0 }, /* 19 */
    { 21, 0,19, 0, 0, 0 }, /* 20 */
    {  0,20, 0,22, 0, 0 }, /* 21 */
    {  0, 0,21, 0, 0, 0 }, /* 22 */
    { 24,21, 0, 0, 0, 0 }, /* 23 */
    { 29,23, 0,26, 0, 0 }, /* 24 */
    { 26, 0,24, 0, 0, 0 }, /* 25 */
    { 27,25,29, 0, 0, 0 }, /* 26 */
    {  0,26,28, 0, 0, 0 }, /* 27 */
    {  0,29,31,27, 0, 0 }, /* 28 */
    { 28,24,30,26, 0, 0 }, /* 29 */
    { 31, 0, 0,29, 0, 0 }, /* 30 */
    {  0,30, 0,29, 0, 0 }, /* 31 */
};

/* Current location */
int currentLocation;

/* Number of turns played in game */
int turnsPlayed;

/* True if player has lit the lamp. */
int lampLit;

/* True if lamp filled with oil. */
int lampFilled;

/* True if player ate food. */
int ateFood;

/* True if player drank water. */
int drankWater;

/* Incremented each turn you are in the tunnel. */
int ratAttack;

/* Tracks state of wolf attack */
int wolfState;

/* Set when game is over */
int gameOver;

const char *introText =
	"     Abandoned Farmhouse Adventure\n"
	"           By Jeff Tranter\n"
	"\n"
	"Your three-year-old grandson has gone\n"
	"missing and was last seen headed in the\n"
	"direction of the abandoned family farm.\n"
	"It's a dangerous place to play. You\n"
	"have to find him before he gets hurt,\n"
	"and it will be getting dark soon...\n";

const char *helpString = "Valid commands:\n"
	"go east/west/north/south/up/down \n"
	"look\n"
	"use <object>\n"
	"examine <object>\n"
	"take <object>\n"
	"drop <object>\n"
	"inventory\n"
	"help\n"
	"You can abbreviate commands and\n"
	"directions to the first letter.\n"
	"Type just the first letter of\n"
	"a direction to move.\n";

/* Line of user input */
char buffer[40];

/* Clear the screen */
static void clearScreen(void)
{
    int i;

    for (i = 0; i < 24; ++i)
        usb_printf("\n");
}

/* Return 1 if carrying an item */
static int carryingItem(char *item)
{
    int i;

    for (i = 0; i < MAXITEMS; i++) {
        if ((Inventory[i] != 0) && (!strcasecmp(DescriptionOfItem[Inventory[i]], item)))
            return 1;
    }
    return 0;
}

/* Return 1 if item it at current location (not carried) */
static int itemIsHere(char *item)
{
    int i;

    /* Find number of the item. */
    for (i = 1; i <= LastItem; i++) {
        if (!strcasecmp(item, DescriptionOfItem[i])) {
            /* Found it, but is it here? */
            if (locationOfItem[i] == currentLocation) {
                return 1;
            } else {
                return 0;
            }
        }
    }
    return 0;
}

/* Inventory command */
static void doInventory(void)
{
    int i;
    int found = 0;

    usb_printf("%s", "You are carrying:\n");
    for (i = 0; i < MAXITEMS; i++) {
        if (Inventory[i] != 0) {
            usb_printf("  %s\n", DescriptionOfItem[Inventory[i]]);
            found = 1;
        }
    }
    if (!found)
        usb_printf("  nothing\n");
}

/* Help command */
static void doHelp(void)
{
    usb_printf("%s", helpString);
}

/* Look command */
static void doLook(void)
{
    int i, loc, seen;

    usb_printf("You are %s.\n", DescriptionOfLocation[currentLocation]);

    seen = 0;
    usb_printf("You see:\n");
    for (i = 1; i <= LastItem; i++) {
        if (locationOfItem[i] == currentLocation) {
            usb_printf("  %s\n", DescriptionOfItem[i]);
            seen = 1;
        }
    }
    if (!seen)
        usb_printf("  nothing special\n");

    usb_printf("You can go:");

    for (i = North; i <= Down; i++) {
        loc = Move[currentLocation][i];
        if (loc != 0) {
            usb_printf(" %s", DescriptionOfDirection[i]);
        }
    }
    usb_printf("\n");
}

/* Drop command */
static void doDrop(void)
{
    int i;
    char *sp;
    char *item;

    /* Command line should be like "D[ROP] ITEM" Item name will be after after first space. */
    sp = strchr(buffer, ' ');
    if (sp == NULL) {
        usb_printf("Drop what?\n");
        return;
    }

    item = sp + 1;

    /* See if we have this item */
    for (i = 0; i < MAXITEMS; i++) {
        if ((Inventory[i] != 0) && (!strcasecmp(DescriptionOfItem[Inventory[i]], item))) {
            /* We have it. Add to location. */
	    if ( Inventory[i] == Lamp )
	    	set_lamp(Drop);

            locationOfItem[Inventory[i]] = currentLocation;
            /* And remove from inventory */
            Inventory[i] = 0;
            usb_printf("Dropped %s.\n", item);
            ++turnsPlayed;

            return;
        }
    }
    /* If here, don't have it. */
    usb_printf("Not carrying %s.\n", item);
}

/* Take command */
static void doTake(void)
{
    int i, j;
    char *sp;
    char *item;

    /* Command line should be like "T[AKE] ITEM" Item name will be after after first space. */
    sp = strchr(buffer, ' ');
    if (sp == NULL) {
        usb_printf("Take what?\n");
        return;
    }

    item = sp + 1;

    if (carryingItem(item)) {
        usb_printf("Already carrying it.\n");
        return;
    }

    /* Find number of the item. */
    for (i = 1; i <= LastItem; i++) {
        if (!strcasecmp(item, DescriptionOfItem[i])) {
            /* Found it, but is it here? */
            if (locationOfItem[i] == currentLocation) {
            /* It is here. Add to inventory. */
            for (j = 0; j < MAXITEMS; j++) {
                if (Inventory[j] == 0) {
                    Inventory[j] = i;
                    /* And remove from location. */
                    locationOfItem[i] = 0;
                    usb_printf("Took %s.\n", item);
                    ++turnsPlayed;

		    if ( i == Lamp )
		    	set_lamp(Take);

                    return;
                }
            }

            /* Reached maximum number of items to carry */ 
            usb_printf("You can't carry any more. Drop something.\n");
            return;
            }
        }
    }

    /* If here, don't see it. */
    usb_printf("I see no %s here.\n", item);
}

/* Go command */
static void doGo(void)
{
    char *sp;
    char dirChar;
    Direction_t dir;

    /* Command line should be like "G[O] N[ORTH]" Direction will be
       the first letter after a space. Or just a single letter
       direction N S E W U D or full directon NORTH etc. */

    sp = strrchr(buffer, ' ');
    if (sp != NULL) {
        dirChar = *(sp+1);
    } else {
        dirChar = buffer[0];
    }
    dirChar = tolower(dirChar);

    if (dirChar == 'n') {
        dir = North;
    } else if (dirChar == 's') {
        dir = South;
    } else if (dirChar == 'e') {
        dir = East;
    } else if (dirChar == 'w') {
        dir = West;
    } else if (dirChar == 'u') {
        dir = Up;
    } else if (dirChar == 'd') {
        dir = Down;
    } else {
        usb_printf("Go where?\n");
        return;
    }

    if (Move[currentLocation][dir] == 0) {
        usb_printf("You can't go %s from here.\n", DescriptionOfDirection[dir]);
        return;
    }

    /* We can move */
    currentLocation = Move[currentLocation][dir];
    usb_printf("You are %s.\n", DescriptionOfLocation[currentLocation]);
    ++turnsPlayed;
}

/* Examine command */
static void doExamine(void)
{
    char *sp;
    char *item;

    /* Command line should be like "E[XAMINE] ITEM" Item name will be after after first space. */
    sp = strchr(buffer, ' ');
    if (sp == NULL) {
        usb_printf("Examine what?\n");
        return;
    }

    item = sp + 1;
    ++turnsPlayed;

    /* Examine bookcase - not an object */
    if (!strcasecmp(item, "bookcase")) {
        usb_printf("You pull back a book and the bookcase\nopens up to reveal a secret room.\n");
        Move[17][North] = 18;
        return;
    }

    /* Make sure item is being carried or is in the current location */
    if (!carryingItem(item) && !itemIsHere(item)) {
        usb_printf("I don't see it here.\n");
        return;
    }

    /* Examine Book */
    if (!strcasecmp(item, "book")) {
        usb_printf("It is a very old book entitled\n\"Apple 1 operation manual\".\n");
        return;
    }

    /* Examine Flashlight */
    if (!strcasecmp(item, "flashlight")) {
        usb_printf("It doesn't have any batteries.\n");
        return;
    }

    /* Examine toy car */
    if (!strcasecmp(item, "toy car")) {
        usb_printf("It is a nice toy car.\nYour grandson Matthew would like it.\n");
        return;
    }

    /* Examine old radio */
    if (!strcasecmp(item, "old radio")) {
        usb_printf("It is a 1940 Zenith 8-S-563 console\nwith an 8A02 chassis. You'd turn it on\nbut the electricity is off.\n");
        return;
    }

   /* Nothing special about this item */
   usb_printf("You see nothing special about it.\n");
}

/* Use command */
static void doUse(void)
{
    char *sp;
    char *item;

    /* Command line should be like "U[SE] ITEM" Item name will be after after first space. */
    sp = strchr(buffer, ' ');
    if (sp == NULL) {
        usb_printf("Use what?\n");
        return;
    }

    item = sp + 1;

    /* Make sure item is being carried or is in the current location */
    if (!carryingItem(item) && !itemIsHere(item)) {
        usb_printf("I don't see it here.\n");
        return;
    }

    ++turnsPlayed;

    /* Use key */
    if (!strcasecmp(item, "key") && (currentLocation == VacantRoom)) {
        usb_printf("You insert the key in the door and it\nopens, revealing a tunnel.\n");
        Move[21][North] = 23;
        return;
    }

    /* Use pitchfork */
    if (!strcasecmp(item, "pitchfork") && (currentLocation == WolfTree) && (wolfState == 0)) {
        usb_printf("You jab the wolf with the pitchfork.\nIt howls and runs away.\n");
        wolfState = 1;
        return;
    }

    /* Use toy car */
    if (!strcasecmp(item, "toy car") && (currentLocation == WolfTree && wolfState == 1)) {
        usb_printf("You show Matthew the toy car and he\ncomes down to take it. You take Matthew\nin your arms and carry him home.\n");
        wolfState = 2;
        return;
    }

    /* Use oil */
    if (!strcasecmp(item, "oil")) {
        if (carryingItem("lamp")) {
            usb_printf("You fill the lamp with oil.\n");
            lampFilled = 1;
	    set_lamp(Filled);
            return;
        } else {
            usb_printf("You don't have anything to use it with.\n");
            return;
        }
    }

    /* Use matches */
    if (!strcasecmp(item, "matches")) {
        if (carryingItem("lamp")) {
            if (lampFilled) {
                usb_printf("You light the lamp. You can see!\n");
                lampLit = 1;
                return;
            } else {
                usb_printf("You can't light the lamp. It needs oil.\n");
                return;
            }
        } else {
            usb_printf("Nothing here to light\n");
        }
    }
                
    /* Use candybar */
    if (!strcasecmp(item, "candybar")) {
        usb_printf("That hit the spot. You no longer feel\nhungry.\n");
        ateFood = 1;
        return;
    }

    /* Use bottle */
    if (!strcasecmp(item, "bottle")) {
        if (currentLocation == Cistern) {
            usb_printf("You fill the bottle with water from the\ncistern and take a drink. You no longer\nfeel thirsty.\n");
            drankWater = 1;
            return;
        } else {
            usb_printf("The bottle is empty. If only you had\nsome water to fill it!\n");
            return;
        }
    }

    /* Use stale meat */
    if (!strcasecmp(item, "stale meat")) {
        usb_printf("The meat looked and tasted bad. You\nfeel very sick and pass out.\n");
        gameOver = 1;
        return;
    }

    /* Default */
    usb_printf("Nothing happens\n");
}

/* Prompt user and get a line of input */
static void prompt(void)
{
    usb_printf("? ");        
    usb_getline(buffer,sizeof(buffer));
}

/* Do special things unrelated to command typed. */
static void doActions(void)
{
    if ((turnsPlayed == 10) && !lampLit) {
        usb_printf("It will be getting dark soon. You need\nsome kind of light or soon you won't\nbe able to see.\n");
    }

    if ((turnsPlayed >= 60) && (!lampLit || (!itemIsHere("lamp") && !carryingItem("lamp")))) {
        usb_printf("It is dark out and you have no light.\nYou stumble around for a while and\nthen fall, hit your head, and pass out.\n");
        gameOver = 1;
        return;
    }

    if ((turnsPlayed == 20) && !drankWater) {
        usb_printf("You are getting very thirsty.\nYou need to get a drink soon.\n");
    }

    if ((turnsPlayed == 30) && !ateFood) {
        usb_printf("You are getting very hungry.\nYou need to find something to eat.\n");
    }

    if ((turnsPlayed == 50) && !drankWater) {
        usb_printf("You pass out due to thirst.\n");
        gameOver = 1;
        return;
    }

    if ((turnsPlayed == 40) && !ateFood) {
        usb_printf("You pass out from hunger.\n");
        gameOver = 1;
        return;
    }

    if (currentLocation == Tunnel) {
        if (itemIsHere("cheese")) {
            usb_printf("The rats go after the cheese.\n");
        } else {
            if (ratAttack < 3) {
                usb_printf("The rats are coming towards you!\n");
                ++ratAttack;
            } else {
                usb_printf("The rats attack and you pass out.\n");
                gameOver = 1;
                return;
            }
        }
    }

    /* wolfState values:  0 - wolf attacking 1 - wolf gone, Matthew in tree. 2 - Matthew safe, you won. Game over. */
    if (currentLocation == WolfTree) {
        switch (wolfState) {
            case 0:
                usb_printf("A wolf is circling around the tree.\nMatthew is up in the tree. You have to\nsave him! If only you had some kind of\nweapon!\n");
                break;
            case 1:
                usb_printf("Matthew is afraid to come\ndown from the tree. If only you had\nsomething to coax him with.\n");
                break;
            case 2:
                usb_printf("Congratulations! You succeeded and won\nthe game. I hope you had as much fun\nplaying the game as I did creating it.\n- Jeff Tranter <tranter@pobox.com>\n");
                gameOver = 1;
                return;
                break;
            }
    }
}

/* Set variables to values for start of game */
static void initialize(void)
{
    currentLocation = Driveway1;
    lampFilled = 0;
    lampLit = 0;
    ateFood = 0;
    drankWater = 0;
    ratAttack = 0;
    wolfState = 0;
    turnsPlayed = 0;
    gameOver= 0;

    /* These doors can get changed during game and may need to be reset O*/
    Move[17][North] = 0;
    Move[21][North] = 0;

    /* Set inventory to default */
    memset(Inventory, 0, sizeof(Inventory[0])*MAXITEMS);
    Inventory[0] = Flashlight;

    /* Put items in their default locations */
    locationOfItem[0]  = 0;                /* NoItem */
    locationOfItem[1]  = Driveway1;        /* Key */
    locationOfItem[2]  = Hayloft;          /* Pitchfork */
    locationOfItem[3]  = 0;                /* Flashlight */
    locationOfItem[4]  = WorkRoom;         /* Lamp */
    locationOfItem[5]  = Garage;           /* Oil */
    locationOfItem[6]  = Kitchen;          /* Candybar */
    locationOfItem[7]  = Driveway2;        /* Bottle */
    locationOfItem[8]  = GirlsBedroom;     /* Doll */
    locationOfItem[9]  = BoysBedroom;      /* ToyCar */
    locationOfItem[10] = ServantsQuarters; /* Matches */
    locationOfItem[11] = Woods25;          /* GoldCoin */
    locationOfItem[12] = Woods29;          /* SilverCoin */
    locationOfItem[13] = DiningRoom;       /* StaleMeat */
    locationOfItem[14] = DrawingRoom;      /* Book */
    locationOfItem[15] = LaundryRoom;      /* Cheese */
    locationOfItem[16] = MasterBedroom;    /* OldRadio */
}

/* Main program (obviously) */
void adventure(void *arg __attribute__((unused)))
{

    usb_getc();     // Wait for user to start

    while (1) {
        initialize();
        clearScreen();
        usb_printf("%s", introText);

        while (!gameOver) {
            prompt();
            if (buffer[0] == '\0') {
            } else if (tolower(buffer[0]) == 'h') {
                doHelp();
            } else if (tolower(buffer[0]) == 'i') {
                doInventory();
            } else if ((tolower(buffer[0]) == 'g')
                       || !strcasecmp(buffer, "n") || !strcasecmp(buffer, "s")
                       || !strcasecmp(buffer, "e") || !strcasecmp(buffer, "w")
                       || !strcasecmp(buffer, "u") || !strcasecmp(buffer, "d")
                       || !strcasecmp(buffer, "north") || !strcasecmp(buffer, "south")
                       || !strcasecmp(buffer, "east") || !strcasecmp(buffer, "west")
                       || !strcasecmp(buffer, "up") || !strcasecmp(buffer, "down")) {
                doGo();
            } else if (tolower(buffer[0]) == 'l') {
                doLook();
            } else if (tolower(buffer[0]) == 't') {
                doTake();
            } else if (tolower(buffer[0]) == 'e') {
                doExamine();
            } else if (tolower(buffer[0]) == 'u') {
                doUse();
            } else if (tolower(buffer[0]) == 'd') {
                doDrop();
            } else if (!strcasecmp(buffer, "xyzzy")) {
                usb_printf("Nice try, but that won't work here.\n");
            } else {
                usb_printf("I don't understand. Try 'help'.\n");
            }

            /* Handle special actions. */
            doActions();
        }
    }
}
