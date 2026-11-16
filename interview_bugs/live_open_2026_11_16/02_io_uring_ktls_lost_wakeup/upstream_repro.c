// SPDX-License-Identifier: MIT
/*
 * A multishot recv on a kTLS socket stops completing while the socket still
 * holds readable records.
 *
 * A sender thread writes 16 KiB records in bursts over a loopback TCP pair with
 * kernel TLS on both ends (fixed keys, no handshake). The receiver reads with
 * one multishot recv and provided buffers. When no CQE arrives for STALL_MS
 * while SIOCINQ > 0 and poll() reports POLLIN, the recv is stuck: it is left
 * alone for PERM_S seconds to show it does not recover, then a single
 * recv(MSG_PEEK) on the socket unsticks it and the run continues.
 *
 * build: gcc -O2 -Wall -o repro repro.c -luring -lpthread
 * usage: ./repro [-t seconds] [-l limit_bytes] [-b nbufs] [-s bufsize] [-w spin_ns] [-p] [-d] [-B mib] [-P us]
 *   -l  set sqe->optlen (IORING_RECV_MSHOT_LIM): end the multishot every N bytes
 *   -p  plain TCP (no kTLS)
 *   -d  default ring (no SINGLE_ISSUER / DEFER_TASKRUN)
 *   -B/-P  sender bursts of 1..B MiB, pauses of 0..P us (default 8, 2000)
 * exit: 0 = no stall, 1 = stalled, 2 = setup error
 */
#include <errno.h>
#include <linux/tls.h>
#include <liburing.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#ifndef SOL_TLS
#define SOL_TLS 282
#endif

#define BGID 0
#define STALL_MS 1000
#define PERM_S 5

static atomic_bool stop;
static unsigned max_burst_mib = 8, max_pause_us = 2000;

static double now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void die(const char *what)
{
	perror(what);
	exit(2);
}

static void ktls(int fd, int dir)
{
	struct tls12_crypto_info_aes_gcm_256 ci = {
		.info.version = TLS_1_3_VERSION,
		.info.cipher_type = TLS_CIPHER_AES_GCM_256,
	};

	memset(ci.key, 0x11, sizeof(ci.key));
	memset(ci.iv, 0x22, sizeof(ci.iv));
	memset(ci.salt, 0x33, sizeof(ci.salt));
	if (setsockopt(fd, IPPROTO_TCP, TCP_ULP, "tls", 3))
		die("TCP_ULP tls (modprobe tls?)");
	if (setsockopt(fd, SOL_TLS, dir, &ci, sizeof(ci)))
		die("SOL_TLS");
}

static void pair(int *tx, int *rx)
{
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
	socklen_t len = sizeof(a);
	int l = socket(AF_INET, SOCK_STREAM, 0);

	if (l < 0 || bind(l, (void *)&a, sizeof(a)) || listen(l, 1) ||
	    getsockname(l, (void *)&a, &len))
		die("listen");
	*tx = socket(AF_INET, SOCK_STREAM, 0);
	if (*tx < 0 || connect(*tx, (void *)&a, sizeof(a)))
		die("connect");
	*rx = accept(l, NULL, NULL);
	if (*rx < 0)
		die("accept");
	close(l);
}

/* Bursts of 1..max_burst_mib MiB at full speed, then a 0..max_pause_us pause. */
static void *sender(void *arg)
{
	int fd = *(int *)arg;
	static char rec[16384];
	unsigned seed = 1;

	while (!atomic_load(&stop)) {
		size_t burst = (size_t)(1 + rand_r(&seed) % max_burst_mib) << 20;

		for (size_t sent = 0; sent < burst && !atomic_load(&stop);) {
			ssize_t n = send(fd, rec, sizeof(rec), MSG_NOSIGNAL);

			if (n < 0) {
				if (errno == EINTR)
					continue;
				return NULL;
			}
			sent += n;
		}
		if (max_pause_us)
			usleep(rand_r(&seed) % max_pause_us);
	}
	return NULL;
}

static void arm(struct io_uring *ring, int fd, unsigned limit)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

	io_uring_prep_recv_multishot(sqe, fd, NULL, 0, 0);
	sqe->flags |= IOSQE_BUFFER_SELECT;
	sqe->buf_group = BGID;
	sqe->optlen = limit;
	sqe->user_data = 1;
}

static void spin(long ns)
{
	double end = now() + ns / 1e9;

	while (ns > 0 && now() < end)
		;
}

