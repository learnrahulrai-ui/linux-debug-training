# Issue 1644: no-answer-key calculation sheet
Source report: https://github.com/axboe/liburing/issues/1644
Use one calculation per exchange. Do not provide a kernel fix until the trainee proves a failure.
Original machine was 7.2.6-arch2-1, liburing 2.15. Your machine is NOT measured here.
Original C reproducer: upstream_repro.c. Compiling source.cpp does NOT trigger kernel bug.
Reporter has 2 DISTINCT sources of evidence: full C reproducer and application kprobe trace.
The numerical trace below comes from the APPLICATION, not from the public C reproducer.

Reported application io_poll_issue entry values:
raw 0x1 -> return -3072
raw 0x22 -> return -3072
raw 0x43 -> return -3072
raw 0x64 -> return -3072
raw 0x80 -> return -3072
raw 0x80 -> return -11
At +56us an io_poll_wake observed raw=0x40000080.
From Linux io_uring/poll.c v7.3-rc4:
IO_POLL_REF_BIAS=128
IO_POLL_RETRY_FLAG=BIT(30)
IO_POLL_REF_MASK=GENMASK(29,0)
From io_uring/net.c v7.3-rc4: MULTISHOT_MAX_RETRY=32.

HAND CALCULATIONS (leave blank until performed):
0x1=___; 0x22=___; 0x43=___; 0x64=___; 0x80=___.
0x22-0x1=___; 0x43-0x22=___; 0x64-0x43=___; 0x80-0x64=___; 0x80-0x80=___.
First non-33 delta=___; observed facts that explain deviation=___.
0x40000080 & 0x3fffffff=___.
0x40000080 & 0x40000000=___.
0x40000080 - 0x80=___.
(0x40000080 - 0x80) & 0x3fffffff=___.
0x80 vs IO_POLL_REF_BIAS: ___.
Check original io_poll_get_ownership, io_poll_check_events and io_poll_task_func before interpreting result.

REAL DATA SIZES:
1024 buffers * 32768 bytes =___ bytes =___ MiB.
256 buffers * 32768 bytes =___ bytes =___ MiB.
16384 byte record * 2 =___ bytes.
1048576 limit / 32768 bytes =___ maximum full-size receives; partial receives change limit behavior.
818802 queued bytes / 32768 byte buffers = quotient___ remainder___.
Original 9 / 17 stalls=___%; 0 / 11 stalls with limit=___%.
A zero count in 11 trials is NOT proof that failure is impossible.

OBSERVED TIMELINE (not from original C reproducer):
+0us io_poll_issue, raw=0x80
+27us io_poll_wake, raw=0x80
+34us tls_sw_recvmsg returned 32768
+55us tls_rx_msg_maybe_announce msg_ready=1 full_len=16406
+56us io_poll_wake, raw=0x40000080
+58us tls_sw_recvmsg returned 16483
+78us tls_sw_recvmsg returned 16393
+83us tls_sw_recvmsg len=32768, MSG_DONTWAIT
+92us tls_rx_msg_maybe_announce msg_ready=1 full_len=16406
+93us io_poll_wake, raw=0x40000080
+95us tls_sw_recvmsg returned -EAGAIN
+96us io_recv/io_poll_issue returned -EAGAIN
Record the exact first bit change=___; event trigger=___; who can observe state=___.
List timestamps that are NOT sufficient to reconstruct all machine instructions=___.
Check net/tls/tls_strp.c before claiming an eternal stall.

TESTS BEFORE CODE:
A. raw=0x80, decode_reference_count = TODO (manual expected value___).
B. raw=0x40000080, decode_retry_bit = TODO (manual expected___).
C. raw=0x40000080, release_reference(raw,0x80) = TODO (manual expected___).
D. after release, loop_condition = TODO (manual expected___).
C++ default output must say STATUS=UNSOLVED. It exits zero solely to verify boilerplate compilation.
Fill TODO A, compile and manually compare. Continue with B, C and D ONLY after each is correct.

REPRODUCTION, isolated VM or controlled loopback test only:
sh probe_readonly.sh
make exercise
./source
make upstream-repro          # requires liburing development headers/library
./upstream-repro -t 45       # original kTLS default
./upstream-repro -t 45 -p    # plain TCP control
./upstream-repro -t 45 -l 1048576 # limited multishot control
./upstream-repro -t 45 -b 256    # smaller buffer count control
Exit code: 0 no stalls observed THIS run; 1 stall observed; 2 setup/runtime error.
Source reports a 1-second absence of CQEs with socket readable, then waits an extra 5 seconds, then calls recv(MSG_PEEK). Record all three observations separately.
Do not claim a cause from one successful or failed command.

AFTER REPRODUCTION:
Capture exact uname, liburing version, kernel SHA, feature values and traces.
Derive raw task and buffer lifetime, count every wakeup, identify all exit paths.
Compare historical commits a26a35e9019f, 704ea888d646, 76b367a2d831, a68ed2df7213.
The last commit fixed a distinct EOF/HUP problem; do not call it the fix for this EPOLLIN issue.
Only after disproving alternative explanations, design a kernel change and test regressions.
