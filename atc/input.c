// Copyright (c) 1987 by Ed James <edjames@berkeley.edu>
// This file is free software, distributed under the BSD license.

#include "include.h"

#define MAXRULES	6
#define MAXDEPTH	15

#define RETTOKEN	'\n'
#define REDRAWTOKEN	'\014' // CTRL(L)
#define	SHELLTOKEN	'!'
#define HELPTOKEN	'?'
#define ALPHATOKEN	256
#define NUMTOKEN	257

typedef struct {
    int token;
    int to_state;
    const char *str;
    const char *(*func) (char);
} RULE;

typedef struct {
    int num_rules;
    RULE *rule;
} STATE;

typedef struct {
    char str[20];
    int state;
    int rule;
    int ch;
    int pos;
} STACK;

#define T_RULE		stack[level].rule
#define T_STATE		stack[level].state
#define T_STR		stack[level].str
#define T_POS		stack[level].pos
#define	T_CH		stack[level].ch

#define NUMELS(a)	(sizeof (a) / sizeof (*(a)))

#define NUMSTATES	NUMELS(st)

RULE
state0[] = {	{ ALPHATOKEN,	1,	"%c:",		setplane},
		{ RETTOKEN,	-1,	"",		NULL    },
		{ HELPTOKEN,	12,	" [a-z]<ret>",	NULL    }},
state1[] = {	{ 't',		2,	" turn",	turn    },
		{ 'a',		3,	" altitude:",	NULL    },
		{ 'c',		4,	" circle",	circle  },
		{ 'm',		7,	" mark",	mark    },
		{ 'u',		7,	" unmark",	unmark  },
		{ 'i',		7,	" ignore",	ignore  },
		{ HELPTOKEN,	12,	" tacmui",	NULL    }},
state2[] = {	{ 'l',		6,	" left",	left    },
		{ 'r',		6,	" right",	right   },
		{ 'L',		4,	" left 90",	Left    },
		{ 'R',		4,	" right 90",	Right   },
		{ 't',		11,	" towards",	NULL    },
		{ 'w',		4,	" to 0",	to_dir  },
		{ 'e',		4,	" to 45",	to_dir  },
		{ 'd',		4,	" to 90",	to_dir  },
		{ 'c',		4,	" to 135",	to_dir  },
		{ 'x',		4,	" to 180",	to_dir  },
		{ 'z',		4,	" to 225",	to_dir  },
		{ 'a',		4,	" to 270",	to_dir  },
		{ 'q',		4,	" to 315",	to_dir  },
		{ HELPTOKEN,	12,	" lrLRt<dir>",	NULL    }},
state3[] = {	{ '+',		10,	" climb",	climb   },
		{ 'c',		10,	" climb",	climb   },
		{ '-',		10,	" descend",	descend },
		{ 'd',		10,	" descend",	descend },
		{ NUMTOKEN,	7,	" %c000 feet",	setalt  },
		{ HELPTOKEN,	12,	" +-cd[0-9]",	NULL    }},
state4[] = {	{ '@',		9,	" at",		NULL    },
		{ 'a',		9,	" at",		NULL    },
		{ RETTOKEN,	-1,	"",		NULL    },
		{ HELPTOKEN,	12,	" @a<ret>",	NULL    }},
state5[] = {	{ NUMTOKEN,	7,	"%c",		delayb  },
		{ HELPTOKEN,	12,	" [0-9]",	NULL    }},
state6[] = {	{ '@',		9,	" at",		NULL    },
		{ 'a',		9,	" at",		NULL    },
		{ 'w',		4,	" 0",		rel_dir },
		{ 'e',		4,	" 45",		rel_dir },
		{ 'd',		4,	" 90",		rel_dir },
		{ 'c',		4,	" 135",		rel_dir },
		{ 'x',		4,	" 180",		rel_dir },
		{ 'z',		4,	" 225",		rel_dir },
		{ 'a',		4,	" 270",		rel_dir },
		{ 'q',		4,	" 315",		rel_dir },
		{ RETTOKEN,	-1,	"",		NULL    },
		{ HELPTOKEN,	12,	" @a<dir><ret>",NULL    }},
state7[] = {	{ RETTOKEN,	-1,	"",		NULL    },
		{ HELPTOKEN,	12,	" <ret>",	NULL    }},
