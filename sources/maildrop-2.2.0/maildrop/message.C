#include	"message.h"
#include	"buffer.h"
#include	"xconfig.h"
#include	"funcs.h"
#include	"varlist.h"

static const char rcsid[]="$Id: message.C,v 1.8 2003/10/30 00:22:57 mrsam Exp $";

Message::Message() : buffer(0), bufptr(0),
		extra_headers(0), extra_headersptr(0), msgsize(0)
{
}

Message::~Message()
{
	mio.fd(-1);	// Either way, it's not our file
	if (buffer)	delete[] buffer;
	if (extra_headers) delete[] extra_headers;
}

void Message::Init()
{
	if (buffer)
	{
		delete[] buffer;
		buffer=0;
	}
	if (extra_headers)
	{
		delete[] extra_headers;
		extra_headers=0;
	}
	extra_headersptr=0;
	msgsize=0;
	msglines=0;
	tempfile.Close();
	mio.fd(-1);
}

void Message::Init(int fd)
{
	Init();
	// If the file descriptor is seekable, and the message is big
	// keep message in the file.
	if ( mseek(fd, 0, SEEK_END) >= 0 &&
		(msgsize=mseek(fd, 0, SEEK_CUR)) >= 0 && msgsize > SMALLMSG)
	{

	// BUGFIX 0.55 - mio takes ownership of the file descriptor. When
	// ::Init() is called, it will call fd(-1) which will CLOSE this
	// descriptor we have now.
	//
	// This func is invoked from main with STDIN - 0, so we must take
	// ownership of a duplicate.

		fd=dup(fd);
		if (fd < 0)	throw "dup() failed.";

		mio.fd(fd);
		if (mio.Rewind() < 0)	seekerr();

	int	c;

		while ((c=mio.get()) >= 0)
			if (c == '\n')	msglines++;

		return;
	}
	// Well, just read the message, and let Init() figure out what to
	// do.

	mseek(fd, 0, SEEK_SET);	// Just in case
	msgsize=0;

#ifdef	BUFSIZ
char	buf[BUFSIZ];
#else
char	buf[512];
#endif
int	n;

	while ((n=read(fd, buf, sizeof(buf))) > 0)
		Init(buf, n);
	if (n < 0)
		throw "Error - read() failed reading message.";
#if CRLF_TERM
	msgsize += msglines;
#endif
}

void Message::Init(const void *data, unsigned cnt)
{
	{
	const char *p=(const char*)data;
	unsigned n=cnt;

		while (n)
		{
			if (*p++ == '\n')	++msglines;
			--n;
		}
	}

	if (mio.fd() < 0)	// Still trying to save to buffer
	{
		if (!buffer)
		{
			buffer=new char[SMALLMSG];
			bufptr=buffer;
			msgsize=0;
			if (!buffer)	outofmem();
		}

		if (SMALLMSG - msgsize >= (off_t)cnt)	// Can still do it
		{
			memcpy(bufptr, data, cnt);
			bufptr += cnt;
			msgsize += cnt;
			return;
		}

	int	fd;

#if	SHARED_TEMPDIR

		fd=tempfile.Open();

		if (fd < 0)
			throw "Unable to create temporary file.";
#else
		while ( (fd=tempfile.Open( TempName(),
			O_RDWR | O_CREAT | O_EXCL, 0600)) < 0 &&
			errno == EEXIST)
			;
		if (fd < 0)
			throw "Unable to create temporary file - check permissions on $HOME/" TEMPDIR;
#endif

		mio.fd(fd);

		if ((off_t)mio.write(buffer, msgsize) != msgsize)
			throw "Unable to write to temporary file - possibly out of disk space.";

		delete[] buffer;
		buffer=0;
	}

	if ((unsigned)mio.write(data, cnt) != cnt)
		throw "Unable to write to temporary file - possibly out of disk space.";
	msgsize += cnt;
}

void Message::ExtraHeaders(const Buffer &buf)
{
	if ( extra_headers )
	{
		delete[] extra_headers;
		extra_headers=0;
	}
	extra_headersptr=0;
	if (!buf.Length())	return;

	extra_headers=new char[buf.Length()+1];
	if (!extra_headers)	outofmem();
	memcpy(extra_headers, (const char *)buf, buf.Length());
	extra_headers[buf.Length()]=0;
	extra_headersptr=extra_headers;
	if (!*extra_headersptr)	extra_headersptr=0;
}

void Message::RewindIgnore()
{
	extra_headersptr=0;
	if (mio.fd() >= 0)
	{
		if (mio.Rewind() < 0)	seekerr();
		return;
	}
	bufptr=buffer;
}

void Message::Rewind()
{
	RewindIgnore();

off_t	n=maildrop.msginfo.msgoffset;

	while (n)
	{
		(void)get_c();
		--n;
	}
	extra_headersptr=extra_headers;
	if (extra_headersptr && !*extra_headersptr)
		extra_headersptr=0;
}

void Message::readerr()
{
	throw "Read error.";
}

void Message::seekerr()
{
	throw "Seek error.";
}

int Message::appendline(Buffer &buf, int stripcr)
{
	if (mio.fd() >= 0 || extra_headersptr)
	{
	int	c;
	int	eof= 1;
	int	lastc=0;

		while ((c=get_c()) > 0 && c != '\n')
		{
			eof=0;
			buf.push(c);
			lastc=c;
		}
		if (c < 0 && eof)	return (-1);
		if (stripcr && lastc == '\r')	buf.pop();
						// Drop trailing CRs
		buf.push('\n');
		return (0);
	}

	if (bufptr >= buffer + msgsize)
	{
		return (-1);
	}

unsigned cnt=buffer + msgsize - bufptr;
unsigned i;

	for (i=0; i<cnt; i++)
		if (bufptr[i] == '\n')
		{
			if (i > 0 && stripcr && bufptr[i-1] == '\r')
				buf.append(bufptr, i-1);
				// Drop trailing CRs
			else
				buf.append(bufptr, i);
			buf += '\n';
			bufptr += ++i;
			return (0);
		}

	if (stripcr && bufptr[i-1] == '\r')
		buf.append(bufptr, cnt-1);
				// Drop trailing CRs
	else
		buf.append(bufptr, cnt);
	bufptr += cnt;
	buf += '\n';
	return (0);
}

void Message::setmsgsize()
{
Buffer	n,v;

	n="SIZE";
	v.append((unsigned long)MessageSize());
	SetVar(n,v);
	n="LINES";
	v.reset();
	v.append((unsigned long)MessageLines());
	SetVar(n,v);
}
