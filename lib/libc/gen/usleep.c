/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)usleep.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#ifdef  _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"
#endif

#if !defined(_THREAD_SAFE) && !defined(USE_NANOSLEEP)
#define	TICK	10000		/* system clock resolution in microseconds */
#define	USPS	1000000		/* number of microseconds in a second */
#endif

#define	setvec(vec, a) \
	vec.sv_handler = a; vec.sv_mask = vec.sv_onstack = 0

#if !defined(_THREAD_SAFE) && !defined(USE_NANOSLEEP)
static int ringring;
#endif

static void
sleephandler()
{
#if !defined(_THREAD_SAFE) && !defined(USE_NANOSLEEP)
	ringring = 1;
#endif
}


void
usleep(useconds)
	unsigned int useconds;
{
#if defined(_THREAD_SAFE) || defined(USE_NANOSLEEP)
	struct timespec time_to_sleep;
	struct timespec time_remaining;
	struct sigvec vec, ovec;

	if (useconds) {
		time_to_sleep.tv_nsec = (useconds % 1000000) * 1000;
		time_to_sleep.tv_sec = useconds / 1000000;
		setvec(vec, sleephandler);
		(void) sigvec(SIGALRM, &vec, &ovec);
		do {
			nanosleep(&time_to_sleep, &time_remaining);
			time_to_sleep = time_remaining;
		} while (time_to_sleep.tv_sec != 0 &&
			 time_to_sleep.tv_nsec != 0);
		(void) sigvec(SIGALRM, &ovec, (struct sigvec *)0);
	}
#else
	register struct itimerval *itp;
	struct itimerval itv, oitv;
	struct sigvec vec, ovec;
	long omask;
	static void sleephandler();

	itp = &itv;
	if (!useconds)
		return;
	timerclear(&itp->it_interval);
	timerclear(&itp->it_value);
	if (setitimer(ITIMER_REAL, itp, &oitv) < 0)
		return;
	itp->it_value.tv_sec = useconds / USPS;
	itp->it_value.tv_usec = useconds % USPS;
	if (timerisset(&oitv.it_value)) {
		if (timercmp(&oitv.it_value, &itp->it_value, >)) {
			oitv.it_value.tv_sec -= itp->it_value.tv_sec;
			oitv.it_value.tv_usec -= itp->it_value.tv_usec;
			if (oitv.it_value.tv_usec < 0) {
				oitv.it_value.tv_usec += USPS;
				oitv.it_value.tv_sec--;
			}
		} else {
			itp->it_value = oitv.it_value;
			oitv.it_value.tv_sec = 0;
			oitv.it_value.tv_usec = 2 * TICK;
		}
	}
	setvec(vec, sleephandler);
	(void) sigvec(SIGALRM, &vec, &ovec);
	omask = sigblock(sigmask(SIGALRM));
	ringring = 0;
	(void) setitimer(ITIMER_REAL, itp, (struct itimerval *)0);
	while (!ringring)
		sigpause(omask &~ sigmask(SIGALRM));
	(void) sigvec(SIGALRM, &ovec, (struct sigvec *)0);
	(void) sigsetmask(omask);
	(void) setitimer(ITIMER_REAL, &oitv, (struct itimerval *)0);
#endif
}
