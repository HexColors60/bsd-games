// Copyright (c) 1985, Stichting Centrum voor Wiskunde en Informatica,
// Copyright (c) 1982 Jay Fenlason <hack@gnu.org>
// This file is free software, distributed under the BSD license.
// This file collects some Unix dependencies; hack.pager.c contains some more

//
// The time is used for:
//      - seed for rand()
//      - year on tombstone and yymmdd in record file
//      - phase of the moon (various monsters react to NEW_MOON or FULL_MOON)
//      - night and midnight (the undead are dangerous at midnight)
//      - determination of what files are "very old"

#include "hack.h"	       // mainly for strchr() which depends on BSD
#include "extern.h"
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

extern int locknum;

void setrandom(void)
{
    srand (time(NULL)+getpid());
}

struct tm *getlt(void)
{
    time_t date;
    time (&date);
    return localtime(&date);
}

int getyear(void)
{
    return 1900 + getlt()->tm_year;
}

const char* getdatestr (void)
{
    static char datestr[7];
    struct tm *lt = getlt();
    sprintf(datestr, "%02d%02d%02d", lt->tm_year % 100, lt->tm_mon + 1, lt->tm_mday);
    return datestr;
}

int phase_of_the_moon(void)
{
    // 0-7, with 0: new, 4: full moon period: 29.5306 days. year: 365.2422 days
    struct tm *lt = getlt();
    int epact, diy, golden;

    diy = lt->tm_yday;
    golden = (lt->tm_year % 19) + 1;
    epact = (11 * golden + 18) % 30;
    if ((epact == 25 && golden > 11) || epact == 24)
	epact++;

    return (((((diy + epact) * 6) + 11) % 177) / 22) & 7;
}

int night(void)
{
    int hour = getlt()->tm_hour;

    return hour < 6 || hour > 21;
}

int midnight(void)
{
    return getlt()->tm_hour == 0;
}

struct stat buf, hbuf;

void gethdate(char *name)
{
#if 0
    // old version - for people short of space

    char *np;

    if (stat(name, &hbuf))
	error("Cannot get status of %s.", (np = strrchr(name, '/')) ? np + 1 : name);
#else
    // version using PATH from: seismo!gregc@ucsf-cgl.ARPA (Greg Couch)

    // The problem with   #include  <sys/param.h>   is that this include file
    // does not exist on all systems, and moreover, that it sometimes includes
    // <sys/types.h> again, so that the compiler sees these typedefs twice.
#define		MAXPATHLEN	1024

    const char *np, *path;
    char filename[MAXPATHLEN + 1];
    if (strchr(name, '/') != NULL || (path = getenv("PATH")) == NULL)
	path = "";

    for (;;) {
	if ((np = strchr(path, ':')) == NULL)
	    np = path + strlen(path);	// point to end str
	if (np - path <= 1)    // %%
	    (void) strcpy(filename, name);
	else {
	    (void) strncpy(filename, path, np - path);
	    filename[np - path] = '/';
	    (void) strcpy(filename + (np - path) + 1, name);
	}
	if (stat(filename, &hbuf) == 0)
	    return;
	if (*np == '\0')
	    break;
	path = np + 1;
    }
    error("Cannot get status of %s.", (np = strrchr(name, '/')) ? np + 1 : name);
#endif
}

int uptodate(int fd)
{
    if (fstat(fd, &buf)) {
	pline("Cannot get status of saved level? ");
	return 0;
    }
    if (buf.st_mtime < hbuf.st_mtime) {
	pline("Saved level is out of date. ");
	return 0;
    }
    return 1;
}

