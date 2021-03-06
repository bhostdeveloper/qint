/*
** Copyright 1998 - 2002 Double Precision, Inc.
** See COPYING for distribution information.
*/

#if	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	"numlib.h"
#include	<string.h>

static const char rcsid[]="$Id: strofft.c,v 1.5 2003/01/05 04:01:17 mrsam Exp $";

char *libmail_str_off_t(off_t t, char *arg)
{
	char	buf[NUMBUFSIZE];
	char	*p=buf+sizeof(buf)-1;
	int	isneg=0;

	if (t < 0)
	{
		t= -t;
		isneg=1;
	}

	*p=0;
	do
	{
		*--p= '0' + (t % 10);
		t=t / 10;
	} while(t);

	if (isneg)
		*--p='-';

	return (strcpy(arg, p));
}