state8[] = {	{ NUMTOKEN,	4,	"%c",		benum   },
		{ HELPTOKEN,	12,	" [0-9]",	NULL    }},
state9[] = {	{ 'b',		5,	" beacon #",	NULL    },
		{ '*',		5,	" beacon #",	NULL    },
		{ HELPTOKEN,	12,	" b*",		NULL    }},
state10[] = {	{ NUMTOKEN,	7,	" %c000 ft",	setrelalt},
		{ HELPTOKEN,	12,	" [0-9]",	NULL    }},
state11[] = {	{ 'b',		8,	" beacon #",	beacon  },
		{ '*',		8,	" beacon #",	beacon  },
		{ 'e',		8,	" exit #",	ex_it   },
		{ 'a',		8,	" airport #",	airport },
		{ HELPTOKEN,	12,	" b*ea",	NULL    }},
state12[] = {	{ -1,		-1,	"",		NULL    }};

#define DEF_STATE(s)	{ NUMELS(s),	(s)	}

STATE st[] = {
    DEF_STATE(state0), DEF_STATE(state1), DEF_STATE(state2),
    DEF_STATE(state3), DEF_STATE(state4), DEF_STATE(state5),
    DEF_STATE(state6), DEF_STATE(state7), DEF_STATE(state8),
    DEF_STATE(state9), DEF_STATE(state10), DEF_STATE(state11),
    DEF_STATE(state12)
};

PLANE p;
STACK stack[MAXDEPTH];
int level;
int tval;
int dest_type, dest_no, dir;

int pop(void)
{
    if (level == 0)
	return -1;
    level--;

    ioclrtoeol(T_POS);

    strcpy(T_STR, "");
    T_RULE = -1;
    T_CH = -1;
    return 0;
}

void rezero(void)
{
    iomove(0);

    level = 0;
    T_STATE = 0;
    T_RULE = -1;
    T_CH = -1;
    T_POS = 0;
    strcpy(T_STR, "");
}

void push(int ruleno, int ch)
{
    int newstate, newpos;

    sprintf(T_STR, st[T_STATE].rule[ruleno].str, tval);
    T_RULE = ruleno;
    T_CH = ch;
    newstate = st[T_STATE].rule[ruleno].to_state;
    newpos = T_POS + strlen(T_STR);

    ioaddstr(T_POS, T_STR);

    if (level == 0)
	ioclrtobot();
    level++;
    T_STATE = newstate;
    T_POS = newpos;
    T_RULE = -1;
    strcpy(T_STR, "");
}

int getcommand(void)
{
    int c, i, done;
    const char *s, *(*func) (char);
    PLANE *pp;

    rezero();

    do {
	c = gettoken();
	if (c == tty_new.c_cc[VERASE]) {
	    if (pop() < 0)
		noise();
	} else if (c == tty_new.c_cc[VKILL]) {
	    while (pop() >= 0) {}
	} else {
	    done = 0;
	    for (i = 0; i < st[T_STATE].num_rules; i++) {
		if (st[T_STATE].rule[i].token == c || st[T_STATE].rule[i].token == tval) {
		    push(i, (c >= ALPHATOKEN) ? tval : c);
		    done = 1;
		    break;
		}
	    }
	    if (!done)
		noise();
	}
    } while (T_STATE != -1);

    if (level == 1)
	return 1;	       // forced update

    dest_type = T_NODEST;

    for (i = 0; i < level; i++) {
	func = st[stack[i].state].rule[stack[i].rule].func;
	if (func != NULL) {
	    if ((s = (*func) (stack[i].ch)) != NULL) {
		ioerror(stack[i].pos, strlen(stack[i].str), s);
		return -1;
	    }
	}
    }

    pp = findplane(p.plane_no);
    if (pp->new_altitude != p.new_altitude)
	pp->new_altitude = p.new_altitude;
    else if (pp->status != p.status)
	pp->status = p.status;
    else {
	pp->new_dir = p.new_dir;
	pp->delayd = p.delayd;
	pp->delayd_no = p.delayd_no;
    }
    return 0;
}

void noise(void)
{
    putchar('\07');
    fflush(stdout);
}

