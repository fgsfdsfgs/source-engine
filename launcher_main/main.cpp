//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.
//
//=====================================================================================//

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <direct.h>
#endif
#if defined( _X360 )
#define _XBOX
#include <xtl.h>
#include <xbdm.h>
#include <stdio.h>
#include <assert.h>
#include "xbox\xbox_core.h"
#include "xbox\xbox_launch.h"
#endif
#ifdef POSIX
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#ifdef PLATFORM_PSVITA
#define VRTLD_LIBDL_COMPAT 1
#include <vrtld.h>
#include <vitasdk.h>
#define MAX_PATH 1024
#else
#include <dlfcn.h>
#define MAX_PATH PATH_MAX
#endif
#endif

#include "tier0/basetypes.h"

#ifdef WIN32
typedef int (*LauncherMain_t)( HINSTANCE hInstance, HINSTANCE hPrevInstance, 
							  LPSTR lpCmdLine, int nCmdShow );
#elif POSIX
typedef int (*LauncherMain_t)( int argc, char **argv );
#else
#error
#endif

#ifdef WIN32
// hinting the nvidia driver to use the dedicated graphics card in an optimus configuration
// for more info, see: http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
extern "C" { _declspec( dllexport ) DWORD NvOptimusEnablement = 0x00000001; }

// same thing for AMD GPUs using v13.35 or newer drivers
extern "C" { __declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1; }

#endif


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
#if !defined( _X360 )

static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ MAX_PATH ];
	char szBuffer[ MAX_PATH ];
	size_t j;
	char *pBuffer = NULL;

	strcpy( szBuffer, pszBuffer );

	pBuffer = strrchr( szBuffer,'\\' );
	if ( pBuffer )
	{
		*(pBuffer+1) = '\0';
	}

	strcpy( basedir, szBuffer );

	j = strlen( basedir );
	if (j > 0)
	{
		if ( ( basedir[ j-1 ] == '\\' ) || 
			 ( basedir[ j-1 ] == '/' ) )
		{
			basedir[ j-1 ] = 0;
		}
	}

	return basedir;
}

#ifdef WIN32

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// Must add 'bin' to the path....
	char* pPath = getenv("PATH");

	// Use the .EXE name to determine the root directory
	char moduleName[ MAX_PATH ];
	char szBuffer[4096];
	if ( !GetModuleFileName( hInstance, moduleName, MAX_PATH ) )
	{
		MessageBox( 0, "Failed calling GetModuleFileName", "Launcher Error", MB_OK );
		return 0;
	}

	// Get the root directory the .exe is in
	char* pRootDir = GetBaseDir( moduleName );

#ifdef _DEBUG
	int len = 
#endif
	_snprintf( szBuffer, sizeof( szBuffer ), "PATH=%s\\bin\\;%s", pRootDir, pPath );
	szBuffer[sizeof( szBuffer ) - 1] = '\0';
	assert( len < sizeof( szBuffer ) );
	_putenv( szBuffer );

	// Assemble the full path to our "launcher.dll"
	_snprintf( szBuffer, sizeof( szBuffer ), "%s\\bin\\launcher.dll", pRootDir );
	szBuffer[sizeof( szBuffer ) - 1] = '\0';

	// STEAM OK ... filesystem not mounted yet
#if defined(_X360)
	HINSTANCE launcher = LoadLibrary( szBuffer );
#else
	HINSTANCE launcher = LoadLibraryEx( szBuffer, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
#endif
	if ( !launcher )
	{
		char *pszError;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&pszError, 0, NULL);

		char szBuf[1024];
		_snprintf(szBuf, sizeof( szBuf ), "Failed to load the launcher DLL:\n\n%s", pszError);
		szBuf[sizeof( szBuf ) - 1] = '\0';
		MessageBox( 0, szBuf, "Launcher Error", MB_OK );

		LocalFree(pszError);
		return 0;
	}

	LauncherMain_t main = (LauncherMain_t)GetProcAddress( launcher, "LauncherMain" );
	return main( hInstance, hPrevInstance, lpCmdLine, nCmdShow );
}

#elif defined (PLATFORM_PSVITA)

#include <vitaGL.h>
#include <string.h>
#include <ctype.h>

#define DATA_PATH "data/halflife2"
#define MAX_ARGV 5

// 256MB libc heap, 512K main thread stack, 70MB for loading game DLLs
// the rest goes to vitaGL
extern "C" SceUInt32 sceUserMainThreadStackSize = 512 * 1024;
extern "C" unsigned int _pthread_stack_default_user = 512 * 1024;
extern "C" unsigned int _newlib_heap_size_user = 260 * 1024 * 1024;
#define VGL_MEM_THRESHOLD ( 70 * 1024 * 1024 )

// HACKHACK: create some slack at the end of the RX segment of the ELF
// for vita-elf-create to put the generated symbol table into
const unsigned char vitaelf_slack __attribute__ ((used, aligned (0x20000))) = 0xFF;

/* HACKHACK: force-export stuff required by the dynamic libs */

#include "SDL.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <wchar.h>
#include <fcntl.h>
#include <dirent.h>
#include <curl/curl.h>
#include <math.h>