// see whether we should throw away this xlock file
int veryold(int fd)
{
    int i;
    time_t date;

    if (fstat(fd, &buf))
	return 0;	       // cannot get status
    if (buf.st_size != sizeof(int))
	return 0;	       // not an xlock file
    (void) time(&date);
    if (date - buf.st_mtime < 3L * 24L * 60L * 60L) {	// recent
	int lockedpid;		// should be the same size as
				// hackpid

	if (read(fd, (char *) &lockedpid, sizeof(lockedpid)) != sizeof(lockedpid))
	    // strange ...
	    return 0;

	// From: Rick Adams <seismo!rick> This will work on
	// 4.1cbsd, 4.2bsd and system 3? & 5. It will do nothing
	// on V7 or 4.1bsd.
	if (!(kill(lockedpid, 0) == -1 && errno == ESRCH))
	    return 0;
    }
    (void) close(fd);
    for (i = 1; i <= MAXLEVEL; i++) {	// try to remove all
	glo(i);
	(void) unlink(lock);
    }
    glo(0);
    if (unlink(lock))
	return 0;	       // cannot remove it
    return 1;		       // success!
}

void getlock(void)
{
    int i = 0, fd;

    (void) fflush(stdout);

    // we ignore QUIT and INT at this point
    if (link(HLOCK, LLOCK) == -1) {
	int errnosv = errno;

	perror(HLOCK);
	printf("Cannot link %s to %s\n", LLOCK, HLOCK);
	switch (errnosv) {
	    case ENOENT:
		printf("Perhaps there is no (empty) file %s ?\n", HLOCK);
		break;
	    case EACCES:
		printf("It seems you don't have write permission here.\n");
		break;
	    case EEXIST:
		printf("(Try again or rm %s.)\n", LLOCK);
		break;
	    default:
		printf("I don't know what is wrong.");
	}
	getret();
	error("%s", "");
	// NOTREACHED
    }
    regularize(lock);
    glo(0);
    if (locknum > 25)
	locknum = 25;

    do {
	if (locknum)
	    lock[0] = 'a' + i++;

	if ((fd = open(lock, O_RDONLY)) == -1) {
	    if (errno == ENOENT)
		goto gotlock;  // no such file
	    perror(lock);
	    (void) unlink(LLOCK);
	    error("Cannot open %s", lock);
	}
	if (veryold(fd))       // if true, this closes fd and unlinks lock
	    goto gotlock;
	(void) close(fd);
    } while (i < locknum);

    (void) unlink(LLOCK);
    error(locknum ? "Too many hacks running now." : "There is a game in progress under your name.");
  gotlock:
    fd = creat(lock, FMASK);
    if (unlink(LLOCK) == -1)
	error("Cannot unlink %s.", LLOCK);
    if (fd == -1) {
	error("cannot creat lock file.");
    } else {
	if (write(fd, (char *) &hackpid, sizeof(hackpid))
	    != sizeof(hackpid)) {
	    error("cannot write lock");
	}
	if (close(fd) == -1) {
	    error("cannot close lock");
	}
    }
}

#ifdef MAIL

//
// Notify user when new mail has arrived. [Idea from Merlyn Leroy, but
// I don't know the details of his implementation.]
// { Later note: he disliked my calling a general mailreader and felt that
//   hack should do the paging itself. But when I get mail, I want to put it
//   in some folder, reply, etc. - it would be unreasonable to put all these
//   functions in hack. }
// The mail daemon '2' is at present not a real monster, but only a visual
// effect. Thus, makemon() is superfluous. This might become otherwise,
// however. The motion of '2' is less restrained than usual: diagonal moves
// from a DOOR are possible. He might also use SDOOR's. Also, '2' is visible
// in a ROOM, even when you are Blind.
// Its path should be longer when you are Telepat-hic and Blind.
//
// Interesting side effects:
//      - You can get rich by sending yourself a lot of mail and selling
//        it to the shopkeeper. Unfortunately mail isn't very valuable.
//      - You might die in case '2' comes along at a critical moment during
//        a fight and delivers a scroll the weight of which causes you to
//        collapse.
//
// Possible extensions:
//      - Open the file MAIL and do fstat instead of stat for efficiency.
//        (But sh uses stat, so this cannot be too bad.)
//      - Examine the mail and produce a scroll of mail called "From somebody".
//      - Invoke MAILREADER in such a way that only this single letter is read.
//
//      - Make him lose his mail when a Nymph steals the letter.
//      - Do something to the text when the scroll is enchanted or cancelled.
#include	"mkroom.h"
static struct stat omstat, nmstat;
static char *mailbox;
static long laststattime;

