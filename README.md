
> NOTE: This repo is mirrored at https://github.com/cryptonector/ctp and https://github.com/nicowilliams/ctp

# Q: What is it?  A: A user-land-RCU-like API for C, permissively licensed

This repository's only current feature is a read-copy-update (RCU) like,
thread-safe variable (TSV) for C.  More C thread-primitives may be added
in the future, thus the repository's name being quite generic.

A TSV lets readers safely keep using a value read from the TSV until
they read the next value.  Memory management is automatic: values are
automatically destroyed when the last reference to a value is released
whether explicitly, or implicitly at the next read, or when a reader
thread exits.  References can also be relinquished manually.  Reads are
_lock-less_ and fast, and _never block writers_.  Writers are serialized
but otherwise interact with readers without locks, thus writes *do not
block reads*.

> In one of the two implementations included readers only execute atomic
> memory loads and stores, though they loop over that when racing with a
> writer.  As aligned loads and stores are typically atomic on modern
> archictures, this means no expensive atomic operations are needed --
> not even a single atomic increment or decrement.

This is not unlike a Clojure `ref`, or like a Haskell `msync`.  It's
also similar to RCU, but unlike RCU, this has a very simple API with
nothing like `synchronize_rcu()`, and doesn't require any cross-CPU
calls nor the ability to make CPUs/threads run, and it has no
application-visible concept of critical sections, therefore it works in
user-land with no special kernel support.

 - One thread needs to create the variable (as many as desired) once by
   calling `thread_safe_var_init()` and providing a value destructor.

   > There is currently no static initializer, though one could be
   > added.  One would typically do this early in `main()` or in a
   > `pthread_once()` initializer.

 - Most threads only ever need to call `thread_safe_var_get()`.

   > Reader threads _may_ also call `thread_safe_var_release()` to allow
   > a value to be freed sooner than otherwise.

 - One or more threads may call `thread_safe_var_set()` to set new
   values on the TSVs.

The API is:

```C
    typedef struct thread_safe_var *thread_safe_var;    /* TSV */
    typedef void (*thread_safe_var_dtor_f)(void *);     /* Value destructor */

    /* Initialize a TSV with a given value destructor */
    int  thread_safe_var_init(thread_safe_var *, thread_safe_var_dtor_f);

    /* Get the current value of the TSV and a version number for it */
    int  thread_safe_var_get(thread_safe_var, void **, uint64_t *);

    /* Set a new value on the TSV (outputs the new version) */
    int  thread_safe_var_set(thread_safe_var, void *, uint64_t *);

    /* Optional functions follow */

    /* Destroy a TSV */
    void thread_safe_var_destroy(thread_safe_var);

    /* Release the reference to the last value read by this thread from the TSV */
    void thread_safe_var_release(thread_safe_var);

    /* Wait for a value to be set on the TSV */
    int  thread_safe_var_wait(thread_safe_var);
```

Value version numbers increase monotonically when values are set.

# Why?  Because read-write locks are terrible

So you have rarely-changing typically-global data (e.g., loaded
configuration, plugin lists, ...), and you have many threads that read
this, and you want reads to be fast.  Worker threads need stable
configuration/whatever while doing work, then when they pick up another
task they can get a newer configuration if there is one.

How would one implement that?

A safe answer is: read-write locks around reading/writing the variable,
and reference count the data.

But read-write locks are inherently bad: readers either can starve
writers or can be blocked by writers.  Either way read-write locks are a
performance problem.

A "thread-safe variable", on the other hand, is always fast to read,
even when there's an active writer, and reading does not starve writers.

# How?

Two implementations are included at this time.