extern "C" void *__aeabi_idiv;
extern "C" void *__aeabi_uidiv;
extern "C" void *__aeabi_idivmod;
extern "C" void *__aeabi_ldivmod;
extern "C" void *__aeabi_uidivmod;
extern "C" void *__aeabi_uldivmod;
extern "C" void *__aeabi_d2lz;
extern "C" void *__aeabi_d2ulz;
extern "C" void *__aeabi_l2d;
extern "C" void *__aeabi_l2f;
extern "C" void *__aeabi_ul2d;
extern "C" void *__aeabi_ul2f;
extern "C" void *_ZTIi;

extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6resizeEjc;
extern "C" void *_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE5c_strEv;
extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev;
extern "C" void *_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6lengthEv;
extern "C" void *_ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE5emptyEv;
extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6assignEPKc;
extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEED1Ev;
extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEaSEPKc;
extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE5eraseEjj;
extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_replaceEjjPKcj;
extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6resizeEj;
extern "C" void *_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv;

extern "C" void *__powidf2;
extern "C" void *__powisf2;

extern "C" {
void *__wrap_calloc(uint32_t nmember, uint32_t size) { return vglCalloc(nmember, size); }
void __wrap_free(void *addr) { vglFree(addr); };
void *__wrap_malloc(uint32_t size) { return vglMalloc(size); };
void *__wrap_memalign(uint32_t alignment, uint32_t size) { return vglMemalign(alignment, size); };
void *__wrap_realloc(void *ptr, uint32_t size) { return vglRealloc(ptr, size); };
void *__wrap_memcpy (void *dst, const void *src, size_t num) { return sceClibMemcpy(dst, src, num); };
void *__wrap_memset (void *ptr, int value, size_t num) { return sceClibMemset(ptr, value, num); };
};

#define DEF_STUB( stret, stname, ... ) \
	extern "C" stret stname ( __VA_ARGS__ ) { }
