/*
 * ttyload
 *
 * tty equivelant to xload
 *
 * Copyright 1996 by David Lindes
 * all right reserved.
 *
 * Version information: $Id: ttyload.c,v 1.8 2001-02-24 10:00:51 lindes Exp $
 *
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <time.h>
#if 0
#include <sys/sysmp.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "ttyload.h"

#define	HEIGHTPAD	9
#define	WIDTHPAD	14
#define	CLOCKWIDTH	7
#define	HOSTLENGTH	20

#define	MINROWS		(HEIGHTPAD + 6)
#define	MINCOLS		(WIDTHPAD + 6)

char *c="$Id: ttyload.c,v 1.8 2001-02-24 10:00:51 lindes Exp $";

char		*kmemfile	= "/dev/kmem",
		strbuf[BUFSIZ],
		*optstring	= "i:",
		*usage	=
		    "Usage: %s [-i secs]\n"
		    "  -i secs\n"
		    "     Alter the number of seconds in "
		    "the interval between refreshes\n"
		    "     The default is 4, and the minimum "
		    "is 1, which is silently clamped.\n";
int		kmemfd, clockpad, clocks;
clock_info	*theclocks;

/* version ID, passed in by the Makefile: */
char		*version	= VERSION;

char *loadstrings[] = {
	" ",	/* blank */
	"\033[31m*\033[m",	/* one minute average */
	"\033[32m*\033[m",	/* five minute average */
	"\033[33m*\033[m",	/* one & five, together */
	"\033[34m*\033[m",	/* fifteen minute average */
	"\033[35m*\033[m",	/* one & fifteen, together */
	"\033[36m*\033[m",	/* five & fifteen, together */
	"\033[37m*\033[m"	/* one, five & fifteen, together */
    };

/* The following two variables should probably be assigned
   using some sort of real logic, rather than these hard-coded
   defaults, but the defaults work for now... */
int	rows		= 40,
	cols		= 80,

	intsecs		= 4,
	debug		= 3,
	theclock	= 0,

	height, width/* ,
	c, i, j, k */;


// void	getload(long, long, load_list *);
void	getload(load_list *);
int	compute_height(load_t, load_t, int);
void	showloads(load_list *);
void	clear_screen();
void	cycle_load_list(load_list*, load_list, int);