The two implementations have slightly different characteristics.

 - One implementation ("slot pair") has O(1) lock-less and spin-less
   reads and O(1) serialized writes.

   But readers call free() and the value destructor, and, sometimes have
   to signal a potentially-waiting writer, which involves acquiring a
   mutex -- a blocking operation, yes, though on an uncontended
   resource, so not really blocking.

   This implementation has a pair of slots, one containing the "current"
   value and one containing the "previous"/"next" value.  Writers make the
   "previous" slot into the next "current" slot, and readers read from
   whichever slot appears to be the current slot.  Values are wrapped
   with a wrapper that includes a reference count, and they are released
   when the reference count drops to zero.

   The trick is that writers will wait until the number of active
   readers of the previous slot is zero.  Thus the last reader of a
   previous slot must signal a potentially-awaiting writer (which
   requires taking a lock that the awaiting writer should have
   relinquished in order to wait).  Thus reading is mostly lock-less and
   never blocks on contended resources.

   Values are reference counted and so released immediately when the
   last reference is dropped.

 - The other implementation ("slot list") has O(1) lock-less reads, with
   unreferenced values garbage collected by serialized writers in `O(N
   log(M))` where N is the maximum number of live threads that have read
   the variable and M is the number of values that have been set and
   possibly released).  If writes are infrequent and readers make use of
   `thread_safe_var_release()`, then garbage collection is `O(1)`.
   
   Readers never call the allocator after the first read in any given
   thread, and writers never call the allocator while holding the writer
   lock.

   Readers have to loop over their fast path, a loop that could run
   indefinitely if there were infinitely many higher-priority writers
   who starve the reader of CPU time.  To help avoid this, writers yield
   the CPU before relinquishing the write lock, thus ensuring that some
   readers will have the CPU ahead of any awaiting higher-priority
   writers.

   This implementation has a list of referenced values, with the head of
   the list always being the current one, and a list of "subscription"
   slots, one slot per-reader thread.  Readers allocate a slot on first
   read, and thence copy the head of the values list to their slots.
   Writers have to perform garbage collection on the list of referenced
   values.

   Subscription slot allocation is lock-less.  Indeed, everything is
   lock-less in the reader, and unlike the slot-pair implementation
   there is no case where the reader has to acquire a lock to signal a
   writer.

   Values are released at the first write after the last reference is
   dropped, as values are garbage collected by writers.

The first implementation written was the slot-pair implementation.  The
slot-list design is much easier to understand on the read-side, but it
is significantly more complex on the write-side.

# Requirements

C89, POSIX threads (though TSV should be portable to Windows),
compilers with atomics intrinsics and/or atomics libraries.

In the future this may be upgraded to a C99 or even C11 requirement.

# Testing

A test program is included that hammers the implementation.  Run it in a
loop, with or without helgrind, TSAN (thread sanitizer), or other thread
race checkers, to look for data races.

Both implementations perform similarly well on the included test.

The test pits 20 reader threads waiting various small amounts of time
between reads (one not waiting at all), against 4 writer threads waiting
various small amounts of time between writes.  This test found a variety
of bugs during development.  In both cases writes are, on average, 5x
slower than reads, and reads are in the ten microseconds range on an old
laptop, running under virtualization.

# Performance

On an old i7 laptop, virtualized, reads on idle thread-safe variables
(i.e., no writers in sight) take about 15ns.  This is because the fast
path in both implementations consists of reading a thread-local variable
and then performing a single acquire-fenced memory read.

On that same system, when threads write very frequently then reads slow
down to about 8us (8000ns).  (But the test had eight times more threads
than CPUs, so the cost of context switching is included in that number.)

On that same system writes on a busy thread-safe variable take about
50us (50000ns), but non-contending writes on an otherwise idle
thread-safe variable take about 180ns.

I.e., this is blindingly fast, especially for intended use case
(infrequent writes).

# Install

Clone this repo, select a configuration, and make it.

For example, to build the slot-pair implementation, use:

    $ make clean slotpair

To build the slot-list implementation, use:

    $ make CPPDEFS=-DHAVE_SCHED_YIELD clean slotlist

A GNU-like make(1) is needed.

Configuration variables:

 - `COPTFLAG`
 - `CDBGFLAG`
 -`CC`
 - `ATOMICS_BACKEND`

   Values: `-DHAVE___ATOMIC`, `-DHAVE___SYNC`, `-DHAVE_INTEL_INTRINSICS`, `-DHAVE_PTHREAD`, `-DNO_THREADS`

 - `TSV_IMPLEMENTATION`

   Values: `-DUSE_TSV_SLOT_PAIR_DESIGN`, `-DUSE_TSV_SUBSCRIPTION_SLOTS_DESIGN`

 - `CPPDEFS`

   `CPPDEFS` can also be used to set `NDEBUG`.

A build configuration system is needed, in part to select an atomic
primitive backend.

Several atomic primitives implementations are available:

 - gcc/clang `__atomic`
 - gcc/clang `__sync`
 - Win32 `Interlocked*()`
 - Intel compiler intrinsics (`_Interlocked*()`)
 - global pthread mutex
 - no synchronization (watch the test blow up!)

# Thread Sanitizer (TSAN) Data Race Reports

Currently TSAN (GCC and Clang both) produces no reports.

It is trivial to cause TSAN to produce reports of data races by
replacing some atomic operations with non-atomic operations, therefore
it's clearly the case that TSAN works to find many data races.  That is
not proof that TSAN would catch all possible data races, or that the
tests exercise all possible data races.  A formal approach to proving
the correctness of TSVs would add value.

