
// See multithread.h for a description of the contents of this file.

#include "stdafx.h"

#include <cassert>

#include "platform.h"
#include "multithread.h"

#ifdef _WIN32

// ===========================================================================
// Windows multithreading

// ---------------------------------------------------------------------------
unsigned mt::coreCount() {
	SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    sysinfo.dwNumberOfProcessors;
	return sysinfo.dwNumberOfProcessors;
}

// ---------------------------------------------------------------------------
struct ThreadInfo {
	void (*start)( void* );
	void *cookie;
};

// ---------------------------------------------------------------------------
static DWORD WINAPI win_thread_entry( LPVOID pvInfo ) {
	ThreadInfo info;
	{
		ThreadInfo *pinfo = (ThreadInfo*)pvInfo;
		info = *pinfo;
		delete pinfo;
	}

	info.start( info.cookie );
	return 0;
}

// ---------------------------------------------------------------------------
void mt::spawnThread( void (*start)( void* ), void *cookie ) {
	ThreadInfo *info = new ThreadInfo;
	info->start = start;
	info->cookie = cookie;
	CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)win_thread_entry, (LPVOID)info, 0, NULL );
}

// ---------------------------------------------------------------------------
void *mt::newMutex() {
	return static_cast<void*>(CreateMutex( NULL, false, NULL ));
}

// ---------------------------------------------------------------------------
void mt::deleteMutex( void *mut ) {
	BOOL ret = CloseHandle( static_cast<HANDLE>(mut) );
	assert( ret );
}

// ---------------------------------------------------------------------------
void mt::lock( void *mut ) {
	DWORD ret = WaitForSingleObject( static_cast<HANDLE>(mut), INFINITE );
	assert( ret != WAIT_FAILED );
}

// ---------------------------------------------------------------------------
void mt::unlock( void *mut ) {
	ReleaseMutex( static_cast<HANDLE>(mut) );
}

// ---------------------------------------------------------------------------
void *mt::newSemaphore( int startCount ) {
	return static_cast<void*>(CreateSemaphore( NULL, startCount, 0x7fffffffl, NULL ));
}

// ---------------------------------------------------------------------------
void mt::deleteSemaphore( void *sema ) {
	BOOL ret = CloseHandle( static_cast<HANDLE>(sema) );
	assert( ret );
}

// ---------------------------------------------------------------------------
void mt::p( void *sema ) {
	DWORD ret = WaitForSingleObject( static_cast<HANDLE>(sema), INFINITE );
	assert( ret != WAIT_FAILED );
}

// ---------------------------------------------------------------------------
void mt::v( void *sema ) {
	BOOL ret = ReleaseSemaphore( static_cast<HANDLE>(sema), 1, NULL );
	assert( ret );
}

#elif defined( __unix__ ) || ( defined( __APPLE__ ) && defined( __MACH__ ) )

// ===========================================================================
// POSIX multithreading

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
unsigned mt::coreCount() {
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	return (unsigned)n;
}

// ---------------------------------------------------------------------------
struct ThreadInfo {
	void (*start)( void* );
	void *cookie;
};

// ---------------------------------------------------------------------------
static void *pthread_main( void *pvInfo ) {
	ThreadInfo info;
	{
		ThreadInfo *pinfo = (ThreadInfo*)pvInfo;
		info = *pinfo;
		delete pinfo;
	}

	info.start( info.cookie );
	return NULL;
}

// ---------------------------------------------------------------------------
void mt::spawnThread( void (*start)( void* ), void *cookie ) {
	ThreadInfo *info = new ThreadInfo;
	info->start = start;
	info->cookie = cookie;

	pthread_t thread;
	pthread_attr_t attr;
	int rv = pthread_attr_init( &attr );
	assert( rv == 0 ); (void)rv;
	rv = pthread_create( &thread, &attr, &pthread_main, (void*)info );
	assert( rv == 0 ); (void)rv;
}

// ---------------------------------------------------------------------------
void *mt::newMutex() {
	pthread_mutex_t *mut = new pthread_mutex_t;
	pthread_mutexattr_t attr;
	int rv = pthread_mutexattr_init( &attr );
	assert( rv == 0 ); (void)rv;
	rv = pthread_mutex_init( mut, &attr );
	assert( rv == 0 ); (void)rv;
	return mut;
}

// ---------------------------------------------------------------------------
void mt::deleteMutex( void *mut ) {
	int rv = pthread_mutex_destroy( (pthread_mutex_t*)mut );
	assert( rv == 0 ); (void)rv;
	delete (pthread_mutex_t*)mut;
}

// ---------------------------------------------------------------------------
void mt::lock( void *mut ) {
	int rv = pthread_mutex_lock( (pthread_mutex_t*)mut );
	assert( rv == 0 ); (void)rv;
}

// ---------------------------------------------------------------------------
void mt::unlock( void *mut ) {
	int rv = pthread_mutex_unlock( (pthread_mutex_t*)mut );
	assert( rv == 0 ); (void)rv;
}

// ---------------------------------------------------------------------------
void *mt::newSemaphore( int startCount ) {
	sem_t *sema = new sem_t;
	int rv = sem_init( sema, 0, startCount );
	assert( rv == 0 ); (void)rv;
	return sema;
}

// ---------------------------------------------------------------------------
void mt::deleteSemaphore( void *pvSema ) {
	sem_t *sema = (sem_t*)pvSema;
	int rv = sem_destroy( sema );
	assert( rv == 0 ); (void)rv;
	delete sema;
}

// ---------------------------------------------------------------------------
void mt::p( void *pvSema ) {
	sem_t *sema = (sem_t*)pvSema;
	int rv = sem_wait( sema );
	assert( rv == 0 ); (void)rv;
}

// ---------------------------------------------------------------------------
void mt::v( void *pvSema ) {
	sem_t *sema = (sem_t*)pvSema;
	int rv = sem_post( sema );
	assert( rv == 0 ); (void)rv;
}

#else

// ===========================================================================
// Default: No multithreading

// ---------------------------------------------------------------------------
unsigned mt::coreCount() {
	return 1;
}

// ---------------------------------------------------------------------------
void mt::spawnThread( void (*start)( void* ), void *cookie ) {
	(void)start;
	(void)cookie;
	assert(false);
}

// ---------------------------------------------------------------------------
void *mt::newMutex() {
	assert(false);
	return NULL;
}

// ---------------------------------------------------------------------------
void mt::deleteMutex( void *mut ) {
	(void)mut;
	assert(false);
}

// ---------------------------------------------------------------------------
void mt::lock( void *mut ) {
	(void)mut;
	assert(false);
}

// ---------------------------------------------------------------------------
void mt::unlock( void *mut ) {
	(void)mut;
	assert(false);
}

// ---------------------------------------------------------------------------
void *mt::newSemaphore( int startCount ) {
	(void)startCount;
	assert(false);
	return NULL;
}

// ---------------------------------------------------------------------------
void mt::deleteSemaphore( void *sema ) {
	(void)sema;
	assert(false);
}

// ---------------------------------------------------------------------------
void mt::p( void *sema ) {
	(void)sema;
	assert(false);
}

// ---------------------------------------------------------------------------
void mt::v( void *sema ) {
	(void)sema;
	assert(false);
}

#endif