int main(argc, argv, envp)
    int		argc;
    char	*argv[],
		*envp[];
{
    float	multiplier;
    load_t	loadaddr;
    load_list	*loadavgs, newload;
    int		c, i, j, k, errflag=0;
    char	*basename;
    char	hostname[HOSTLENGTH + 1];
    time_t	thetime;
    struct tm	*thetimetm;

    while((c = getopt(argc, argv, optstring)) != EOF)
    {
	switch(c)
	{
	    case 'i':
		intsecs	= atoi(optarg);
		break;
	    default:
		errflag++;
		break;	/* redundant */
	}
    }
    if(errflag)
    {
	fprintf(stderr, usage, basename);
	exit(1);
    }

    if(gethostname(hostname, HOSTLENGTH))
    {
	perror("NOTICE: couldn't determine hostname");
	strcpy(hostname, "localhost");
	sleep(2);
    }

    /* do space-padding of hostname out to HOSTLENGTH chars: */
    for(i = 0; i < (HOSTLENGTH); i++)
    {
    	if(hostname[i] == '\0')
	{
	    hostname[i]		= ' ';
	    hostname[i + 1]	= '\0';
	}
    }

    basename		= (char *)strrchr(*argv, '/');
    if(!basename)
	basename	= *argv;

    intsecs	= MAX(1, intsecs);	/* must be positive */
    height	= rows - HEIGHTPAD;
    width	= cols - WIDTHPAD;
    clocks	= MAX(width/intsecs, width/CLOCKWIDTH);
    clockpad	= (width / clocks) - CLOCKWIDTH;

    if(rows < MINROWS)
    {
	fprintf(stderr, "Sorry, %s requires at least %d rows to run.\n",
		basename,
		MINROWS,
	    NULL);
	exit(1);
    }
    if(cols < MINCOLS)
    {
	fprintf(stderr, "Sorry, %s requires at least %d cols to run.\n",
		basename,
		MINCOLS,
	    NULL);
	exit(1);
    }

    loadavgs	= (load_list *)calloc(width, sizeof(load_list));
    theclocks	= (clock_info *)calloc(clocks, sizeof(clock_info));

    if(!loadavgs)
    {
	perror("calloc for loadavgs failed");
	exit(1);
    }

    if(!theclocks)
    {
	perror("calloc for clocks failed");
	exit(1);
    }

    for(i=0;i<clocks;i++)
    {
	theclocks[i].pos	= -1;
    }

#if 0	/* need to pull this */
    loadaddr	= sysmp(MP_KERNADDR, MPKA_AVENRUN);

    if(loadaddr == -1)
    {
	perror("Couldn't determine load address");
	exit(1);
    }

    kmemfd	= open(kmemfile, O_RDONLY);

    if(kmemfd < 0)
    {
	perror("Couldn't open memory file");
	exit(1);
    }
#endif	/* 0 */

    for(i=0;i<width;i++)
    {
	if(i != 0)
	{
	    sleep(intsecs);
	}

	time(&thetime);

	thetimetm	= localtime(&thetime);

	getload(&loadavgs[i]);

	if(((thetimetm->tm_sec) / intsecs) == 0)
	{
	    if(!ascftime(strbuf, "^%H:%M", thetimetm))
	    {
		/* This should never happen, I hope... */
		perror("ascftime failed");
		exit(1);
	    }
	    theclocks[theclock].pos	= i;
	    strcpy(theclocks[theclock].clock, strbuf);
	    theclock++;
	    theclock	%= clocks;

	    if(theclock >= clocks)
	    {
		/* Hopefully, I'll get this to the point
		   where it well never happen...  As I first
		   write it, I'm fairly certain it will, but
		   that should be fixable... */
		fprintf(stderr, "Internal error: too many clocks!");
		exit(1);
	    }
	}

	if(!ascftime(strbuf, "%T", thetimetm))
	{
	    /* This should never happen, I hope... */
	    perror("ascftime failed");
	    exit(1);
	}

	clear_screen();

	printf("%s   %.2f, %.2f, %.2f   %s       ttyload, v%s\n\n",
		hostname,
		(loadavgs[i].one_minute / 1024.),
		(loadavgs[i].five_minute / 1024.),
		(loadavgs[i].fifteen_minute / 1024.),
		strbuf + 1,
		version,
	    NULL);

	if(debug > 3)
	    printf("Load averages: %f, %f, %f\n",
		    loadavgs[i].one_minute / 1024.,
		    loadavgs[i].five_minute / 1024.,
		    loadavgs[i].fifteen_minute / 1024.,
		NULL);

	showloads(loadavgs);

	if(i == (width - 1))
	{
	    if(debug > 4)
	    {
		printf("CYCLING LOAD LIST...\n");
		sleep(3);
	    }
	    getload(&newload);
	    cycle_load_list(loadavgs, newload, width);
	    i--;
	}
    }
}