void getmailstatus(void)
{
    if (!(mailbox = getenv("MAIL")))
	return;
    if (stat(mailbox, &omstat)) {
#ifdef PERMANENT_MAILBOX
	pline("Cannot get status of MAIL=%s .", mailbox);
	mailbox = 0;
#else
	omstat.st_mtime = 0;
#endif				// PERMANENT_MAILBOX
    }
}

void ckmailstatus(void)
{
    if (!mailbox
#ifdef MAILCKFREQ
	|| moves < laststattime + MAILCKFREQ
#endif				// MAILCKFREQ
	)
	return;
    laststattime = moves;
    if (stat(mailbox, &nmstat)) {
#ifdef PERMANENT_MAILBOX
	pline("Cannot get status of MAIL=%s anymore.", mailbox);
	mailbox = 0;
#else
	nmstat.st_mtime = 0;
#endif				// PERMANENT_MAILBOX
    } else if (nmstat.st_mtime > omstat.st_mtime) {
	if (nmstat.st_size)
	    newmail();
	getmailstatus();       // might be too late ...
    }
}

void newmail(void)
{
    // produce a scroll of mail
    struct obj *obj;
    struct monst *md;

    obj = mksobj(SCR_MAIL);
    if (md = makemon(&pm_mail_daemon, u.ux, u.uy))	// always succeeds
	mdrush(md, 0);

    pline("\"Hello, %s! I have some mail for you.\"", plname);
    if (md) {
	if (dist(md->mx, md->my) > 2)
	    pline("\"Catch!\"");
	more();

	// let him disappear again
	mdrush(md, 1);
	mondead(md);
    }
    obj = addinv(obj);
    (void) identify(obj);      // set known and do prinv()
}

// make md run through the cave
void mdrush(struct monst *md, int away)
{
    int uroom = inroom(u.ux, u.uy);
    if (uroom >= 0) {
	int tmp = rooms[uroom].fdoor;
	int cnt = rooms[uroom].doorct;
	int fx = u.ux, fy = u.uy;
	while (cnt--) {
	    if (dist(fx, fy) < dist(doors[tmp].x, doors[tmp].y)) {
		fx = doors[tmp].x;
		fy = doors[tmp].y;
	    }
	    tmp++;
	}
	tmp_at(-1, md->data->mlet);	// open call
	if (away) {	       // interchange origin and destination
	    unpmon(md);
	    tmp = fx;
	    fx = md->mx;
	    md->mx = tmp;
	    tmp = fy;
	    fy = md->my;
	    md->my = tmp;
	}
	while (fx != md->mx || fy != md->my) {
	    int dx, dy, nfx = fx, nfy = fy, d1, d2;

	    tmp_at(fx, fy);
	    d1 = DIST(fx, fy, md->mx, md->my);
	    for (dx = -1; dx <= 1; dx++)
		for (dy = -1; dy <= 1; dy++)
		    if (dx || dy) {
			d2 = DIST(fx + dx, fy + dy, md->mx, md->my);
			if (d2 < d1) {
			    d1 = d2;
			    nfx = fx + dx;
			    nfy = fy + dy;
			}
		    }
	    if (nfx != fx || nfy != fy) {
		fx = nfx;
		fy = nfy;
	    } else {
		if (!away) {
		    md->mx = fx;
		    md->my = fy;
		}
		break;
	    }
	}
	tmp_at(-1, -1);	       // close call
    }
    if (!away)
	pmon(md);
}

void readmail(void)
{
#ifdef DEF_MAILREADER	       // This implies that UNIX is defined
    char *mr = 0;
    more();
    if (!(mr = getenv("MAILREADER")))
	mr = DEF_MAILREADER;
    if (child(1)) {
	execl(mr, mr, (char *) 0);
	exit(1);
    }
#else				// DEF_MAILREADER
    (void) page_file(mailbox, false);
#endif				// DEF_MAILREADER
    // get new stat; not entirely correct: there is a small time window
    // where we do not see new mail
    getmailstatus();
}
#endif				// MAIL

// normalize file name - we don't like ..'s or /'s
void regularize (char *s)
{
    char *lp;
    while ((lp = strchr(s, '.')) || (lp = strchr(s, '/')))
	*lp = '_';
}