#define DEF_STUB_LOGGED( stret, stname, ... ) \
	extern "C" stret stname ( __VA_ARGS__ ) { fprintf( stderr, "!! " #stname "\n" ); }

extern "C" void glGetTexLevelParameteriv( unsigned target, int level, unsigned pname, int *params )
{
	// unsupported for individual texlevels
	// ... but also unsupported in general
	// glGetTexParameteriv( target, pname, params );
}

extern "C" void glTexParameterfv( unsigned target, unsigned parm, const float *params )
{
	// ayy lmao
	// this is only called with GL_TEXTURE_BORDER_COLOR and GL_TEXTURE_LOD_BIAS
	int intparms[4] = { 0 };
	glTexParameteriv( target, parm, intparms );
}

extern "C" void glSamplerParameterfv( unsigned target, unsigned parm, const float *params )
{
	// same, but iv is unimplemented
}

DEF_STUB( void, glBlendColor, float r, float g, float b, float a )
DEF_STUB( void, glGenQueries, unsigned count, unsigned *objs )
DEF_STUB( void, glDeleteQueries, unsigned count, const unsigned *objs )
DEF_STUB_LOGGED( void, glBeginQuery, unsigned type, unsigned obj )
DEF_STUB( void, glEndQuery, unsigned type )
DEF_STUB( void, glGetQueryObjectiv, unsigned obj, unsigned parm, int *out )
DEF_STUB( void, glGetQueryObjectuiv, unsigned obj, unsigned parm, unsigned *out )
DEF_STUB( void, glGetSynciv, unsigned a, unsigned b, unsigned c, unsigned *d, int *e )
DEF_STUB_LOGGED( void, glClientWaitSync, unsigned a, unsigned b, unsigned long long c )
DEF_STUB_LOGGED( void, glWaitSync, unsigned a, unsigned b, unsigned long long c )
DEF_STUB_LOGGED( void, glFenceSync, unsigned a, unsigned b )
DEF_STUB( void, glDeleteSync, unsigned a )
DEF_STUB( void, glDetachShader, unsigned prog, unsigned sh )
DEF_STUB( void, glDrawBuffer, unsigned target )
DEF_STUB( void, glDrawBuffers, unsigned target )
DEF_STUB( void, glReadBuffer, unsigned target )
DEF_STUB_LOGGED( void, glBlitFramebuffer, int x, int y, int w, int h, int x2, int y2, int w2, int h2 )
DEF_STUB_LOGGED( void, glCopyBufferSubData, unsigned a, unsigned b, int c, int d, int e )
DEF_STUB( void, glFramebufferTexture3D, GLenum a, GLenum b, GLenum c, GLuint d, GLint e, GLint f )
DEF_STUB_LOGGED( void, glRenderbufferStorageMultisample, GLenum a, GLsizei b, GLenum c, GLsizei d, GLsizei e )

#undef DEF_STUB
#undef DEF_STUB_LOGGED

// generates a filename in the cwd
static char *Q_tmpnam( char *s )
{
	static char buf[1024];
	static int cnt = 0;
	snprintf( buf, sizeof(buf), "__tmp_%06d", cnt++ );
	if ( s ) strcpy( s, buf );
	return buf;
}

static const vrtld_export_t aux_exports[] =
{
	VRTLD_EXPORT_SYMBOL( __aeabi_d2lz ),
	VRTLD_EXPORT_SYMBOL( __aeabi_d2ulz ),
	VRTLD_EXPORT_SYMBOL( __aeabi_idiv ),
	VRTLD_EXPORT_SYMBOL( __aeabi_idivmod ),
	VRTLD_EXPORT_SYMBOL( __aeabi_ldivmod ),
	VRTLD_EXPORT_SYMBOL( __aeabi_uidivmod ),
	VRTLD_EXPORT_SYMBOL( __aeabi_uldivmod ),
	VRTLD_EXPORT_SYMBOL( __aeabi_uidiv ),
	VRTLD_EXPORT_SYMBOL( __aeabi_l2d ),
	VRTLD_EXPORT_SYMBOL( __aeabi_l2f ),
	VRTLD_EXPORT_SYMBOL( __aeabi_ul2d ),
	VRTLD_EXPORT_SYMBOL( __aeabi_ul2f ),
	VRTLD_EXPORT_SYMBOL( _ZTIi ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6resizeEjc ),
	VRTLD_EXPORT_SYMBOL( _ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE5c_strEv ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev ),
	VRTLD_EXPORT_SYMBOL( _ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6lengthEv ),
	VRTLD_EXPORT_SYMBOL( _ZNKSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE5emptyEv ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6assignEPKc ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEED1Ev ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEaSEPKc ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE5eraseEjj ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_replaceEjjPKcj ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6resizeEj ),
	VRTLD_EXPORT_SYMBOL( _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv ),
	VRTLD_EXPORT_SYMBOL( _impure_ptr ),
	VRTLD_EXPORT_SYMBOL( __powidf2 ),
	VRTLD_EXPORT_SYMBOL( __powisf2 ),
	VRTLD_EXPORT_SYMBOL( SDL_Init ),
	VRTLD_EXPORT_SYMBOL( SDL_GL_GetProcAddress ),
	VRTLD_EXPORT_SYMBOL( SDL_SetClipboardText ),
	VRTLD_EXPORT_SYMBOL( SDL_GetClipboardText ),
	VRTLD_EXPORT_SYMBOL( SDL_HasClipboardText ),
	VRTLD_EXPORT_SYMBOL( scePowerGetArmClockFrequency ),
	VRTLD_EXPORT_SYMBOL( curl_global_init ),
	VRTLD_EXPORT_SYMBOL( curl_easy_init ),
	VRTLD_EXPORT_SYMBOL( curl_easy_perform ),
	VRTLD_EXPORT_SYMBOL( curl_easy_cleanup ),
	VRTLD_EXPORT_SYMBOL( curl_easy_getinfo ),
	VRTLD_EXPORT_SYMBOL( curl_easy_setopt ),
	VRTLD_EXPORT_SYMBOL( atol ),
	VRTLD_EXPORT_SYMBOL( time ),
	VRTLD_EXPORT_SYMBOL( mktime ),
	VRTLD_EXPORT_SYMBOL( ctime ),
	VRTLD_EXPORT_SYMBOL( ctime_r ),
	VRTLD_EXPORT_SYMBOL( fscanf ),
	VRTLD_EXPORT_SYMBOL( feof ),
	VRTLD_EXPORT_SYMBOL( ferror ),
	VRTLD_EXPORT_SYMBOL( freopen ),
	VRTLD_EXPORT_SYMBOL( dup ),
	VRTLD_EXPORT_SYMBOL( rewind ),
	VRTLD_EXPORT_SYMBOL( fileno_unlocked ),
	VRTLD_EXPORT_SYMBOL( vasprintf ),
	VRTLD_EXPORT_SYMBOL( vsprintf ),
	VRTLD_EXPORT_SYMBOL( vswprintf ),
	VRTLD_EXPORT_SYMBOL( vprintf ),
	VRTLD_EXPORT_SYMBOL( printf ),
	VRTLD_EXPORT_SYMBOL( putchar ),
	VRTLD_EXPORT_SYMBOL( puts ),
	VRTLD_EXPORT_SYMBOL( select ),
	VRTLD_EXPORT_SYMBOL( socket ),
	VRTLD_EXPORT_SYMBOL( connect ),
	VRTLD_EXPORT_SYMBOL( accept ),
	VRTLD_EXPORT_SYMBOL( listen ),
	VRTLD_EXPORT_SYMBOL( bind ),
	VRTLD_EXPORT_SYMBOL( setsockopt ),
	VRTLD_EXPORT_SYMBOL( getsockopt ),
	VRTLD_EXPORT_SYMBOL( gethostname ),
	VRTLD_EXPORT_SYMBOL( gethostbyname ),
	VRTLD_EXPORT_SYMBOL( inet_addr ),
	VRTLD_EXPORT_SYMBOL( send ),
	VRTLD_EXPORT_SYMBOL( sendto ),
	VRTLD_EXPORT_SYMBOL( recv ),
	VRTLD_EXPORT_SYMBOL( recvfrom ),
	VRTLD_EXPORT_SYMBOL( tolower ),
	VRTLD_EXPORT_SYMBOL( toupper ),
	VRTLD_EXPORT_SYMBOL( isalnum ),
	VRTLD_EXPORT_SYMBOL( isalpha ),
	VRTLD_EXPORT_SYMBOL( isupper ),
	VRTLD_EXPORT_SYMBOL( islower ),
	VRTLD_EXPORT_SYMBOL( isprint ),
	VRTLD_EXPORT_SYMBOL( ispunct ),
	VRTLD_EXPORT_SYMBOL( iscntrl ),
	VRTLD_EXPORT_SYMBOL( isatty ),
	VRTLD_EXPORT_SYMBOL( strtok ),
	VRTLD_EXPORT_SYMBOL( strspn ),
	VRTLD_EXPORT_SYMBOL( strchrnul ),
	VRTLD_EXPORT_SYMBOL( stpcpy ),
	VRTLD_EXPORT_SYMBOL( rand ),
	VRTLD_EXPORT_SYMBOL( srand ),
	VRTLD_EXPORT_SYMBOL( qsort_r ),
	VRTLD_EXPORT_SYMBOL( unlink ),
	VRTLD_EXPORT_SYMBOL( access ),
	VRTLD_EXPORT_SYMBOL( wcstol ),
	VRTLD_EXPORT_SYMBOL( wcstoll ),
	VRTLD_EXPORT_SYMBOL( wcstombs ),
	VRTLD_EXPORT_SYMBOL( wcscat ),
	VRTLD_EXPORT_SYMBOL( wcschr ),
	VRTLD_EXPORT_SYMBOL( wcsncat ),
	VRTLD_EXPORT_SYMBOL( wcsncpy ),
	VRTLD_EXPORT_SYMBOL( wcstod ),
	VRTLD_EXPORT_SYMBOL( swscanf ),
	VRTLD_EXPORT_SYMBOL( sincosf ),
	VRTLD_EXPORT_SYMBOL( fminf ),
	VRTLD_EXPORT_SYMBOL( fmaxf ),
	VRTLD_EXPORT_SYMBOL( alphasort ),

	VRTLD_EXPORT( "dlopen", (void *)vrtld_dlopen ),
	VRTLD_EXPORT( "dlclose", (void *)vrtld_dlclose ),
	VRTLD_EXPORT( "dlsym", (void *)vrtld_dlsym ),
	VRTLD_EXPORT( "tmpnam", (void *)Q_tmpnam ),
	VRTLD_EXPORT( "iconv", (void *)SDL_iconv ),
	VRTLD_EXPORT( "iconv_open", (void *)SDL_iconv_open ),
	VRTLD_EXPORT( "iconv_close", (void *)SDL_iconv_close ),
	VRTLD_EXPORT( "calloc", (void *)__wrap_calloc ),
	VRTLD_EXPORT( "free", (void *)__wrap_free ),
	VRTLD_EXPORT( "malloc", (void *)__wrap_malloc ),
	VRTLD_EXPORT( "memalign", (void *)__wrap_memalign ),
	VRTLD_EXPORT( "realloc", (void *)__wrap_realloc ),
	VRTLD_EXPORT( "memcpy", (void *)__wrap_memcpy ),
	VRTLD_EXPORT( "memset", (void *)__wrap_memset ),
};

extern "C" const vrtld_export_t *__vrtld_exports = aux_exports;
extern "C" const unsigned int __vrtld_num_exports = sizeof( aux_exports ) / sizeof( *aux_exports );

/* end of export crap */

// DLL load order
static const char *dep_list[] = {
	"tier0",
	"steam_api",
	"vstdlib",
	"togl",
};

static bool PSVita_GetBasePath( char *buf, const size_t buflen )
{
	// check if a xash3d folder exists on one of these drives
	// default to the last one (ux0)
	static const char *drives[] = { "uma0", "imc0", "ux0" };
	SceUID dir;
	size_t i;

	for ( i = 0; i < sizeof( drives ) / sizeof( *drives ); ++i )
	{
		snprintf( buf, buflen, "%s:" DATA_PATH, drives[i] );
		dir = sceIoDopen( buf );
		if ( dir >= 0 )
		{
			sceIoDclose( dir );
			return true;
		}
	}

	return false;
}

static const char *PSVita_GetLaunchParameter( char *outbuf )
{
	SceAppUtilAppEventParam param;
	memset( &param, 0, sizeof( param ) );
	sceAppUtilReceiveAppEvent( &param );
	if( param.type == 0x05 )
	{
		sceAppUtilAppEventParseLiveArea( &param, outbuf );
		return outbuf;
	}
	return NULL;
}

/*
===========
PSVita_GetArgv

On the PS Vita under normal circumstances argv is empty, so we'll construct our own
based on which button the user pressed in the LiveArea launcher.
===========
*/
static int PSVita_GetArgv( int in_argc, char **in_argv, char ***out_argv )
{
	static const char *fake_argv[MAX_ARGV] = { "./eboot.bin", NULL };
	int fake_argc = 1;
	char tmp[2048] = { 0 };
	SceAppUtilInitParam initParam = { 0 };
	SceAppUtilBootParam bootParam = { 0 };

	// on the Vita under normal circumstances argv is empty, unless we're launching from Change Game
	sceAppUtilInit( &initParam, &bootParam );

	if( in_argc > 1 )
	{
		// probably coming from Change Game, in which case we just need to keep the old args
		*out_argv = in_argv;
		return in_argc;
	}

	// got empty args, which means that we're probably coming from LiveArea
	// construct argv based on which button the user pressed in the LiveArea launcher
	if( PSVita_GetLaunchParameter( tmp ))
	{
		if( !strcmp( tmp, "dev" ))
		{
			// user hit the "Developer Mode" button, inject "-log" and "-dev" arguments
			fake_argv[fake_argc++] = "-log";
			fake_argv[fake_argc++] = "-dev";
			fake_argv[fake_argc++] = "2";
		}
	}

	*out_argv = (char **)fake_argv;
	return fake_argc;
}

int main( int argc, char *argv[] )
{
	char tmp[1024] = { 0 };
	char **fake_argv = NULL;
	int fake_argc = 0;

	// cd to the base dir immediately for library loading to work
	if( PSVita_GetBasePath( tmp, sizeof( tmp )))
	{
		chdir( tmp );
	}

	// make the temp dir
	mkdir( "temp", 0777 );

	sceTouchSetSamplingState( SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_STOP );
	scePowerSetArmClockFrequency( 444 );
	scePowerSetBusClockFrequency( 222 );
	scePowerSetGpuClockFrequency( 222 );
	scePowerSetGpuXbarClockFrequency( 166 );
	sceSysmoduleLoadModule( SCE_SYSMODULE_NET );

	if( vrtld_init( 0 ) < 0 )
	{
		fprintf( stderr, "Fatal Error: Could not init vrtld:\n%s\n", vrtld_dlerror() );
		return 0;
	}

	// init vitaGL, leaving some memory for DLL mapping
	vglUseVram( GL_TRUE );
	vglUseExtraMem( GL_TRUE );
	vglUseCachedMem( GL_TRUE );
	vglInitWithCustomThreshold( 0, 960, 544, VGL_MEM_THRESHOLD, 0, 0, 0, 0 );

	// load dependencies
	for ( size_t i = 0; i < sizeof(dep_list) / sizeof(*dep_list); ++i )
	{
		snprintf( tmp, sizeof(tmp), "bin/lib%s" DLL_EXT_STRING, dep_list[i] );
		void *dep = dlopen( tmp, RTLD_NOW );
		if ( !dep )
		{
			fprintf( stderr, "Could not load %s: %s\n", tmp, dlerror() );
			return 0;
		}
	}

	// now we can chain into the launcher DLL

	void *launcher = dlopen( "bin/liblauncher" DLL_EXT_STRING, RTLD_NOW );
	if ( !launcher )
		launcher = dlopen( "bin/launcher" DLL_EXT_STRING, RTLD_NOW );

	if ( !launcher )
	{
		fprintf( stderr, "%s\nFailed to load the launcher\n", dlerror() );
		return 0;
	}

	LauncherMain_t emain = (LauncherMain_t)dlsym( launcher, "LauncherMain" );
	if ( !emain )
	{
		fprintf( stderr, "Failed to load the launcher entry proc\n" );
		return 0;
	}

	fake_argc = PSVita_GetArgv( argc, argv, &fake_argv );
	return emain( fake_argc, fake_argv );
}

#elif defined (POSIX)

#if defined( LINUX )

#include <fcntl.h>
#include <sys/stat.h>

static bool IsDebuggerPresent( int time )
{
	// Need to get around the __wrap_open() stuff. Just find the open symbol
	// directly and use it...
	typedef int (open_func_t)( const char *pathname, int flags, mode_t mode );
	open_func_t *open_func = (open_func_t *)dlsym( RTLD_NEXT, "open" );

	if ( open_func )
	{
		for ( int i = 0; i < time; i++ )
		{
			int tracerpid = -1;

			int fd = (*open_func)( "/proc/self/status", O_RDONLY, S_IRUSR );
			if (fd >= 0)
			{
				char buf[ 4096 ];
				static const char tracerpid_str[] = "TracerPid:";

				const int len = read( fd, buf, sizeof(buf) - 1 );
				if ( len > 0 )
				{
					buf[ len ] = 0;

					const char *str = strstr( buf, tracerpid_str );
					tracerpid = str ? atoi( str + sizeof( tracerpid_str ) ) : -1;
				}

				close( fd );
			}

			if ( tracerpid > 0 )
				return true;

			sleep( 1 );
		}
	}

	return false;
}

static void WaitForDebuggerConnect( int argc, char *argv[], int time )
{
	for ( int i = 1; i < argc; i++ )
	{
		if ( strstr( argv[i], "-wait_for_debugger" ) )
		{
			printf( "\nArg -wait_for_debugger found.\nWaiting %dsec for debugger...\n", time );
			printf( "  pid = %d\n", getpid() );

			if ( IsDebuggerPresent( time ) )
				printf("Debugger connected...\n\n");

			break;
		}
	}
}

#else

static void WaitForDebuggerConnect( int argc, char *argv[], int time )
{
}

#endif // !LINUX

int main( int argc, char *argv[] )
{
	char ld_path[4196];
	char *path = "bin/";
	char *ld_env;

	if( (ld_env = getenv("LD_LIBRARY_PATH")) != NULL )
	{
		snprintf(ld_path, sizeof(ld_path), "%s:bin/", ld_env);
		path = ld_path;
	}

	setenv("LD_LIBRARY_PATH", path, 1);

	extern char** environ;
	if( getenv("NO_EXECVE_AGAIN") == NULL )
	{
		setenv("NO_EXECVE_AGAIN", "1", 1);
		execve(argv[0], argv, environ);
	}

	void *launcher = dlopen( "bin/liblauncher" DLL_EXT_STRING, RTLD_NOW );
	if ( !launcher )
		fprintf( stderr, "%s\nFailed to load the launcher\n", dlerror() );

	if( !launcher )
		launcher = dlopen( "bin/launcher" DLL_EXT_STRING, RTLD_NOW );

	if ( !launcher )
	{
		fprintf( stderr, "%s\nFailed to load the launcher\n", dlerror() );
		return 0;
	}

	LauncherMain_t main = (LauncherMain_t)dlsym( launcher, "LauncherMain" );
	if ( !main )
	{
		fprintf( stderr, "Failed to load the launcher entry proc\n" );
		return 0;
	}

#if defined(__clang__) && !defined(OSX)
	// When building with clang we absolutely need the allocator to always
	// give us 16-byte aligned memory because if any objects are tagged as
	// being 16-byte aligned then clang will generate SSE instructions to move
	// and initialize them, and an allocator that does not respect the
	// contract will lead to crashes. On Linux we normally use the default
	// allocator which does not give us this alignment guarantee.
	// The google tcmalloc allocator gives us this guarantee.
	// Test the current allocator to make sure it gives us the required alignment.
	void* pointers[20];
	for (int i = 0; i < ARRAYSIZE(pointers); ++i)
	{
		void* p = malloc( 16 );
		pointers[ i ] = p;
		if ( ( (size_t)p ) & 0xF )
		{
			printf( "%p is not 16-byte aligned. Aborting.\n", p );
			printf( "Pass /define:CLANG to VPC to correct this.\n" );
			return -10;
		}
	}
	for (int i = 0; i < ARRAYSIZE(pointers); ++i)
	{
		if ( pointers[ i ] )
			free( pointers[ i ] );
	}

	if ( __has_feature(address_sanitizer) )
	{
		printf( "Address sanitizer is enabled.\n" );
	}
	else
	{
		printf( "No address sanitizer!\n" );
	}
#endif

	WaitForDebuggerConnect( argc, argv, 30 );

	return main( argc, argv );
}

#else
#error
#endif // WIN32 || POSIX


#else // X360
//-----------------------------------------------------------------------------
// 360 Quick and dirty command line parsing. Returns true if key found,
// false otherwise. Caller can optionally get next argument.
//-----------------------------------------------------------------------------
bool ParseCommandLineArg( const char *pCmdLine, const char* pKey, char* pValueBuff = NULL, int valueBuffSize = 0 )
{
	int keyLen = (int)strlen( pKey );
	const char* pArg = pCmdLine;
	for ( ;; )
	{
		// scan for match
		pArg = strstr( (char*)pArg, pKey );
		if ( !pArg )
		{
			return false;
		}
		
		// found, but could be a substring
		if ( pArg[keyLen] == '\0' || pArg[keyLen] == ' ' )
		{
			// exact match
			break;
		}

		pArg += keyLen;
	}

	if ( pValueBuff )
	{
		// caller wants next token
		// skip past key and whitespace
		pArg += keyLen;
		while ( *pArg == ' ' )
		{
			pArg++;
		}

		int i;
		for ( i=0; i<valueBuffSize; i++ )
		{
			pValueBuff[i] = *pArg;
			if ( *pArg == '\0' || *pArg == ' ' )
				break;
			pArg++;
		}
		pValueBuff[i] = '\0';
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// 360 Quick and dirty command line arg stripping.
//-----------------------------------------------------------------------------
void StripCommandLineArg( const char *pCmdLine, char *pNewCmdLine, const char *pStripArg )
{
	// cannot operate in place
	assert( pCmdLine != pNewCmdLine );

	int numTotal = strlen( pCmdLine ) + 1;
	const char* pArg = strstr( pCmdLine, pStripArg );
	if ( !pArg )
	{
		strcpy( pNewCmdLine, pCmdLine );
		return;
	}

	int numDiscard = strlen( pStripArg );
	while ( pArg[numDiscard] && ( pArg[numDiscard] != '-' && pArg[numDiscard] != '+' ) )
	{
		// eat whitespace up to the next argument
		numDiscard++;
	}

	memcpy( pNewCmdLine, pCmdLine, pArg - pCmdLine );
	memcpy( pNewCmdLine + ( pArg - pCmdLine ), (void*)&pArg[numDiscard], numTotal - ( pArg + numDiscard - pCmdLine  ) );

	// ensure we don't leave any trailing whitespace, occurs if last arg is stripped
	int len = strlen( pNewCmdLine );
	while ( len > 0 &&  pNewCmdLine[len-1] == ' ' )
	{
		len--;
	}
	pNewCmdLine[len] = '\0';
}

//-----------------------------------------------------------------------------
// 360 Conditional spew
//-----------------------------------------------------------------------------
void Spew( const char *pFormat, ... )
{
#if defined( _DEBUG )
	char	msg[2048];
	va_list	argptr;

	va_start( argptr, pFormat );
	vsprintf( msg, pFormat, argptr );
	va_end( argptr );

	OutputDebugString( msg );
#endif
}

//-----------------------------------------------------------------------------
// Adheres to possible xbox exclude paths in for -dvddev mode.
//-----------------------------------------------------------------------------
bool IsBinExcluded( const char *pRemotePath, bool *pExcludeAll )
{
	*pExcludeAll = false;
	bool bIsBinExcluded = false;

	// find optional exclusion file
	char szExcludeFile[MAX_PATH];
	sprintf( szExcludeFile, "%s\\xbox_exclude_paths.txt", pRemotePath );
	HANDLE hFile = CreateFile( szExcludeFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		int len = GetFileSize( hFile, NULL );
		if ( len > 0 )
		{
			char *pBuffer = ( char *)malloc( len + 1 );
			memset( pBuffer, 0, len+1 );
			DWORD numBytesRead;
			if ( ReadFile( hFile, pBuffer, len, &numBytesRead, NULL ) )
			{
				strlwr( pBuffer );
				if ( strstr( pBuffer, "\"*\"" ) )
				{
					*pExcludeAll = true;
					bIsBinExcluded = true;
				}
				else if ( strstr( pBuffer, "\"\\bin\"" ) )
				{
					// override file either specifies an exclusion of the root or the bin directory
					bIsBinExcluded = true;
				}
			}
			free( pBuffer );
		}
		CloseHandle( hFile );
	}

	return bIsBinExcluded;
}

//-----------------------------------------------------------------------------
// Get the new entry point and command line
//-----------------------------------------------------------------------------
LauncherMain_t GetLaunchEntryPoint( char *pNewCommandLine )
{
	HMODULE		hModule;
	char		*pCmdLine;

	// determine source of our invocation, internal or external
	// a valid launch payload will have an embedded command line
	// command line could be from internal restart in dev or retail mode
	CXboxLaunch xboxLaunch;
	int payloadSize;
	unsigned int launchID;
	char *pPayload;
	bool bInternalRestart = xboxLaunch.GetLaunchData( &launchID, (void**)&pPayload, &payloadSize );
	if ( !bInternalRestart || !payloadSize || launchID != VALVE_LAUNCH_ID )
	{
		// launch is not ours
		if ( launchID == LAUNCH_DATA_DEMO_ID )
		{
			// data is a demo blob, not ready to handle yet, so ignore
		}

		// could be first time, get command line from system
		pCmdLine = GetCommandLine();
		if ( !stricmp( pCmdLine, "\"default.xex\"" ) )
		{
			// matches retail xex and no arguments, mut be first time retail launch
			pCmdLine = "default.xex -dvd";
#if defined( _MEMTEST )
			pCmdLine = "default.xex -dvd +mat_picmip 2";
#endif
		}
	}
	else
	{
		// get embedded command line from payload
		pCmdLine = pPayload;
	}

	int launchFlags = xboxLaunch.GetLaunchFlags();
#if !defined( _CERT )
	if ( launchFlags & LF_ISDEBUGGING )
	{
		while ( !DmIsDebuggerPresent() )
		{
		}

		Sleep( 1000 );
		Spew( "Resuming debug session.\n" );
	}
#endif

	if ( launchID == VALVE_LAUNCH_ID )
	{
		// unforunately, the xbox erases its internal store upon first fetch
		// must re-establish it so the payload that contains other data (past command line) can be accessed by the game
		// the launch data will be owned by tier0 and supplied to game
		xboxLaunch.SetLaunchData( pPayload, payloadSize, launchFlags );
	}

	// The 360 has no paths and therefore the xex must reside in the same location as the dlls.
	// Only the xex must reside locally, on the box, but the dlls can be mounted from the remote share.
	// Resolve all known implicitly loaded dlls to be explicitly loaded now to allow their remote location.
	const char *pImplicitDLLs[] =
	{
		"tier0_360.dll",
		"vstdlib_360.dll",
		"vxbdm_360.dll",

		// last slot reserved, as dynamic, used to determine which application gets launched
		"???"
	};

	// Corresponds to pImplicitDLLs. A dll load failure is only an error if that dll is tagged as required.
	const bool bDllRequired[] = 
	{
		true,	// tier0
		true,	// vstdlib
		false,	// vxbdm
		true,	// ???
	};

	char gameName[32];
	bool bDoChooser = false;
	if ( !ParseCommandLineArg( pCmdLine, "-game", gameName, sizeof( gameName ) ) )
	{
		// usage of remote share requires a game (default to hl2) 
		// remote share mandates an absolute game path which is detected by the host
		strcpy( gameName, "hl2" );
		bDoChooser = true;
	}
	else
	{
		// sanitize a possible absolute game path back to expected game name
		char *pSlash = strrchr( gameName, '\\' );
		if ( pSlash )
		{
			memcpy( gameName, pSlash+1, strlen( pSlash+1 )+1 );
		}
	}

	char shareName[32];
	if ( !ParseCommandLineArg( pCmdLine, "-share", shareName, sizeof( shareName ) ) )
	{
		// usage of remote share requires a share name for the game folder (default to game) 
		strcpy( shareName, "game" );
	}

	if ( ( xboxLaunch.GetLaunchFlags() & LF_EXITFROMGAME ) && !( xboxLaunch.GetLaunchFlags() & LF_GAMERESTART ) )
	{
		// exiting from a game back to chooser
		bDoChooser = true;
	}

	// If we're restarting from an invite, we're funneling into TF2
	if ( launchFlags & LF_INVITERESTART )
	{
		strcpy( gameName, "tf" );
		bDoChooser = false;
	}

	// resolve which application gets launched
	if ( bDoChooser )
	{
		// goto high level 1 of N game selector
		pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1] = "AppChooser_360.dll"; 
	}
	else
	{
		pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1] = "launcher_360.dll";
	}

	// the base path is the where the game is predominantly anchored
	char basePath[128];
	// a remote path is for development mode only, on the host pc
	char remotePath[128];

#if !defined( _CERT )
	if ( !ParseCommandLineArg( pCmdLine, "-dvd" ) )
	{
		// development mode only, using host pc
		// auto host name detection can be overriden via command line
		char hostName[32];
		if ( !ParseCommandLineArg( pCmdLine, "-host", hostName, sizeof( hostName ) ) )
		{
			// auto detect, the 360 machine name must be <HostPC>_360
			DWORD length = sizeof( hostName );
			HRESULT hr = DmGetXboxName( hostName, &length );
			if ( hr != XBDM_NOERR )
			{
				Spew( "FATAL: Could not get xbox name: %s\n", hostName );
				return NULL;
			}
			char *p = strstr( hostName, "_360" );
			if ( !p )
			{
				Spew( "FATAL: Xbox name must be <HostPC>_360\n" );
				return NULL;
			}
			*p = '\0';
		}

		sprintf( remotePath, "net:\\smb\\%s\\%s", hostName, shareName );

		// network remote shares seem to be buggy, but always manifest as the gamedir being unaccessible
		// validate now, otherwise longer wait until process eventually fails
		char szFullPath[MAX_PATH];
		WIN32_FIND_DATA findData;
		sprintf( szFullPath, "%s\\%s\\*.*", remotePath, gameName );
		HANDLE hFindFile = FindFirstFile( szFullPath, &findData );
		if ( hFindFile == INVALID_HANDLE_VALUE )
		{
			Spew( "*******************************************************************\n" );
			Spew( "FATAL: Access to remote share '%s' on host PC lost. Forcing cold reboot.\n", szFullPath );
			Spew( "FATAL: After reboot completes to dashboard, restart application.\n" );
			Spew( "*******************************************************************\n" );
			DmRebootEx( DMBOOT_COLD, NULL, NULL, NULL );
			return NULL;
		}
		FindClose( hFindFile );
	}
#endif

	char *searchPaths[2];
	searchPaths[0] = basePath;
	int numSearchPaths = 1;

	bool bExcludeAll = false;
	bool bAddRemotePath = false;

	if ( ParseCommandLineArg( pCmdLine, "-dvd" ) )
	{
		// game runs from dvd only 
		strcpy( basePath, "d:" );
	}
	else if ( ParseCommandLineArg( pCmdLine, "-dvddev" ) )
	{
		// dvd development, game runs from dvd and can fall through to access remote path
		// check user configuration for possible \bin exclusion from Xbox HDD
		strcpy( basePath, "d:" );
		
		if ( IsBinExcluded( remotePath, &bExcludeAll ) )
		{
			searchPaths[0] = remotePath;
			numSearchPaths = 1;
		}
		else
		{
			searchPaths[0] = basePath;
			searchPaths[1] = remotePath;
			numSearchPaths = 2;
		}

		if ( bExcludeAll )
		{
			// override, user has excluded everything, game runs from remote path only
			strcpy( basePath, remotePath );
		}
		else
		{
			// -dvddev appends a -remote <remotepath> for the filesystem to detect
			bAddRemotePath = true;
		}
	}
	else
	{
		// game runs from remote path only
		strcpy( basePath, remotePath );
	}

	// load all the dlls specified
	char dllPath[MAX_PATH];
	for ( int i=0; i<ARRAYSIZE( pImplicitDLLs ); i++ )
	{
		hModule = NULL;
		for ( int j = 0; j < numSearchPaths; j++ )
		{
			sprintf( dllPath, "%s\\bin\\%s", searchPaths[j], pImplicitDLLs[i] );
			hModule = LoadLibrary( dllPath );
			if ( hModule )
			{
				break;
			}
		}
		if ( !hModule && bDllRequired[i] )
		{
			Spew( "FATAL: Failed to load dll: '%s'\n", dllPath );
			return NULL;
		}
	}

	char cleanCommandLine[512];
	char tempCommandLine[512];
	StripCommandLineArg( pCmdLine, tempCommandLine, "-basedir" );
	StripCommandLineArg( tempCommandLine, cleanCommandLine, "-game" );

	if ( bAddRemotePath )
	{
		// the remote path is only added for -dvddev mode
		StripCommandLineArg( cleanCommandLine, tempCommandLine, "-remote" );
		sprintf( cleanCommandLine, "%s -remote %s", tempCommandLine, remotePath );
	}

	// set the alternate command line
	sprintf( pNewCommandLine, "%s -basedir %s -game %s\\%s", cleanCommandLine, basePath, basePath, gameName );

	// the 'main' export is guaranteed to be at ordinal 1
	// the library is already loaded, this just causes a lookup that will resolve against the shortname
	const char *pLaunchDllName = pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1];
	hModule = LoadLibrary( pLaunchDllName );
	LauncherMain_t main = (LauncherMain_t)GetProcAddress( hModule, (LPSTR)1 );
	if ( !main )
	{
		Spew( "FATAL: 'LauncherMain' entry point not found in %s\n", pLaunchDllName );
		return NULL;
	}

	return main;
}

//-----------------------------------------------------------------------------
// 360 Application Entry Point.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
	char newCmdLine[512];
	LauncherMain_t newMain = GetLaunchEntryPoint( newCmdLine );
	if ( newMain )
	{
		// 360 has no concept of instances, spoof one 
		newMain( (HINSTANCE)1, (HINSTANCE)0, (LPSTR)newCmdLine, 0 );
	}
}
#endif
