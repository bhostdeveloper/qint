#ifndef	message_h
#define	message_h

static const char message_h_rcsid[]="$Id: message.h,v 1.5 2003/01/26 15:45:16 mrsam Exp $";

//////////////////////////////////////////////////////////////////////////////
//
// A Message object represents the message being filtered.  It is expected
// that the message is going to be scanned, repetitively, from start to
// finish, in one direction.  Therefore, access to the message contents is
// sequential.
//
// A Message object can be initiated in two ways.  It can be initiated from
// a file.  The Message object will check if the file is seekable.  If so,
// the Message object will simply use the Mio class to access the file.
// If the file is not seekable, the message will be saved in a temporary
// file, and the Mio class will be used to access the temporary file.
//
// However, if the message size turns out to be smaller the SMALLMSG bytes,
// the message is saved in a memory buffer, and accessed from there, for
// speed.  We presume that most messages are less than SMALLMSG bytes long.
//
// The second way is for the messages contents to be manually specified.
// For this initialization method, first the message is saved into the
// memory buffer.  If it proves to be too small, the message gets written
// out into a temporary file.
//
//////////////////////////////////////////////////////////////////////////////

#include	"mio.h"
#include	"tempfile.h"
#include	<sys/types.h>
#include	<errno.h>
#include	<string.h>
#include	"config.h"

#if HAVE_UNISTD_H
#include	<unistd.h>
#else

#ifndef	SEEK_SET
#define	SEEK_SET	0
#endif

#endif

class Buffer;

class Message {
	Mio mio;
	TempFile tempfile;
	char	*buffer;
	char	*bufptr;
	char	*extra_headers;
	char	*extra_headersptr;
	off_t	msgsize;
	off_t	msglines;

	static void readerr();
	static void seekerr();
public:
	Message();
	~Message();
	void Init(int);		// Initialize from file descriptor

	void Init();		// Begin initializing externally
	void Init(const void *, unsigned);	// From existing contents.
	void ExtraHeaders(const Buffer &);
	void Rewind();		// Start reading the message
	void RewindIgnore();	// Rewind, ignore msginfo
	int appendline(Buffer &, int=1);	// Read newline-terminated line.
	void seek(off_t);
	off_t tell();
	int get_c();
	int peek();
	off_t MessageSize();
	off_t MessageLines() { return (msglines); }
	void setmsgsize();
	} ;

#include	"funcs.h"
#include	"maildrop.h"

inline int Message::peek()		// Current character.
{
	if (extra_headersptr)
		return ( (unsigned char) *extra_headersptr );

	if (mio.fd() >= 0)
	{
		errno=0;

	int	c=mio.peek();

		if (c < 0 && errno)	readerr();
		return (c);
	}

	if (bufptr >= buffer + msgsize)	return (-1);
	return ( (int)(unsigned char)*bufptr );
}

inline int Message::get_c()		// Get character.
{
int	c;

	if (extra_headersptr)
	{
		c= (unsigned char) *extra_headersptr++;
		if (!*extra_headersptr)	extra_headersptr=0;
		return (c);
	}

	if (mio.fd() >= 0)
	{
		errno=0;

		c=mio.get();
		if (c < 0 && errno)	readerr();
		return (c);
	}

	if (bufptr >= buffer + msgsize)	return (-1);
	return ( (int)(unsigned char)*bufptr++ );
}

inline off_t Message::tell()
{
off_t	pos;

	if ( mio.fd() < 0)
		pos=bufptr - buffer;
	else
	{
		pos=mio.tell();
		if (pos == -1)	seekerr();
	}
	pos -= maildrop.msginfo.msgoffset;
	if (extra_headersptr)
		pos += extra_headersptr-extra_headers;
	else
	{
		if (extra_headers)	pos += strlen(extra_headers);
	}
	return (pos);
}

inline void Message::seek(off_t n)
{
int	l=0;

	if (extra_headers && n < (l=strlen(extra_headers)))
	{
		extra_headersptr= extra_headers + n;
		n=0;
	}
	else
	{
		extra_headersptr=0;
		n -= l;
	}
	n += maildrop.msginfo.msgoffset;

	if (mio.fd() >= 0)
	{
		if (mio.seek(n, SEEK_SET) < 0)	seekerr();
	}
	else
	{
		if (n > msgsize)	n=msgsize;
		bufptr=buffer+n;
	}
}

inline off_t Message::MessageSize()
{
	off_t s=msgsize;
	s -= maildrop.msginfo.msgoffset;
	if (extra_headers)
		s += strlen(extra_headers);

	return (s);
}

#endif