# Helgrind Data Race Reports

Helgrind reports a number of possible data races.  Many of them are
indeed races, just safe races.  For example this one:

```
Possible data race during write of size 8 at 0x4AAA930 by thread #4
Locks held: 1, at address 0x4AAA8A8
   at 0x485FAD4: atomic_write_ptr (atomics.c:298)
   by 0x485F09B: thread_safe_var_set (thread_safe_global.c:1126)
   by 0x10C05F: writer (t.c:627)
   by 0x484E8AA: ??? (in /usr/libexec/valgrind/vgpreload_helgrind-amd64-linux.so)
   by 0x4913946: start_thread (pthread_create.c:435)
   by 0x49A3A43: clone (clone.S:100)

This conflicts with a previous read of size 8 by thread #3
Locks held: none
   at 0x485FA8E: atomic_read_ptr (atomics.c:232)
   by 0x485EEDF: thread_safe_var_get (thread_safe_global.c:1049)
   by 0x485F7E1: thread_safe_var_wait (thread_safe_global.c:1321)
   by 0x10B598: reader (t.c:476)
   by 0x484E8AA: ??? (in /usr/libexec/valgrind/vgpreload_helgrind-amd64-linux.so)
   by 0x4913946: start_thread (pthread_create.c:435)
   by 0x49A3A43: clone (clone.S:100)
 Address 0x4aaa930 is 144 bytes inside a block of size 176 alloc'd
   at 0x484AAA3: calloc (in /usr/libexec/valgrind/vgpreload_helgrind-amd64-linux.so)
   by 0x485EA60: thread_safe_var_init (thread_safe_global.c:919)
   by 0x10A19D: main (t.c:259)
 Block was alloc'd by thread #1
```

Which corresponds to this:

```C
1125     /* Publish the new value */
1126     atomic_write_ptr((volatile void **)&vp->values, new_value);
1127     vp->nvalues++;
```

```C
1048     while (atomic_read_ptr((volatile void **)&slot->value) !=
1049            (newest = atomic_read_ptr((volatile void **)&vp->values)))
1050         atomic_write_ptr((volatile void **)&slot->value, newest);
```

And, assuming the atomic read/write operations correctly implement
consumer/producer memory barriers, then this is safe because of that
loop at 1048-1050.

The thread sanitizer does not report these races.  Changing some of
these atomics to non-atomics does cause the thread sanitizer to report
the race:

```diff
diff --git a/thread_safe_global.c b/thread_safe_global.c
index 58e8a62..2a788ca 100644
--- a/thread_safe_global.c
+++ b/thread_safe_global.c
@@ -1047,7 +1047,7 @@ thread_safe_var_get(thread_safe_var vp, void **res, uint64_t *version)
      */
     while (atomic_read_ptr((volatile void **)&slot->value) !=
            (newest = atomic_read_ptr((volatile void **)&vp->values)))
-        atomic_write_ptr((volatile void **)&slot->value, newest);
+        slot->value = newest;

     if (newest != NULL) {
         *res = newest->value;
```

causes TSAN to report:

```
WARNING: ThreadSanitizer: data race (pid=746793)
  Write of size 8 at 0x7b1800000000 by thread T1 (mutexes: write M10):
    #0 thread_safe_var_get /home/nico/ws/ctp/thread_safe_global.c:1050 (libtsgv.so+0x5e42)
    #1 thread_safe_var_wait /home/nico/ws/ctp/thread_safe_global.c:1328 (libtsgv.so+0x8059)
    #2 reader /home/nico/ws/ctp/t.c:476 (t+0x8fe7)

  Previous atomic read of size 8 at 0x7b1800000000 by thread T3 (mutexes: write M9):
    #0 __tsan_atomic64_load ../../../../src/libsanitizer/tsan/tsan_interface_atomic.cpp:539 (libtsan.so.0+0x7fe0e)
    #1 atomic_read_ptr /home/nico/ws/ctp/atomics.c:232 (libtsgv.so+0x844f)
    #2 mark_values /home/nico/ws/ctp/thread_safe_global.c:1242 (libtsgv.so+0x6ee3)
    #3 thread_safe_var_set /home/nico/ws/ctp/thread_safe_global.c:1138 (libtsgv.so+0x6ee3)
    #4 writer /home/nico/ws/ctp/t.c:626 (t+0x8305)
```

and more.

And this Helgrind report:

