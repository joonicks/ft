#define _GNU_SOURCE
#include <dirent.h> /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>


#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct linux_dirent
{
	uint32_t	d_ino;
	off_t		d_off;
	uint16_t	d_reclen;
	char		d_name[];
};

struct dataent
{
	uint64_t	nr;
	char		name[200];

} dataent;

#define DB_ENTS	2000

struct dataent	db[DB_ENTS];

#define BUF_SIZE 1024
#define MAXLEN	4096
#define MSGLEN	2048

char	globaldata[MAXLEN];

/*
 *  Format text and send to a socket or file descriptor
 */
int fdwrite(int fd, const char *format, ...)
{
	va_list msg;

	if (fd == -1)
		return(-1);

	va_start(msg,format);
	vsprintf(globaldata,format,msg);
	va_end(msg);

	return(write(fd,globaldata,strlen(globaldata)));
}

/*
 *  Read any data waiting on a socket or file descriptor
 *  and return any complete lines to the caller
 */
char *fdread(int fd, char *rest, char *line)
{
	char	*src,*dst,*rdst;
	int	n;

	errno = EAGAIN;

	src = rest;
	dst = line;

	while(*src)
	{
		if (*src == '\n' || *src == '\r')
		{
		gotline:
			while(*src == '\n' || *src == '\r')
				src++;
			*dst = 0;
			dst = rest;
			while(*src)
				*(dst++) = *(src++);
			*dst = 0;
			return((*line) ? line : NULL);
		}
		*(dst++) = *(src++);
	}
	rdst = src;

	n = read(fd,globaldata,MSGLEN-2);
	switch(n)
	{
	case 0:
		errno = EPIPE;
	case -1:
		return(NULL);
	}

	globaldata[n] = 0;
	src = globaldata;

	while(*src)
	{
		if (*src == '\r' || *src == '\n')
			goto gotline;
		if ((dst - line) >= (MSGLEN-2))
		{
			/*
			 *  line is longer than buffer, let the wheel spin
			 */
			src++;
			continue;
		}
		*(rdst++) = *(dst++) = *(src++);
	}
	*rdst = 0;
	return(NULL);
}

void findfiles(const char *directory)
{
	int	fd, nread;
	char	buf[BUF_SIZE];
	char	subdir[BUF_SIZE];
	char	symdir[BUF_SIZE];
	struct linux_dirent *d;
	int	bpos;
	char	d_type,*src,*dst;

	fd = open(directory, O_RDONLY | O_DIRECTORY);
	if (fd == -1)
		handle_error("open");

	for (;;)
	{
		nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
		if (nread == -1)
			handle_error("getdents");

		if (nread == 0)
			break;

		for (bpos = 0; bpos < nread;)
		{
			d = (struct linux_dirent *) (buf + bpos);
			d_type = *(buf + bpos + d->d_reclen - 1);

			sprintf(subdir,"%s/%s",directory,d->d_name);

			if (d_type == DT_REG)
			{
				if (strcmp(d->d_name,"meta.txt") == 0)
					printf("file %s\n",subdir);
			}
			else
			if (d_type == DT_DIR && d->d_name[0] != '.')
			{
				printf("dir %s\n",subdir);
				findfiles(subdir);
			}
			bpos += d->d_reclen;
		}
	}
	close(fd);
}

int main(int argc, char *argv[])
{
	char	line[MSGLEN];
	char	rest[MSGLEN];
	char	*c,*s,*d,*name,*ph;
	int	fd,dbct,a,skip;

	memset(db,0,sizeof(db));

	findfiles(".");

	exit(EXIT_SUCCESS);
}