int gettoken(void)
{
    while ((tval = getAChar()) == REDRAWTOKEN || tval == SHELLTOKEN) {
	if (tval == SHELLTOKEN) {
#ifdef BSD
	    struct itimerval itv;
	    itv.it_value.tv_sec = 0;
	    itv.it_value.tv_usec = 0;
	    setitimer(ITIMER_REAL, &itv, NULL);
#endif
#ifdef SYSV
	    int aval;
	    aval = alarm(0);
#endif
	    if (fork() == 0)   // child
	    {
		char *shell, *base;

		done_screen();

		// run user's favorite shell
		if ((shell = getenv("SHELL")) != NULL) {
		    base = strrchr(shell, '/');
		    if (base == NULL)
			base = shell;
		    else
			base++;
		    execl(shell, base, (char *) 0);
		} else
		    execl(_PATH_BSHELL, "sh", (char *) 0);

		exit(0);       // oops
	    }

	    wait(0);
	    tcsetattr(fileno(stdin), TCSADRAIN, &tty_new);
#ifdef BSD
	    itv.it_value.tv_sec = 0;
	    itv.it_value.tv_usec = 1;
	    itv.it_interval.tv_sec = sp->update_secs;
	    itv.it_interval.tv_usec = 0;
	    setitimer(ITIMER_REAL, &itv, NULL);
#endif
#ifdef SYSV
	    alarm(aval);
#endif
	}
	redraw();
    }

    if (isdigit(tval))
	return NUMTOKEN;
    else if (isalpha(tval))
	return ALPHATOKEN;
    else
	return tval;
}

const char *setplane(char c)
{
    PLANE *pp;

    pp = findplane(number(c));
    if (pp == NULL)
	return "Unknown Plane";
    memcpy(&p, pp, sizeof(p));
    p.delayd = 0;
    return NULL;
}

const char *turn(char c UNUSED)
{
    if (p.altitude == 0)
	return "Planes at airports may not change direction";
    return NULL;
}

const char *circle(char c UNUSED)
{
    if (p.altitude == 0)
	return "Planes cannot circle on the ground";
    p.new_dir = MAXDIR;
    return NULL;
}

const char *left(char c UNUSED)
{
    dir = D_LEFT;
    p.new_dir = p.dir - 1;
    if (p.new_dir < 0)
	p.new_dir += MAXDIR;
    return NULL;
}

const char *right(char c UNUSED)
{
    dir = D_RIGHT;
    p.new_dir = p.dir + 1;
    if (p.new_dir >= MAXDIR)
	p.new_dir -= MAXDIR;
    return NULL;
}

const char *Left(char c UNUSED)
{
    p.new_dir = p.dir - 2;
    if (p.new_dir < 0)
	p.new_dir += MAXDIR;
    return NULL;
}

const char *Right(char c UNUSED)
{
    p.new_dir = p.dir + 2;
    if (p.new_dir >= MAXDIR)
	p.new_dir -= MAXDIR;
    return NULL;
}

const char *delayb(char c)
{
    int xdiff, ydiff;

    c -= '0';

    if (c >= sp->num_beacons)
	return "Unknown beacon";
    xdiff = sp->beacon[(int) c].x - p.xpos;
    xdiff = SGN(xdiff);
    ydiff = sp->beacon[(int) c].y - p.ypos;
    ydiff = SGN(ydiff);
    if (xdiff != displacement[p.dir].dx || ydiff != displacement[p.dir].dy)
	return "Beacon is not in flight path";
    p.delayd = 1;
    p.delayd_no = c;

    if (dest_type != T_NODEST) {
	switch (dest_type) {
	    case T_BEACON:
		xdiff = sp->beacon[dest_no].x - sp->beacon[(int) c].x;
		ydiff = sp->beacon[dest_no].y - sp->beacon[(int) c].y;
		break;
	    case T_EXIT:
		xdiff = sp->exit[dest_no].x - sp->beacon[(int) c].x;
		ydiff = sp->exit[dest_no].y - sp->beacon[(int) c].y;
		break;
	    case T_AIRPORT:
		xdiff = sp->airport[dest_no].x - sp->beacon[(int) c].x;
		ydiff = sp->airport[dest_no].y - sp->beacon[(int) c].y;
		break;
	    default:
		return "Bad case in delayb!  Get help!";
		break;
	}
	if (xdiff == 0 && ydiff == 0)
	    return "Would already be there";
	p.new_dir = DIR_FROM_DXDY(xdiff, ydiff);
	if (p.new_dir == p.dir)
	    return "Already going in that direction";
    }
    return NULL;
}