```
Possible data race during write of size 1 at 0x4AAA047 by thread #3
Locks held: none
   at 0x48546D6: mempcpy (in /usr/libexec/valgrind/vgpreload_helgrind-amd64-linux.so)
   by 0x490A631: _IO_new_file_xsputn (fileops.c:1236)
   by 0x490A631: _IO_file_xsputn@@GLIBC_2.2.5 (fileops.c:1197)
   by 0x48F4079: outstring_func (vfprintf-internal.c:239)
   by 0x48F4079: __vfprintf_internal (vfprintf-internal.c:1404)
   by 0x48DF58E: printf (printf.c:33)
   by 0x10B57E: reader (t.c:474)
   by 0x484E8AA: ??? (in /usr/libexec/valgrind/vgpreload_helgrind-amd64-linux.so)
   by 0x4913946: start_thread (pthread_create.c:435)
   by 0x49A3A43: clone (clone.S:100)

This conflicts with a previous write of size 1 by thread #2
Locks held: none
   at 0x48546D6: mempcpy (in /usr/libexec/valgrind/vgpreload_helgrind-amd64-linux.so)
   by 0x490A631: _IO_new_file_xsputn (fileops.c:1236)
   by 0x490A631: _IO_file_xsputn@@GLIBC_2.2.5 (fileops.c:1197)
   by 0x48F4079: outstring_func (vfprintf-internal.c:239)
   by 0x48F4079: __vfprintf_internal (vfprintf-internal.c:1404)
   by 0x48DF58E: printf (printf.c:33)
   by 0x10B57E: reader (t.c:474)
   by 0x484E8AA: ??? (in /usr/libexec/valgrind/vgpreload_helgrind-amd64-linux.so)
   by 0x4913946: start_thread (pthread_create.c:435)
   by 0x49A3A43: clone (clone.S:100)
```

is almost certainly not a bug in glibc but a spurious data race report
from Helgrind.

I'm still investigating all the Helgrind possible data race reports.

Because TSAN is intimately involved with the compiler, and because
Helgrind does not seem to understand atomic operations, I currently
trust TSAN more than Helgrind for lock-less data structures.

# TODO

 - Don't create a pthread-specific variable for each TSV.  Instead share
   one pthread-specific for all TSVs.  This would require having the
   pthread-specific values be a pointer to a structure that has a
   pointer to an array of per-TSV elements, with
   `thread_safe_var_init()` allocating an array index for each TSV.

   This is important because there can be a maximum number of
   pthread-specifics and we must not be the cause of exceeding that
   maximum.

 - Add an attributes optional input argument to the init function.

   Callers should be able to express the following preferences:

    - OK for readers to spin, yes or no.        (No  -> slot-pair)
    - OK for readers to alloc/free, yes or no.  (No  -> slot-list, GC)
    - Whether version waiting is desired.       (Yes -> slot-pair)

   On conflict give priority to functionality.

 - Add a version predicate to set or a variant that takes a version
   predicate.  (A version predicate -> do not set the new value unless
   the current value's version number is the given one.)

 - Add an API for waiting for values older than some version number to
   be released?
  
   This is tricky for the slot-pair case because we don't have a list of
   extant values, but we need it in order to determine what is the
   oldest live version at any time.  Such a list would have to be
   doubly-linked and updating the double links to remove items would be
   rather difficult to do lock-less-ly and thread-safely.  We could
   defer freeing of list elements so that only tail elements can be
   removed.  When a wrapper's refcount falls to zero, signal any waiters
   who can then garbage collect the lists with the writer lock held and
   find the oldest live version.

   For the slot-list case the tricky part is that unreferenced values
   are only detected when there's a write.  We could add a refcount to
   the slot-list case, so that when refcounts fall to zero we signal any
   waiter(s), but because of the way readers find a current value...
   reference counts could go down to zero then back up, so we must still
   rely on GC to actually free, and we can only rely on refcounts to
   signal a waiter.

   It seems we need a list and refcounts, so that the slot-pair and
   slot-list cases become quite similar, and the only difference
   ultimately is that slot-list can spin while slot-pair cannot.  Thus
   we might want to merge the two implementations, with attributes of
   the variable (see above) determining which codepaths get taken.

   Note too that both implementations can (or do) defer calling of the
   value destructor so that reading is fast.  This should be an option.

 - Add a static initializer?

 - Add a better build system.

 - Add an implementation using read-write locks to compare performance
   with.

 - Use symbol names that don't conflict with any known atomics libraries
   (so those can be used as an atomics backend).  Currently the atomics
   symbols are loosely based on Illumos atomics primitives.

 - Support Win32 (perhaps by building a small pthread compatibility
   library; only mutexes and condition variables are needed).
