# Native Vita compatibility review — revision 01.55

Reference: Backstab HD port by v-atamanenko, commit
290130a8d6cb4b882ad445da4b4d9ac9f8377436:
https://github.com/v-atamanenko/backstab-vita

The reference confirms the use of an ARM Android library, softfp VitaSDK,
kuBridge, vitaGL and game-specific lifecycle/graphics patches. Its game entry
points cannot be substituted for Zombiewood's GL2JNI lifecycle. The reference
also uses a 256 MB newlib heap and a 6 MB vitaGL threshold, so those old numbers
alone are not proof of a defect. Revision 01.54's smaller heap, larger headroom
and conservative textures remain unverified mitigation choices.

Concrete defects corrected in the Android threading bridge:

1. pthread_once marked the control occupied, then allowed every competing
   thread to return before the initializer finished. It now distinguishes
   in-progress from complete and publishes completion with release/acquire
   atomics. The game imports pthread_once.
2. The object registry's own lazy lock creation raced. Mutex/condition first
   use also performed separate lookup, allocation and registration operations,
   allowing two threads to create different native objects for one Android
   object. A native static registry lock and serialized, double-checked object
   creation now prevent duplicate publication. Allocation/registry failures
   return errors instead of dereferencing invalid objects.
3. pthread_create overwrote every explicit thread stack with 512 KB, ignoring
   pthread_attr_setstacksize. Explicit stack sizes are now preserved; threads
   without a requested size use a 1 MB default.
4. sem_timedwait rejected future deadlines, used an overflowing 32-bit seconds
   multiplication, and returned failures without the required errno. It now
   uses a 64-bit absolute deadline, handles available tokens immediately, waits
   for future deadlines and reports ETIMEDOUT on expiry.
5. The error logger's lazy kernel-mutex creation could race. It now uses a
   statically initialized native pthread mutex. Failed mutex/condition destroy
   calls no longer free an object that the native API rejected destroying.

tools/test_thread_bridge.c runs four rounds with eight competing threads,
checking single completion and 8,000 protected increments per round, plus
explicit 2 MB stack preservation, a delayed semaphore post, timeout expiry and
an available token with a past deadline: 12 checks. This tests the compiled ARM
bridge through Vita3K; it does not validate real-device boot or GPU behavior.
No test-input or test-runner code is linked into the release VPK.

The physical data paths, all mapped import names, both OBB archive CRCs and
the packaged SELF are checked separately. The user-reported Vita-1000 failure
before the first presented frame remains unconfirmed as resolved; no device
log or crash dump was available during this review.