const char *beacon(char c UNUSED)
{
    dest_type = T_BEACON;
    return NULL;
}

const char *ex_it(char c UNUSED)
{
    dest_type = T_EXIT;
    return NULL;
}

const char *airport(char c UNUSED)
{
    dest_type = T_AIRPORT;
    return NULL;
}

const char *climb(char c UNUSED)
{
    dir = D_UP;
    return NULL;
}

const char *descend(char c UNUSED)
{
    dir = D_DOWN;
    return NULL;
}

const char *setalt(char c)
{
    if ((p.altitude == c - '0') && (p.new_altitude == p.altitude))
	return "Already at that altitude";
    p.new_altitude = c - '0';
    return NULL;
}

const char *setrelalt(char c)
{
    if (c == 0)
	return "altitude not changed";

    switch (dir) {
	case D_UP:
	    p.new_altitude = p.altitude + c - '0';
	    break;
	case D_DOWN:
	    p.new_altitude = p.altitude - (c - '0');
	    break;
	default:
	    return "Unknown case in setrelalt!  Get help!";
	    break;
    }
    if (p.new_altitude < 0)
	return "Altitude would be too low";
    else if (p.new_altitude > 9)
	return "Altitude would be too high";
    return NULL;
}

const char *benum(char c)
{
    dest_no = c -= '0';

    switch (dest_type) {
	case T_BEACON:
	    if (c >= sp->num_beacons)
		return "Unknown beacon";
	    p.new_dir = DIR_FROM_DXDY(sp->beacon[(int) c].x - p.xpos, sp->beacon[(int) c].y - p.ypos);
	    break;
	case T_EXIT:
	    if (c >= sp->num_exits)
		return "Unknown exit";
	    p.new_dir = DIR_FROM_DXDY(sp->exit[(int) c].x - p.xpos, sp->exit[(int) c].y - p.ypos);
	    break;
	case T_AIRPORT:
	    if (c >= sp->num_airports)
		return "Unknown airport";
	    p.new_dir = DIR_FROM_DXDY(sp->airport[(int) c].x - p.xpos, sp->airport[(int) c].y - p.ypos);
	    break;
	default:
	    return "Unknown case in benum!  Get help!";
	    break;
    }
    return NULL;
}

const char *to_dir(char c)
{
    p.new_dir = dir_no(c);
    return NULL;
}

const char *rel_dir(char c)
{
    int angle;

    angle = dir_no(c);
    switch (dir) {
	case D_LEFT:
	    p.new_dir = p.dir - angle;
	    if (p.new_dir < 0)
		p.new_dir += MAXDIR;
	    break;
	case D_RIGHT:
	    p.new_dir = p.dir + angle;
	    if (p.new_dir >= MAXDIR)
		p.new_dir -= MAXDIR;
	    break;
	default:
	    return "Bizarre direction in rel_dir!  Get help!";
	    break;
    }
    return NULL;
}

const char *mark(char c UNUSED)
{
    if (p.altitude == 0)
	return "Cannot mark planes on the ground";
    if (p.status == S_MARKED)
	return "Already marked";
    p.status = S_MARKED;
    return NULL;
}

const char *unmark(char c UNUSED)
{
    if (p.altitude == 0)
	return "Cannot unmark planes on the ground";
    if (p.status == S_UNMARKED)
	return "Already unmarked";
    p.status = S_UNMARKED;
    return NULL;
}

const char *ignore(char c UNUSED)
{
    if (p.altitude == 0)
	return "Cannot ignore planes on the ground";
    if (p.status == S_IGNORED)
	return "Already ignored";
    p.status = S_IGNORED;
    return NULL;
}

int dir_no (char c)
{
    static const char c_DirChars [MAXDIR+1] = "wedcxzaq";
    const char* f = strchr (c_DirChars, c);
    assert (f && "bad character in dir_no");
    return f ? f - c_DirChars : -1;
}