void	showloads(loadavgs)
    load_list	*loadavgs;
{
    load_list	min	= {LONG_MAX-1, LONG_MAX-1, LONG_MAX-1, 0, 0, 0},
		max	= {0, 0, 0, 0, 0, 0};
    load_t	lmin, lmax;
    float	omin, omax;
    int		i, j, k;

    if(debug>3)
    {
	printf("Starting with min set: %d, %d, %d\n",
		min.one_minute,
		min.five_minute,
		min.fifteen_minute,
	    NULL);
	printf("Starting with first set: %d, %d, %d\n",
		loadavgs[0].one_minute,
		loadavgs[0].five_minute,
		loadavgs[0].fifteen_minute,
	    NULL);
	sleep(1);
    }
    for(i=0;i<width;i++)
    {
	if(debug>9)
	{
	    printf("Checking for min/max at %d...\n", i);
	    printf("Comparing, for example, %d <=> %d\n",
		    min.one_minute, loadavgs[i].one_minute,
		NULL);
	}
	min.one_minute	= MIN(min.one_minute, loadavgs[i].one_minute);
	min.five_minute	= MIN(min.five_minute, loadavgs[i].five_minute);
	min.fifteen_minute
		= MIN(min.fifteen_minute, loadavgs[i].fifteen_minute);
	max.one_minute	= MAX(max.one_minute, loadavgs[i].one_minute);
	max.five_minute	= MAX(max.five_minute, loadavgs[i].five_minute);
	max.fifteen_minute
		= MAX(max.fifteen_minute, loadavgs[i].fifteen_minute);
    }
    if(debug>3)
    {
	printf("Continuing with min set: %d,%d,%d\n",
		min.one_minute,
		min.five_minute,
		min.fifteen_minute,
	    NULL);
	printf("Continuing with first set: %d,%d,%d\n",
		loadavgs[0].one_minute,
		loadavgs[0].five_minute,
		loadavgs[0].fifteen_minute,
	    NULL);
	sleep(1);
	printf("MIN Load averages: %f, %f, %f\n",
		min.one_minute / 1024.,
		min.five_minute / 1024.,
		min.fifteen_minute / 1024.,
	    NULL);
	printf("MAX Load averages: %f, %f, %f\n",
		max.one_minute / 1024.,
		max.five_minute / 1024.,
		max.fifteen_minute / 1024.,
	    NULL);
    }
    lmin=MIN(min.one_minute, MIN(min.five_minute, min.fifteen_minute));
    lmax=MAX(max.one_minute, MAX(max.five_minute, max.fifteen_minute));

    if(debug > 3)
	printf("Overall MIN, MAX: %f, %f\n", lmin/1024., lmax/1024.);

    omin	= (int)(lmin / 1024);
    lmin	= 1024 * omin;

    if((lmax / 1024.) < .25)
    {
	omax	= .25;
    }
    else if((lmax / 1024.) < .5)
    {
	omax	= .5;
    }
    else
    {
	omax	= (int)(lmax / 1024) + 1;
    }
    lmax	= 1024 * omax;

    if(debug > 3)
    {
	printf("Boundaries: %d, %d...  ", omin, omax);
	printf("Long Boundaries: %d, %d\n", lmin, lmax);
    }


    for(i=0;i<width;i++)
    {
	loadavgs[i].height1 =
	compute_height(loadavgs[i].one_minute, lmax, height);
	loadavgs[i].height5 =
	compute_height(loadavgs[i].five_minute, lmax, height);
	loadavgs[i].height15 =
	compute_height(loadavgs[i].fifteen_minute, lmax, height);

	if(debug > 3)
	{
	    printf("Load averages: %f, %f, %f  -- ",
		    loadavgs[i].one_minute / 1024.,
		    loadavgs[i].five_minute / 1024.,
		    loadavgs[i].fifteen_minute / 1024.,
		NULL);
	    printf("Heights: %d, %d, %d\n",
		    loadavgs[i].height1,
		    loadavgs[i].height5,
		    loadavgs[i].height15,
		NULL);
	}
    }

    for(j=0;j<=height;j++)
    {
	printf("%6.2f   ",
		(((omax)*(height-j)) / (height))
	    );
	for(i=0;i<width;i++)
	{
	    k=0;
	    if(loadavgs[i].height1	== j)
		k+=ONE;
	    if(loadavgs[i].height5	== j)
		k+=FIVE;
	    if(loadavgs[i].height15	== j)
		k+=FIFTEEN;
	    printf("%s", loadstrings[k]);
	}
	printf("\n");
    }

    memset(strbuf, ' ', BUFSIZ);
    strbuf[cols-1]	= '\0';

    for(i=0;i<clocks;i++)
    {
	if(theclocks[i].pos > 0)
	{
	    strncpy(
		    &strbuf[9+theclocks[i].pos],
		    theclocks[i].clock,
		    6,
		NULL);
	}
    }

    printf("%s\n  Legend:\n"
	   "     1 min: %s, 5 min: %s, 15 min: %s\n"
	   "     1&5 same: %s, 1&15: %s, 5&15: %s, all: %s\n",
	    strbuf,
	    loadstrings[1],
	    loadstrings[2],
	    loadstrings[4],
	    loadstrings[3],
	    loadstrings[5],
	    loadstrings[6],
	    loadstrings[7],
	NULL);
}

#if 0	/* being phased out... ugliness. */
void	getload(kmemfd, loadaddr, loadavgs)
    long	kmemfd, loadaddr;
    load_list	*loadavgs;
{
    if((lseek(kmemfd, loadaddr, SEEK_SET)) != loadaddr)
    {
	perror("Couldn't seek to load address");
	exit(1);
    }

    if((read(kmemfd, loadavgs, 3*sizeof(load_t))) != 3*sizeof(load_t))
    {
	perror("Couldn't read load data");
	exit(1);
    }
}
#endif

int	compute_height(thisload, maxload, height)
    load_t	thisload,
		maxload;
    int		height;
{
    return(/*height-*/(height*((maxload-thisload)/(float)maxload)));
}

void	clear_screen()
{
    printf("\033[H\033[2J");
}

void	cycle_load_list(loadavgs, newload, width)
    load_list	*loadavgs,
		newload;
    int		width;
{
    /* This function will eventually have code to
       clear locations on the screen that change. */

    int i;

    for(i=0;i<(width-1);i++)
    {
	loadavgs[i]	= loadavgs[i+1];
    }
    loadavgs[i]	= newload;

    for(i=0;i<clocks;i++)
    {
	theclocks[i].pos--;
    }
}