int main(int argc, char **argv)
{
	unsigned nbufs = 1024, bufsize = 32768, limit = 0;
	double secs = 60;
	long spin_ns = 0;
	int plain = 0, deflt = 0, c, tx, rx, ret;

	while ((c = getopt(argc, argv, "t:l:b:s:w:pdB:P:")) != -1) {
		switch (c) {
		case 't': secs = atof(optarg); break;
		case 'l': limit = strtoul(optarg, NULL, 0); break;
		case 'b': nbufs = strtoul(optarg, NULL, 0); break;
		case 's': bufsize = strtoul(optarg, NULL, 0); break;
		case 'w': spin_ns = atol(optarg); break;
		case 'p': plain = 1; break;
		case 'd': deflt = 1; break;
		case 'B': max_burst_mib = strtoul(optarg, NULL, 0); break;
		case 'P': max_pause_us = strtoul(optarg, NULL, 0); break;
		default: return 2;
		}
	}

	setvbuf(stdout, NULL, _IOLBF, 0);
	struct utsname u;
	uname(&u);
	printf("kernel %s, %s, %s ring, %u x %u B buffers, limit %u, spin %ld ns, %.0f s\n",
	       u.release, plain ? "plain TCP" : "kTLS", deflt ? "default" : "DEFER_TASKRUN",
	       nbufs, bufsize, limit, spin_ns, secs);

	pair(&tx, &rx);
	if (!plain) {
		ktls(tx, TLS_TX);
		ktls(rx, TLS_RX);
	}

	struct io_uring ring;
	struct io_uring_params p = {
		.flags = IORING_SETUP_CQSIZE |
			 (deflt ? 0 : IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN),
		.cq_entries = 4 * nbufs,
	};
	if ((ret = io_uring_queue_init_params(64, &ring, &p)))
		return fprintf(stderr, "queue_init: %s\n", strerror(-ret)), 2;

	struct io_uring_buf_ring *br = io_uring_setup_buf_ring(&ring, nbufs, BGID, 0, &ret);
	char *bufs = malloc((size_t)nbufs * bufsize);
	if (!br || !bufs)
		return fprintf(stderr, "buf ring: %s\n", strerror(-ret)), 2;
	for (unsigned i = 0; i < nbufs; i++)
		io_uring_buf_ring_add(br, bufs + (size_t)i * bufsize, bufsize, i,
				      io_uring_buf_ring_mask(nbufs), i);
	io_uring_buf_ring_advance(br, nbufs);

	pthread_t th;
	pthread_create(&th, NULL, sender, &tx);
	arm(&ring, rx, limit);

	double start = now(), last = start;
	unsigned long long bytes = 0, rearms = 0, enobufs = 0, stalls = 0;
	struct __kernel_timespec ts = { .tv_nsec = 100 * 1000 * 1000 };

	while (now() - start < secs) {
		struct io_uring_cqe *cqe;
		unsigned head, seen = 0;

		io_uring_submit_and_wait_timeout(&ring, &cqe, 1, &ts, NULL);
		io_uring_for_each_cqe(&ring, head, cqe) {
			seen++;
			if (cqe->flags & IORING_CQE_F_BUFFER) {
				unsigned bid = cqe->flags >> IORING_CQE_BUFFER_SHIFT;

				io_uring_buf_ring_add(br, bufs + (size_t)bid * bufsize, bufsize, bid,
						      io_uring_buf_ring_mask(nbufs), 0);
				io_uring_buf_ring_advance(br, 1);
			}
			if (cqe->res > 0) {
				bytes += cqe->res;
				spin(spin_ns);
			}
			if (!(cqe->flags & IORING_CQE_F_MORE)) {
				if (cqe->res == -ENOBUFS)
					enobufs++;
				else if (cqe->res <= 0)
					return fprintf(stderr, "recv ended: %d\n", cqe->res), 2;
				rearms++;
				arm(&ring, rx, limit);
			}
		}
		io_uring_cq_advance(&ring, seen);
		if (seen) {
			last = now();
			continue;
		}
		if (now() - last < STALL_MS / 1000.0)
			continue;

		int inq = 0;
		struct pollfd pfd = { .fd = rx, .events = POLLIN };
		ioctl(rx, SIOCINQ, &inq);
		poll(&pfd, 1, 0);
		if (inq <= 0 || !(pfd.revents & POLLIN))
			continue;

		stalls++;
		printf("STALL %llu at %.1f s after %.2f GiB: no CQE for %.1f s, SIOCINQ %d, POLLIN set\n",
		       stalls, now() - start, bytes / 1073741824.0, now() - last, inq);
		if (stalls == 1) {
			sleep(PERM_S);
			io_uring_submit_and_wait_timeout(&ring, &cqe, 1, &ts, NULL);
			printf("  still stuck after %d more s: %s\n", PERM_S,
			       io_uring_cq_ready(&ring) ? "no, it recovered" : "yes");
		}
		char b;
		recv(rx, &b, 1, MSG_PEEK | MSG_DONTWAIT);
		last = now();
	}

	atomic_store(&stop, true);
	shutdown(tx, SHUT_RDWR);
	shutdown(rx, SHUT_RDWR);
	pthread_join(th, NULL);
	printf("%.2f GiB in %.1f s, %llu re-arms (%llu ENOBUFS), %llu stalls\n",
	       bytes / 1073741824.0, now() - start, rearms, enobufs, stalls);
	return stalls ? 1 : 0;
}
