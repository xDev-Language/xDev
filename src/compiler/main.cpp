/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Compiler
 * File......: main.cpp
 *
 */

#include "internal.h"
#include "platform.h"
#include "version.h"

#include "lexer.h"
#include "parser.hpp"
#include "semantic.hpp"
#include "preprocessor.hpp"
#include "llvm.hpp"
#include "dll.hpp"
#include "interface.hpp"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <llvm/ADT/SmallString.h>
#include <llvm/Object/Archive.h>
#include <llvm/Object/ArchiveWriter.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>

#if X_OS_WINDOWS
#include <windows.h>
#include <lld/Common/Driver.h>
LLD_HAS_DRIVER( mingw )
#elif X_OS_LINUX
#include <lld/Common/Driver.h>
LLD_HAS_DRIVER( elf )
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

using X_STRINGS = std::vector<std::string>;
using X_PATHS = std::vector<fs::path>;

/* Temporary files of one build, removed with their folder at the end */
struct X_TEMP_FILES
{
   fs::path directory;
   X_PATHS files;

   bool create( std::string &error )
   {
      llvm::SmallString<256> path;
      std::error_code code;
      const fs::path root = fs::temp_directory_path( code );

      if( code )
      {
         error = code.message();
         return false;
      }

      if( auto created = llvm::sys::fs::createUniqueDirectory( ( root / "xdev" ).string(), path ) )
      {
         error = created.message();
         return false;
      }

      directory = path.str().str();

      return true;
   }

   fs::path object( size_t index )
   {
      files.push_back( directory / ( "module" + std::to_string( index ) + ".o" ) );

      return files.back();
   }

   ~X_TEMP_FILES()
   {
      std::error_code error;

      for( const fs::path &file : files )
         fs::remove( file, error );

      if( !directory.empty() )
         fs::remove( directory, error );
   }
};

/* The command line of one build */
struct X_OPTIONS
{
   X_STRINGS inputs;
   X_STRINGS objects;
   X_STRINGS libraries;
   X_STRINGS includePaths;
   X_STRINGS definitions;
   X_STRINGS libraryPaths;
   X_STRINGS images;
   X_STRINGS icons;
   X_STRINGS interfaces; /* files that may hold class descriptions */
   bool dumpAst = false;
   bool compileOnly = false;
   bool preprocessOnly = false;
   bool syntaxOnly = false;
   bool unusedWarnings = true; /* -Wno-unused: no warnings about unused variables */
   bool assembly = false;
   bool emitLLVM = false;
   bool windowsSubsystem = false;
   bool sharedLibrary = false;  /* --shared: a DLL module */
   bool dynamicRuntime = false; /* --dynamic: a program with libxdev.dll */
   bool archiveOutput = false;
   bool printIr = false;
   std::string outputPath;
   std::string irPath;
   std::string binPath;
   std::string objPath;
   std::string ppoPath;
   fs::path binary;
   fs::path archive;
};

/* One compilation of a PRG file */
struct X_COMPILE
{
   std::string input;
   std::string object;
   bool libraryModule = false;
   bool sharedModule = false;
   bool dumpAst = false;
   bool printIr = false;
   bool preprocessOnly = false;
   std::string irPath;
   std::string ppoPath;
};

/* What the linker gets */
struct X_LINK
{
   std::string object;
   std::string libDir;
   std::string binary;
   X_STRINGS libraries;
   X_STRINGS libraryPaths;
   bool windowsSubsystem = false;
   bool sharedLibrary = false;
   bool dynamicRuntime = false;
};

/* One rule of a .rmk file; state: 0 waiting, 1 being built, 2 built */
struct X_RMK_RULE
{
   std::string target;
   X_STRINGS sources;
   X_STRINGS options;
   int line = 0;
   int state = 0;
};

/* A .rmk file being built */
struct X_RMK
{
   const char *argv0;
   std::string makefile;
   std::vector<X_RMK_RULE> rules;
};

static int s_driver( int argc, char **argv );

/* ---------------------------------------------------------------------------------------------
   Output paths
   --------------------------------------------------------------------------------------------- */

/* <prefix>/lib next to bin/xdev */
static fs::path s_runtimeLibDir( const char *argv0 )
{
#if X_OS_WINDOWS
   std::vector<char> buffer( MAX_PATH );
   DWORD length = GetModuleFileNameA( NULL, buffer.data(), ( DWORD )buffer.size() );

   if( length > 0 && length < buffer.size() )
      return fs::path( std::string( buffer.data(), length ) ).parent_path().parent_path() / "lib";
#endif

   const std::string executable = llvm::sys::fs::getMainExecutable( argv0, ( void * )&s_runtimeLibDir );

   return fs::path( executable ).parent_path().parent_path() / "lib";
}

/* A path without extension gets .exe, or .dll for a module of --shared */
static fs::path s_binaryPath( const std::string &requestedPath, bool sharedLibrary )
{
   fs::path path( requestedPath );

#if X_OS_WINDOWS
   if( !path.has_extension() )
      path += IIF( sharedLibrary, ".dll", ".exe" );
#else
   UNUSED( sharedLibrary );
#endif

   return path;
}

static fs::path s_userLibDir( const char *argv0 )
{
   return s_runtimeLibDir( argv0 ) / "xdev";
}

/* On Windows the extension is compared without regard to case */
static bool s_hasExtension( const std::string &path, const char *expected )
{
   std::string extension = fs::path( path ).extension().string();

#if X_OS_WINDOWS
   for( char &ch : extension )
      ch = ( char )std::tolower( ( unsigned char )ch );
#endif

   return extension == expected;
}

/* ---------------------------------------------------------------------------------------------
   Program icon
   --------------------------------------------------------------------------------------------- */

/* Little-endian numbers of an .ico file */
static unsigned s_word( const std::string &data, size_t at )
{
   const unsigned low = ( unsigned char )data[at];
   const unsigned high = ( unsigned char )data[at + 1];

   return low | ( high << 8 );
}

static unsigned s_dword( const std::string &data, size_t at )
{
   return s_word( data, at ) | ( s_word( data, at + 2 ) << 16 );
}

static void s_put16( std::string &to, unsigned value )
{
   to += ( char )( value & 0xFF );
   to += ( char )( ( value >> 8 ) & 0xFF );
}

static void s_put32( std::string &to, unsigned value )
{
   s_put16( to, value & 0xFFFF );
   s_put16( to, value >> 16 );
}

/* ICONDIR: reserved 0, type 1, the image count, then 16 bytes per image */
static bool s_isIcon( const std::string &data )
{
   if( data.size() < 6 || s_word( data, 0 ) != 0 || s_word( data, 2 ) != 1 )
      return false;

   return s_word( data, 4 ) != 0 && data.size() >= 6 + 16 * ( size_t )s_word( data, 4 );
}

/* One entry of a .res file: a 32-byte header, the bytes, then padding to 4 bytes */
static void s_resourceEntry( std::string &out, unsigned type, unsigned name, unsigned flags, const std::string &bytes )
{
   s_put32( out, ( unsigned )bytes.size() );
   s_put32( out, 32 );
   s_put16( out, 0xFFFF );
   s_put16( out, type );
   s_put16( out, 0xFFFF );
   s_put16( out, name );
   s_put32( out, 0 );
   s_put16( out, flags );
   s_put16( out, 0 );
   s_put32( out, 0 );
   s_put32( out, 0 );
   out += bytes;

   while( out.size() % 4 != 0 )
      out += '\0';
}

/*
 * The .ico images become the resources RT_ICON 1..n and RT_GROUP_ICON 1 of a .res file linked by LLD.
 * Windows shows the icon in Explorer; GUI programs also use it in their windows and on the taskbar.
 */
static bool s_writeIconResource( const std::string &icon, const std::string &resource, std::string &error )
{
   std::ifstream file( icon, std::ios::binary );
   const std::string data( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
   std::string output;
   std::string group;
   unsigned count;
   std::ofstream out;

   if( !s_isIcon( data ) )
   {
      error = "plik nie jest ikona .ico: " + icon;
      return false;
   }

   count = s_word( data, 4 );
   s_resourceEntry( output, 0, 0, 0, std::string() );
   s_put16( group, 0 );
   s_put16( group, 1 );
   s_put16( group, count );

   for( unsigned i = 0; i < count; ++i )
   {
      const size_t at = 6 + 16 * ( size_t )i;
      const size_t size = s_dword( data, at + 8 );
      const size_t offset = s_dword( data, at + 12 );

      if( offset > data.size() || size > data.size() - offset )
      {
         error = "uszkodzona ikona .ico: " + icon;
         return false;
      }

      s_resourceEntry( output, 3, i + 1, 0x1010, data.substr( offset, size ) );
      group += data.substr( at, 12 );
      s_put16( group, i + 1 );
   }

   s_resourceEntry( output, 14, 1, 0x1030, group );

   out.open( resource, std::ios::binary );
   out.write( output.data(), ( std::streamsize )output.size() );

   if( !out )
   {
      error = "nie mozna zapisac zasobow ikony: " + resource;
      return false;
   }

   return true;
}

/* ---------------------------------------------------------------------------------------------
   Libraries
   --------------------------------------------------------------------------------------------- */

/* A static library of the objects; a repeated file name gets the prefix memberN- */
static bool s_writeArchive( const std::string &archivePath, const X_STRINGS &objectPaths, std::string &error )
{
   const auto mode = llvm::SymtabWritingMode::NormalSymtab;
   const auto kind = llvm::object::Archive::K_GNU;
   std::vector<llvm::NewArchiveMember> members;
   X_STRINGS memberNames;

   /* MemberName points into memberNames, which may not grow */
   members.reserve( objectPaths.size() );
   memberNames.reserve( objectPaths.size() );

   for( size_t i = 0; i < objectPaths.size(); ++i )
   {
      const std::string &path = objectPaths[i];
      auto member = llvm::NewArchiveMember::getFile( path, true );
      std::string memberName = fs::path( path ).filename().string();

      if( !member )
      {
         error = "nie mozna dodac obiektu do archiwum: " + path + ": " + llvm::toString( member.takeError() );
         return false;
      }

      if( std::find( memberNames.begin(), memberNames.end(), memberName ) != memberNames.end() )
         memberName = "member" + std::to_string( i ) + "-" + memberName;

      memberNames.push_back( std::move( memberName ) );
      member->MemberName = memberNames.back();
      members.push_back( std::move( *member ) );
   }

   if( llvm::Error written = llvm::writeArchive( archivePath, members, mode, kind, true, false ) )
   {
      error = "nie mozna utworzyc archiwum: " + archivePath + ": " + llvm::toString( std::move( written ) );
      return false;
   }

   return true;
}

/* An import library of a DLL: name.dll.a */
static bool s_isImportLibrary( const fs::path &path )
{
   return s_hasExtension( path.string(), ".a" ) && s_hasExtension( path.stem().string(), ".dll" );
}

/* The import library libname.dll.a of make dynamic when it lies next to libname.a, otherwise the library itself */
static std::string s_importLibrary( const std::string &library )
{
   const fs::path imported = fs::path( library ).replace_extension( ".dll.a" );
   std::error_code code;

   if( fs::is_regular_file( imported, code ) )
      return imported.string();

   return library;
}

/* The static .a files of <prefix>/lib/xdev, sorted; a missing folder means none */
static bool s_readUserLibraries( const char *argv0, X_STRINGS &libraries, std::string &error )
{
   const fs::path directory = s_userLibDir( argv0 );
   std::error_code code;

   if( !fs::exists( directory, code ) )
   {
      if( code )
      {
         error = "nie mozna sprawdzic katalogu bibliotek uzytkownika: " + code.message();
         return false;
      }

      return true;
   }

   for( fs::directory_iterator entry( directory, code ), end; entry != end && !code; entry.increment( code ) )
   {
      const fs::path &path = entry->path();

      if( entry->is_regular_file( code ) && !code && s_hasExtension( path.string(), ".a" ) && !s_isImportLibrary( path ) )
         libraries.push_back( path.string() );
   }

   if( code )
   {
      error = "nie mozna odczytac katalogu bibliotek uzytkownika: " + code.message();
      return false;
   }

   std::sort( libraries.begin(), libraries.end() );

   return true;
}

/* A global function defined here, not only referenced; broken symbols are skipped */
static bool s_definedFunction( const llvm::object::SymbolRef &symbol, std::string &name )
{
   const uint32_t external = llvm::object::SymbolRef::SF_Undefined | llvm::object::SymbolRef::SF_Weak;
   auto flags = symbol.getFlags();
   auto type = symbol.getType();
   auto symbolName = symbol.getName();

   if( !flags || !type || !symbolName )
   {
      if( !flags )
         llvm::consumeError( flags.takeError() );

      if( !type )
         llvm::consumeError( type.takeError() );

      if( !symbolName )
         llvm::consumeError( symbolName.takeError() );

      return false;
   }

   if( ( *flags & llvm::object::SymbolRef::SF_Global ) == 0 || ( *flags & external ) != 0 )
      return false;

   if( *type != llvm::object::SymbolRef::ST_Function || symbolName->empty() )
      return false;

   name = symbolName->str();

   return true;
}

/* The functions an installed library defines */
static bool s_libraryFunctions( const std::string &library, std::set<std::string> &functions, std::string &error )
{
   auto buffer = llvm::MemoryBuffer::getFile( library );

   if( !buffer )
   {
      error = "nie mozna odczytac biblioteki uzytkownika '" + library + "': " + buffer.getError().message();
      return false;
   }

   auto archive = llvm::object::Archive::create( ( *buffer )->getMemBufferRef() );
   if( !archive )
   {
      error = "niepoprawna biblioteka uzytkownika '" + library + "': " + llvm::toString( archive.takeError() );
      return false;
   }

   llvm::Error childError = llvm::Error::success();

   for( const auto &child : ( *archive )->children( childError ) )
   {
      auto binary = child.getAsBinary();
      std::string name;

      if( !binary )
      {
         llvm::consumeError( binary.takeError() );
         continue;
      }

      const auto *object = llvm::dyn_cast<llvm::object::ObjectFile>( binary->get() );
      if( !object )
         continue;

      for( const auto &symbol : object->symbols() )
      {
         if( s_definedFunction( symbol, name ) )
            functions.insert( name );
      }
   }

   if( childError )
   {
      error = "nie mozna odczytac zawartosci biblioteki uzytkownika '" + library + "': ";
      error += llvm::toString( std::move( childError ) );
      return false;
   }

   return true;
}

/* The same function in two installed libraries is an error */
static bool s_checkUserLibraryConflicts( const X_STRINGS &libraries, std::string &error )
{
   std::map<std::string, std::string> owners;

   for( const std::string &library : libraries )
   {
      std::set<std::string> functions;

      if( !s_libraryFunctions( library, functions, error ) )
         return false;

      for( const std::string &function : functions )
      {
         const auto found = owners.find( function );

         if( found != owners.end() )
         {
            error = "blad linkera: funkcja '" + function + "' jest zdefiniowana w obu bibliotekach uzytkownika:\n  ";
            error += found->second + "\n  " + library;
            return false;
         }

         owners.emplace( function, library );
      }
   }

   return true;
}

/* xdev install --list */
static int s_listLibraries( const char *argv0 )
{
   X_STRINGS libraries;
   std::string error;

   if( !s_readUserLibraries( argv0, libraries, error ) )
   {
      std::cerr << error << "\n";
      return 2;
   }

   for( const std::string &library : libraries )
      std::cout << fs::path( library ).filename().string() << "\n";

   return 0;
}

/* xdev install file.a: the library is copied to <prefix>/lib/xdev */
static int s_installLibrary( const char *argv0, const char *file )
{
   const fs::path directory = s_userLibDir( argv0 );
   const fs::path source = file;
   const fs::path target = directory / source.filename();
   std::error_code code;

   if( !fs::is_regular_file( source, code ) || code )
   {
      std::cerr << "nie znaleziono biblioteki: " << source.string() << "\n";
      return 2;
   }

   fs::create_directories( directory, code );
   if( code )
   {
      std::cerr << "nie mozna utworzyc katalogu bibliotek uzytkownika: " << code.message() << "\n";
      return 2;
   }

   if( !fs::equivalent( source, target, code ) )
   {
      code.clear();
      fs::copy_file( source, target, fs::copy_options::overwrite_existing, code );

      if( code )
      {
         std::cerr << "nie mozna zainstalowac biblioteki: " << code.message() << "\n";
         return 2;
      }
   }

   std::cout << "Zainstalowano '" << target.string() << "'.\n";

   return 0;
}

/* xdev uninstall name: name, name.a or libname.a in <prefix>/lib/xdev, with its import library name.dll.a */
static int s_uninstallLibrary( const char *argv0, const char *library )
{
   const fs::path directory = s_userLibDir( argv0 );
   fs::path name = library;
   fs::path target;
   fs::path imported;
   std::error_code code;

   if( name.empty() || name.has_parent_path() || name.has_root_path() )
   {
      std::cerr << "nazwa biblioteki nie moze zawierac sciezki\n";
      return 2;
   }

   if( !name.has_extension() )
   {
      const fs::path exact = directory / ( name.string() + ".a" );

      if( fs::is_regular_file( exact, code ) && !code )
         name += ".a";
      else
         name = "lib" + name.string() + ".a";
   }

   if( !s_hasExtension( name.string(), ".a" ) )
   {
      std::cerr << "nazwa biblioteki musi wskazywac plik .a\n";
      return 2;
   }

   target = directory / name;
   if( !fs::remove( target, code ) )
   {
      if( code )
         std::cerr << "nie mozna usunac biblioteki: " << code.message() << "\n";
      else
         std::cerr << "biblioteka nie jest zainstalowana: " << target.string() << "\n";

      return 2;
   }

   std::cout << "Odinstalowano '" << target.string() << "'.\n";

   /* The import library of make dynamic goes with the library */
   imported = fs::path( target ).replace_extension( ".dll.a" );
   if( fs::remove( imported, code ) )
      std::cout << "Odinstalowano '" << imported.string() << "'.\n";
   else if( code )
   {
      std::cerr << "nie mozna usunac biblioteki importu: " << code.message() << "\n";
      return 2;
   }

   return 0;
}

/* xdev install and xdev uninstall; -1 for any other command */
static int s_manageUserLibraries( int argc, char **argv )
{
   const std::string command = argv[1];

   if( command == "install" && argc == 3 && std::string( argv[2] ) == "--list" )
      return s_listLibraries( argv[0] );

   if( command == "install" )
   {
      if( argc != 3 || !s_hasExtension( argv[2], ".a" ) )
      {
         std::cerr << "uzycie: xdev install <plik.a>\n";
         return 2;
      }

      return s_installLibrary( argv[0], argv[2] );
   }

   if( command == "uninstall" )
   {
      if( argc != 3 )
      {
         std::cerr << "uzycie: xdev uninstall <nazwa>\n";
         return 2;
      }

      return s_uninstallLibrary( argv[0], argv[2] );
   }

   return -1;
}

/* ---------------------------------------------------------------------------------------------
   Linking
   --------------------------------------------------------------------------------------------- */

#if X_OS_WINDOWS || X_OS_LINUX
static bool s_runLld( const std::vector<const char *> &args, const lld::DriverDef &driver, std::string &error )
{
   const std::vector<lld::DriverDef> drivers{ driver };
   std::string outText;
   std::string errText;
   llvm::raw_string_ostream outStream( outText );
   llvm::raw_string_ostream errStream( errText );
   const lld::Result result = lld::lldMain( args, outStream, errStream, drivers );

   if( result.retCode != 0 )
   {
      error = "LLD nie utworzyl programu: " + errText;
      return false;
   }

   return true;
}
#endif

#if X_OS_WINDOWS
/* The manifest of every program: Common Controls 6 and the active code page UTF-8 */
static const char s_manifest[] =
   "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
   "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n"
   "  <dependency>\n"
   "    <dependentAssembly>\n"
   "      <assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\"\n"
   "                        processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>\n"
   "    </dependentAssembly>\n"
   "  </dependency>\n"
   "  <application xmlns=\"urn:schemas-microsoft-com:asm.v3\">\n"
   "    <windowsSettings>\n"
   "      <activeCodePage xmlns=\"http://schemas.microsoft.com/SMI/2019/WindowsSettings\">"
   "UTF-8</activeCodePage>\n"
   "    </windowsSettings>\n"
   "  </application>\n"
   "</assembly>\n";

/* The Windows libraries linked after the group */
static const char *s_windowsLibraries[] = {
   "-ladvapi32", "-ldinput8", "-lgdi32", "-limm32", "-lole32", "-loleaut32", "-lsetupapi",
   "-lshell32", "-luser32", "-luuid", "-lversion", "-lwinmm", "-lkernel32"
};

static bool s_writeManifest( const fs::path &path, std::string &error )
{
   std::ofstream file( path, std::ios::binary );

   file.write( s_manifest, sizeof( s_manifest ) - 1 );
   file.close();

   if( !file )
   {
      error = "nie mozna zapisac manifestu UTF-8: " + path.string();
      return false;
   }

   return true;
}

/* The MinGW runtime; builtins is the compiler-rt library */
static void s_addMingwRuntime( std::vector<const char *> &args, const char *builtins )
{
   args.push_back( "-lmingw32" );
   args.push_back( builtins );
   args.push_back( "-lunwind" );
   args.push_back( "-lmoldname" );
   args.push_back( "-lmingwex" );
   args.push_back( "-lmsvcrt" );
}

/* The folder of the C compiler: the parent of the bin folder of clang.exe found on PATH */
static bool s_compilerDir( fs::path &directory, std::string &error )
{
   std::vector<char> buffer( MAX_PATH );
   DWORD length = SearchPathA( NULL, "clang.exe", NULL, ( DWORD )buffer.size(), buffer.data(), NULL );

   if( length == 0 || length >= buffer.size() )
   {
      error = "nie znaleziono clang.exe w PATH; dodaj do PATH katalog bin srodowiska MSYS2 clang64";
      return false;
   }

   directory = fs::path( std::string( buffer.data(), length ) ).parent_path().parent_path();
   return true;
}

/* compiler-rt is in lib/clang/<version>/lib/windows; the version is the one folder in lib/clang */
static bool s_builtinsLibrary( const fs::path &compilerDir, std::string &builtins, std::string &error )
{
   const fs::path clangDir = compilerDir / "lib" / "clang";
   std::error_code code;

   for( const fs::directory_entry &entry : fs::directory_iterator( clangDir, code ) )
   {
      const fs::path path = entry.path() / "lib" / "windows" / "libclang_rt.builtins-x86_64.a";

      if( fs::is_regular_file( path ) )
      {
         builtins = path.string();
         return true;
      }
   }

   error = "brak libclang_rt.builtins-x86_64.a w " + clangDir.string();
   return false;
}

/* LLD MinGW with the start files and libraries of the C compiler. A program gets the UTF-8 manifest; a module
   of --shared gets the DLL start file, exports only its public functions and writes its import library.
   --shared and --dynamic link libxdev.dll through libxdev.dll.a instead of libxdev.a. --gc-sections leaves out
   every function the program does not reach from main, the constructors, the exports and the REQUEST anchors */
static bool s_linkExecutable( const X_LINK &link, X_TEMP_FILES &temporary, std::string &error )
{
   const fs::path manifestPath = temporary.directory / "utf8.manifest";
   const std::string manifestInput = "--Xlink=/manifestinput:" + manifestPath.string();
   const std::string libDirFlag = "-L" + link.libDir;
   const char *runtimeName = IIF( link.dynamicRuntime, "libxdev.dll.a", "libxdev.a" );
   const std::string runtimeLib = ( fs::path( link.libDir ) / runtimeName ).string();
   const std::string importLib = link.binary + ".a";
   fs::path compilerDir;
   std::string crt2;
   std::string crtBegin;
   std::string crtEnd;
   std::string builtins;
   std::string compilerLibFlag;
   X_STRINGS searchPaths;
   std::vector<const char *> args;
   std::error_code code;

   if( link.dynamicRuntime && !fs::is_regular_file( runtimeLib, code ) )
   {
      error = "brak " + runtimeLib + "; zbuduj xDev przez make dynamic";
      return false;
   }

   if( !s_compilerDir( compilerDir, error ) || !s_builtinsLibrary( compilerDir, builtins, error ) )
      return false;

   crt2 = ( compilerDir / "lib" / IIF( link.sharedLibrary, "dllcrt2.o", "crt2.o" ) ).string();
   crtBegin = ( compilerDir / "lib" / "crtbegin.o" ).string();
   crtEnd = ( compilerDir / "lib" / "crtend.o" ).string();
   compilerLibFlag = "-L" + ( compilerDir / "lib" ).string();

   for( const std::string &path : link.libraryPaths )
      searchPaths.push_back( "-L" + path );

   searchPaths.push_back( "-L." );

   args = { "ld.lld", "-m", "i386pep", "-Bdynamic", "--gc-sections", "-o", link.binary.c_str(), crt2.c_str(),
            crtBegin.c_str() };

   if( link.sharedLibrary )
   {
      args.push_back( "--shared" );
      args.push_back( "--exclude-all-symbols" );
      args.push_back( "--out-implib" );
      args.push_back( importLib.c_str() );
   }
   else
   {
      temporary.files.push_back( manifestPath );

      if( !s_writeManifest( manifestPath, error ) )
         return false;

      args.push_back( "--Xlink=/manifest:embed" );
      args.push_back( manifestInput.c_str() );
      args.push_back( "--subsystem" );
      args.push_back( IIF( link.windowsSubsystem, "windows", "console" ) );
   }

   for( const std::string &path : searchPaths )
      args.push_back( path.c_str() );

   args.push_back( libDirFlag.c_str() );
   args.push_back( compilerLibFlag.c_str() );
   args.push_back( link.object.c_str() );
   args.push_back( "--start-group" );

   for( const std::string &library : link.libraries )
      args.push_back( library.c_str() );

   args.push_back( runtimeLib.c_str() );
   args.push_back( "-lc++" );
   args.push_back( "-lc++abi" );
   args.push_back( "--end-group" );
   args.push_back( "-lm" );
   s_addMingwRuntime( args, builtins.c_str() );

   for( const char *library : s_windowsLibraries )
      args.push_back( library );

   s_addMingwRuntime( args, builtins.c_str() );
   args.push_back( "-lkernel32" );
   args.push_back( crtEnd.c_str() );

   return s_runLld( args, { lld::MinGW, &lld::mingw::link }, error );
}
#elif X_OS_LINUX
/* Start files and lists written when the toolchain was built */
static const char *s_toolchainFiles[] = {
   "crt1.o", "crti.o", "crtbegin.o", "crtend.o", "crtn.o", "interpreter", "libraries", "search-paths"
};

static bool s_checkToolchain( const fs::path &toolchain, std::string &error )
{
   for( const char *name : s_toolchainFiles )
   {
      if( !fs::is_regular_file( toolchain / name ) )
      {
         error = "brak pliku toolchaina: " + ( toolchain / name ).string();
         return false;
      }
   }

   return true;
}

/* The path of the ELF loader of the system, from toolchain/interpreter */
static bool s_readInterpreter( const fs::path &toolchain, std::string &interpreter, std::string &error )
{
   std::ifstream file( toolchain / "interpreter", std::ios::binary );

   interpreter.assign( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );

   if( !interpreter.empty() && interpreter.back() == '\0' )
      interpreter.pop_back();

   const bool readable = file && !interpreter.empty() && interpreter.find( '\0' ) == std::string::npos;

   if( !readable || !fs::path( interpreter ).is_absolute() || !fs::is_regular_file( interpreter ) )
   {
      error = "brak lub niepoprawna sciezka systemowego loadera ELF w " + ( toolchain / "interpreter" ).string();
      return false;
   }

   return true;
}

/* -L for every folder in toolchain/search-paths */
static bool s_toolchainSearchPaths( const fs::path &toolchain, X_STRINGS &arguments, std::string &error )
{
   const fs::path list = toolchain / "search-paths";
   std::ifstream file( list );
   std::string path;

   if( !file )
   {
      error = "nie mozna otworzyc sciezek bibliotek toolchaina: " + list.string();
      return false;
   }

   while( std::getline( file, path ) )
   {
      if( !path.empty() )
         arguments.push_back( "-L" + path );
   }

   if( file.bad() )
   {
      error = "nie mozna odczytac sciezek bibliotek toolchaina: " + list.string();
      return false;
   }

   return true;
}

/* The runtime libraries named in toolchain/libraries; there is at least one */
static bool s_toolchainLibraries( const fs::path &toolchain, X_STRINGS &arguments, std::string &error )
{
   const fs::path list = toolchain / "libraries";
   std::ifstream file( list );
   std::string name;
   size_t count = 0;

   while( std::getline( file, name ) )
   {
      if( name.empty() || fs::path( name ).has_parent_path() || !fs::is_regular_file( toolchain / name ) )
      {
         error = "brak lub niepoprawna biblioteka toolchaina: " + name;
         return false;
      }

      arguments.push_back( ( toolchain / name ).string() );
      ++count;
   }

   if( file.bad() || count == 0 )
   {
      error = "nie mozna odczytac bibliotek toolchaina: " + list.string();
      return false;
   }

   return true;
}

/* LLD ELF with the toolchain of lib/toolchain; --gc-sections leaves out the functions the program does not reach */
static bool s_linkExecutable( const X_LINK &link, X_TEMP_FILES &, std::string &error )
{
   const fs::path toolchain = fs::path( link.libDir ) / "toolchain";
   std::string interpreter;
   X_STRINGS arguments;
   std::vector<const char *> args;

   if( !s_checkToolchain( toolchain, error ) || !s_readInterpreter( toolchain, interpreter, error ) )
      return false;

   arguments = { "ld.lld", "--eh-frame-hdr", "--gc-sections", "--dynamic-linker", interpreter, "-o", link.binary };
   arguments.push_back( ( toolchain / "crt1.o" ).string() );
   arguments.push_back( ( toolchain / "crti.o" ).string() );
   arguments.push_back( ( toolchain / "crtbegin.o" ).string() );

   for( const std::string &path : link.libraryPaths )
      arguments.push_back( "-L" + path );

   arguments.push_back( "-L." );
   arguments.push_back( "-L" + link.libDir );
   arguments.push_back( "-L" + toolchain.string() );

   if( !s_toolchainSearchPaths( toolchain, arguments, error ) )
      return false;

   arguments.push_back( link.object );
   arguments.push_back( "--start-group" );
   arguments.insert( arguments.end(), link.libraries.begin(), link.libraries.end() );
   arguments.push_back( ( fs::path( link.libDir ) / "libxdev.a" ).string() );
   arguments.push_back( "--as-needed" );

   if( !s_toolchainLibraries( toolchain, arguments, error ) )
      return false;

   arguments.push_back( interpreter );
   arguments.push_back( "--no-as-needed" );
   arguments.push_back( "--end-group" );
   arguments.push_back( ( toolchain / "crtend.o" ).string() );
   arguments.push_back( ( toolchain / "crtn.o" ).string() );

   for( const std::string &argument : arguments )
      args.push_back( argument.c_str() );

   return s_runLld( args, { lld::Gnu, &lld::elf::link }, error );
}
#else
/* Other systems: clang++ links the program */
static bool s_linkExecutable( const X_LINK &link, X_TEMP_FILES &, std::string &error )
{
   const std::string libDirFlag = "-L" + link.libDir;
   const std::string runtimeLib = ( fs::path( link.libDir ) / "libxdev.a" ).string();
   X_STRINGS searchPaths;
   std::vector<char *> args;
   pid_t pid;
   int status;

   args.push_back( const_cast<char *>( "clang++" ) );
   args.push_back( const_cast<char *>( link.object.c_str() ) );

   for( const std::string &path : link.libraryPaths )
      searchPaths.push_back( "-L" + path );

   for( const std::string &path : searchPaths )
      args.push_back( const_cast<char *>( path.c_str() ) );

   args.push_back( const_cast<char *>( "-L." ) );
   args.push_back( const_cast<char *>( libDirFlag.c_str() ) );
   args.push_back( const_cast<char *>( "-Wl,--start-group" ) );

   for( const std::string &library : link.libraries )
      args.push_back( const_cast<char *>( library.c_str() ) );

   args.push_back( const_cast<char *>( runtimeLib.c_str() ) );
   args.push_back( const_cast<char *>( "-Wl,--end-group" ) );
   args.push_back( const_cast<char *>( "-lm" ) );
   args.push_back( const_cast<char *>( "-o" ) );
   args.push_back( const_cast<char *>( link.binary.c_str() ) );
   args.push_back( NULL );

   pid = fork();

   if( pid < 0 )
   {
      error = "nie mozna uruchomic clang++";
      return false;
   }

   if( pid == 0 )
   {
      execvp( args[0], args.data() );
      _exit( 127 );
   }

   if( waitpid( pid, &status, 0 ) < 0 || !WIFEXITED( status ) || WEXITSTATUS( status ) != 0 )
   {
      error = "clang++ nie utworzyl programu";
      return false;
   }

   return true;
}
#endif

/* ---------------------------------------------------------------------------------------------
   Standard include directory
   --------------------------------------------------------------------------------------------- */

/* <prefix>/include/xdev, next to bin/xdev */
static void s_addStandardIncludePath( const char *argv0, X_STRINGS &includePaths )
{
#if X_OS_WINDOWS
   std::wstring modulePath( MAX_PATH, L'\0' );
   DWORD length = 0;
   fs::path include;
   std::error_code code;

   UNUSED( argv0 );

   for( ;; )
   {
      length = GetModuleFileNameW( nullptr, &modulePath[0], ( DWORD )modulePath.size() );

      if( length == 0 )
         return;

      if( length < modulePath.size() )
         break;

      modulePath.resize( modulePath.size() * 2 );
   }

   modulePath.resize( length );
   include = ( fs::path( modulePath ).parent_path() / ".." / "include" / "xdev" ).lexically_normal();
#else
   const fs::path include = s_runtimeLibDir( argv0 ).parent_path() / "include" / "xdev";
   std::error_code code;
#endif

   if( fs::is_directory( include, code ) && !code )
      includePaths.push_back( include.string() );
}

/* ---------------------------------------------------------------------------------------------
   Compilation
   --------------------------------------------------------------------------------------------- */

/* The .ppo file may not replace the source or a header */
static bool s_writePpo( const std::string &path, const X_PREPROCESSED &source, std::string &error )
{
   std::error_code code;
   std::ofstream output;

   if( fs::exists( path, code ) )
   {
      for( const std::string &file : source.files )
      {
         if( fs::equivalent( path, file, code ) )
         {
            error = "plik .ppo nie moze zastapic zrodla ani naglowka: " + file;
            return false;
         }

         if( code )
            break;
      }
   }

   if( code )
   {
      error = code.message();
      return false;
   }

   output.open( path, std::ios::binary | std::ios::trunc );
   output << source.text;
   output.close();

   if( !output )
   {
      error = "nie mozna zapisac: " + path;
      return false;
   }

   return true;
}

/* #stdout messages go to stdout, warnings to stderr; -E -o file writes the .ppo file */
static bool s_preprocess( const X_COMPILE &job, const X_OPTIONS &options, X_PREPROCESSED &source, std::string &error )
{
   const bool processed = x_preprocess( job.input, options.includePaths, options.definitions, source, error );

   for( const std::string &message : source.messages )
      std::cout << message << '\n';

   for( const std::string &warning : source.warnings )
      std::cerr << warning << '\n';

   if( !processed )
      return false;

   if( job.ppoPath.empty() )
      return true;

   return s_writePpo( job.ppoPath, source, error );
}

/* Every error of the parser as file:line:column: error: message */
static bool s_parse( const X_TOKLIST &tokens, const X_PREPROCESSED &source, X_PROGRAM &program, std::string &error )
{
   X_PARSER parser( tokens );
   std::vector<std::string> errors;

   for( const X_CLASS_DECL &cls : program.importedClasses )
      parser.addClassType( cls.name );

   if( parser.parse( program, error ) )
      return true;

   for( const X_DIAGNOSTIC &diagnostic : parser.diagnostics() )
      errors.push_back( source.location( diagnostic.line, diagnostic.column ) + ": error: " + diagnostic.message );

   error = x_joinLines( errors );

   return false;
}

/* LLVM IR, then the object file */
static bool s_generate( const X_COMPILE &job, const X_PROGRAM &program, const X_LOCATION &location, std::string &error )
{
   X_LLVM backend;

   if( !backend.generate( program, error, job.libraryModule, location, job.sharedModule ) )
      return false;

   if( job.printIr )
      std::cout << backend.ir();

   if( !job.irPath.empty() && !backend.writeIR( job.irPath, error ) )
      return false;

   return job.object.empty() || backend.writeObject( job.object, error );
}

/* Preprocessor, lexer, parser, semantic analysis, then LLVM; -E, -fsyntax-only and -ast-dump stop earlier */
static bool s_compileSource( const X_COMPILE &job, const X_OPTIONS &options, std::string &error )
{
   X_PREPROCESSED source;
   X_PROGRAM program;
   X_TOKLIST tokens;
   X_STRINGS warnings;
   bool parsed;
   const X_LOCATION location = [&]( int line ) { return source.location( line ); };

   if( !s_preprocess( job, options, source, error ) )
      return false;

   if( job.preprocessOnly )
   {
      if( job.ppoPath.empty() )
         std::cout << source.text;

      return true;
   }

   tokens = x_lex( source.text.c_str() );
   if( tokens.error != nullptr )
   {
      error = source.location( tokens.errorLine, tokens.errorColumn ) + ": error: " + tokens.error;
      x_toklistFree( &tokens );
      return false;
   }

   if( !x_classInterfaceRead( options.interfaces, program, error ) )
   {
      x_toklistFree( &tokens );
      error = "blad opisu klas: " + error;
      return false;
   }

   parsed = s_parse( tokens, source, program, error );
   x_toklistFree( &tokens );

   if( !parsed )
      return false;

   if( !x_analyzeProgram( program, error, job.libraryModule, location, IIF( options.unusedWarnings, &warnings, nullptr ) ) )
      return false;

   for( const std::string &warning : warnings )
      std::cerr << warning << '\n';

   if( job.dumpAst )
      x_astDump( program );

   if( job.object.empty() && !job.printIr && job.irPath.empty() )
      return true;

   return s_generate( job, program, location, error );
}

/* A module of a program or of a library: an object file, no other output */
static X_COMPILE s_moduleJob( const std::string &input, const std::string &object )
{
   X_COMPILE job;

   job.input = input;
   job.object = object;
   job.libraryModule = true;

   return job;
}

/* ---------------------------------------------------------------------------------------------
   Help
   --------------------------------------------------------------------------------------------- */

/* xDev options follow C compiler conventions; only implemented options are listed */
static const char s_helpText[] =
   "\nSkladnia: xdev [opcje] plik.prg [modul.prg | plik.o | biblioteka.a ...]\n"
   "          xdev [opcje] (plik.prg | plik.o) [...] -o biblioteka.a\n"
   "          xdev --shared [opcje] modul.prg [inny.prg ...] [-o modul.dll]\n"
   "          xdev -each [opcje] *.prg\n"
   "          xdev plik.rmk\n"
   "          xdev install <biblioteka.a>\n"
   "          xdev install --list\n"
   "          xdev uninstall <nazwa>\n"
   "\nPliki:\n"
   "  plik.prg                Pierwszy plik zrodlowy; zawiera Main i nadaje nazwe wynikowi\n"
   "  modul.prg               Kolejny plik zrodlowy programu\n"
   "  plik.o, biblioteka.a    Plik obiektowy lub biblioteka statyczna do zlinkowania\n"
   "  ikona.ico               Ikona programu Windows; najwyzej jedna\n"
   "  obraz.png, obraz.bmp    Obrazek wbudowany w program pod swoja nazwa\n"
   "  plik.rmk                Plik budowania: wiersze \"cel: pliki [opcje]\", np. -Wno-unused;\n"
   "                          cel powstaje, gdy go brak lub jest starszy od ktoregos pliku\n"
   "\nOpcje:\n"
   "  -h, --help              Wyswietl pomoc\n"
   "  --version               Wyswietl informacje o wersji\n"
   "  -o <plik>               Ustaw plik wynikowy; .a tworzy biblioteke statyczna\n"
   "  -c                      Utworz plik obiektowy bez linkowania\n"
   "  -each                   Zbuduj kazdy plik.prg jako osobny program\n"
   "  -E                      Uruchom tylko preprocesor; wynik na stdout lub do -o <plik>\n"
   "  -fsyntax-only           Sprawdz skladnie bez zapisywania wyniku\n"
   "  -Wno-unused             Nie ostrzegaj o nieuzywanych zmiennych i parametrach\n"
   "  -S -emit-llvm           Zapisz LLVM IR w plik.ll lub -o <plik>\n"
   "  -Xclang -ast-dump       Wyswietl drzewo skladni bez linkowania\n"
   "  -I <katalog>            Dodaj katalog plikow naglowkowych\n"
   "  -D <nazwa>[=<wartosc>]  Zdefiniuj makro preprocesora\n"
   "  -L <katalog>            Dodaj katalog wyszukiwania bibliotek\n"
   "  -l <nazwa>              Dolacz biblioteke o podanej nazwie\n"
   "  --windows               Utworz program Windows GUI bez konsoli\n"
   "  --shared                Utworz modul DLL bez Main\n"
   "  --dynamic               Linkuj ze wspolnym runtime libxdev.dll (xDev zbudowany przez make dynamic)\n"
   "  --                      Zakoncz przetwarzanie opcji\n"
   "\nWynik:\n"
   "  program                 plik.exe na Windows, plik na innych systemach\n"
   "  -c                      plik.o\n"
   "  --shared                modul.dll i biblioteka importu modul.dll.a\n"
   "  -o biblioteka.a         Biblioteka statyczna z plikow .prg i .o\n"
   "\nInne:\n"
   "  <prefix>/lib/xdev       Zainstalowane biblioteki sa dolaczane automatycznie\n"
   "  Unused( zmienna, ... )  W kodzie: zmienne nieuzywane celowo, bez ostrzezenia\n";

static void s_version( std::ostream &output = std::cout )
{
   output << "xDev compiler " X_VER_NUMBER " (r" X_REVISION ")\n";
   output << "Copyright (c) 2025-2026, " X_VER_URL "\n";
}

static void s_help()
{
   s_version();
   std::cout << s_helpText;
}

/* The version once per run, also when -each or a .rmk file starts several builds; a blank line follows */
static void s_banner()
{
   static bool shown = false;

   if( shown )
      return;

   s_version( std::cerr );
   std::cerr << "\n";
   shown = true;
}

/* ---------------------------------------------------------------------------------------------
   Make file .rmk
   --------------------------------------------------------------------------------------------- */

/*
 * An .rmk file contains only "target: files" dependencies; xdev supplies the build commands.
 * An .o target uses one PRG; executables and .a libraries use PRGs, objects and libraries.
 * Headers are dependencies only. Words starting with "-" are joined xdev options (--windows, -Iinclude, -DNAME).
 * Targets are built when missing or older than an input, the .rmk file, the runtime or an automatically linked
 * user library.
 */

/* Runs the driver again in this process with a new command line */
static int s_runDriver( X_STRINGS args )
{
   std::vector<char *> argv;

   for( std::string &arg : args )
      argv.push_back( arg.data() );

   argv.push_back( nullptr );

   return s_driver( ( int )args.size(), argv.data() );
}

/* A name relative to the folder of the .rmk file */
static std::string s_rmkPath( const fs::path &base, const std::string &name )
{
   if( fs::path( name ).is_relative() && !base.empty() )
      return ( base / name ).string();

   return name;
}

/* The colon of "target: files"; a colon followed by a non-space (C:\path) belongs to a name */
static size_t s_rmkColon( const std::string &text )
{
   size_t colon = text.find( ':' );

   while( colon != std::string::npos && colon + 1 < text.size() && !std::isspace( ( unsigned char )text[colon + 1] ) )
      colon = text.find( ':', colon + 1 );

   return colon;
}

/* "target: files options"; the error message, or nullptr */
static const char *s_rmkRule( const std::string &text, const fs::path &base, X_RMK_RULE &rule )
{
   const size_t colon = s_rmkColon( text );
   std::istringstream left;
   std::istringstream right;
   std::string word;
   std::string extra;

   if( text[0] == ' ' || text[0] == '\t' )
      return "polecenia nie sa obslugiwane, xdev sam kompiluje i linkuje";

   if( colon != std::string::npos )
      left.str( text.substr( 0, colon ) );

   if( !( left >> rule.target ) || ( left >> extra ) || rule.target[0] == '.' )
      return "oczekiwano \"cel: pliki\"";

   right.str( text.substr( colon + 1 ) );

   while( right >> word )
   {
      if( word[0] == '-' )
         rule.options.push_back( word );
      else
         rule.sources.push_back( s_rmkPath( base, word ) );
   }

   rule.target = s_rmkPath( base, rule.target );

   return nullptr;
}

/* // starts a comment, \ at the end of a line continues it */
static bool s_rmkRead( const fs::path &path, std::vector<X_RMK_RULE> &rules, std::string &error )
{
   std::ifstream input( path );
   std::string text;
   std::string line;
   int number = 0;
   int start = 0;

   if( !input )
   {
      error = "nie mozna otworzyc: " + path.string();
      return false;
   }

   while( std::getline( input, line ) )
   {
      const size_t comment = line.find( "//" );
      X_RMK_RULE rule;
      const char *message;
      bool more;

      ++number;

      if( comment != std::string::npos )
         line.erase( comment );

      while( !line.empty() && std::isspace( ( unsigned char )line.back() ) )
         line.pop_back();

      if( text.empty() )
         start = number;

      more = !line.empty() && line.back() == '\\';
      if( more )
         line.pop_back();

      text += line;

      if( more )
      {
         text += ' ';
         continue;
      }

      if( text.find_first_not_of( " \t" ) == std::string::npos )
      {
         text.clear();
         continue;
      }

      message = s_rmkRule( text, path.parent_path(), rule );
      text.clear();

      if( message )
      {
         error = path.string() + ":" + std::to_string( start ) + ": " + message;
         return false;
      }

      rule.line = start;
      rules.push_back( rule );
   }

   if( rules.empty() )
   {
      error = path.string() + ": brak celow";
      return false;
   }

   return true;
}

static bool s_rmkBuild( X_RMK &make, size_t index, std::string &error );

/* The rules whose target is this source are built first */
static bool s_rmkDependencies( X_RMK &make, const std::string &source, std::string &error )
{
   const fs::path wanted = fs::path( source ).lexically_normal();

   for( size_t i = 0; i < make.rules.size(); ++i )
   {
      if( fs::path( make.rules[i].target ).lexically_normal() == wanted && !s_rmkBuild( make, i, error ) )
         return false;
   }

   return true;
}

/* Files passed to xdev besides the PRG files */
static bool s_isLinkInput( const std::string &source )
{
   if( s_hasExtension( source, ".o" ) || s_hasExtension( source, ".a" ) )
      return true;

   return s_hasExtension( source, ".png" ) || s_hasExtension( source, ".bmp" ) || s_hasExtension( source, ".ico" );
}

/* A program is built again when the runtime or an installed library is newer */
static bool s_newerLibraries( const char *argv0, fs::file_time_type built, bool &outdated, std::string &error )
{
   const fs::path runtime = s_runtimeLibDir( argv0 ) / "libxdev.a";
   X_STRINGS userLibraries;
   std::error_code code;

   if( fs::is_regular_file( runtime, code ) && fs::last_write_time( runtime, code ) > built )
      outdated = true;

   if( !s_readUserLibraries( argv0, userLibraries, error ) )
      return false;

   for( const std::string &library : userLibraries )
   {
      if( fs::is_regular_file( library, code ) && fs::last_write_time( library, code ) > built )
         outdated = true;
   }

   return true;
}

/* xdev [-c] files options -o target */
static bool s_rmkRun( const char *argv0, const X_RMK_RULE &rule, X_STRINGS args )
{
   if( s_hasExtension( rule.target, ".o" ) )
      args.insert( args.begin(), "-c" );

   args.insert( args.end(), rule.options.begin(), rule.options.end() );
   args.push_back( "-o" );
   args.push_back( rule.target );
   args.insert( args.begin(), argv0 );

   return s_runDriver( args ) == 0;
}

/* The sources first, then the target when it is missing or older than a source */
static bool s_rmkBuild( X_RMK &make, size_t index, std::string &error )
{
   X_RMK_RULE &rule = make.rules[index];
   const std::string where = make.makefile + ":" + std::to_string( rule.line ) + ": ";
   const bool object = s_hasExtension( rule.target, ".o" );
   std::error_code code;
   fs::file_time_type built;
   bool outdated;
   X_STRINGS args;
   size_t programs = 0;

   if( rule.state == 2 )
      return true;

   if( rule.state == 1 )
   {
      error = where + "zaleznosc cykliczna: " + rule.target;
      return false;
   }

   rule.state = 1;
   outdated = !fs::exists( rule.target, code );

   if( !outdated )
      built = fs::last_write_time( rule.target, code );

   if( !outdated && fs::last_write_time( make.makefile, code ) > built )
      outdated = true;

   for( const std::string &source : rule.sources )
   {
      if( !s_rmkDependencies( make, source, error ) )
         return false;

      if( !fs::exists( source, code ) )
      {
         error = where + "brak pliku: " + source;
         return false;
      }

      if( !outdated && fs::last_write_time( source, code ) > built )
         outdated = true;

      /* The PRG files come first, in their order; the first one names the program */
      if( s_hasExtension( source, ".prg" ) )
      {
         args.insert( args.begin() + ( long )programs, source );
         ++programs;
      }
      else if( s_isLinkInput( source ) )
         args.push_back( source );
   }

   if( !outdated && !object && !s_newerLibraries( make.argv0, built, outdated, error ) )
      return false;

   if( programs == 0 || ( object && args.size() != 1 ) )
   {
      if( object )
         error = where + "cel .o wymaga jednego pliku PRG: " + rule.target;
      else
         error = where + "cel wymaga co najmniej jednego pliku PRG: " + rule.target;

      return false;
   }

   if( !outdated )
      std::cerr << "'" << rule.target << "' jest aktualny.\n";
   else if( !s_rmkRun( make.argv0, rule, args ) )
   {
      error = where + "nie zbudowano: " + rule.target;
      return false;
   }

   rule.state = 2;

   return true;
}

static int s_make( const char *argv0, const std::string &makefile )
{
   X_RMK make{ argv0, makefile, {} };
   std::string error;

   if( !s_rmkRead( makefile, make.rules, error ) )
   {
      std::cerr << error << "\n";
      return 2;
   }

   for( size_t i = 0; i < make.rules.size(); ++i )
   {
      if( !s_rmkBuild( make, i, error ) )
      {
         std::cerr << error << "\n";
         return 1;
      }
   }

   return 0;
}

/* ---------------------------------------------------------------------------------------------
   -each
   --------------------------------------------------------------------------------------------- */

/* Options whose value is the next word */
static bool s_takesValue( const std::string &arg )
{
   return arg == "-L" || arg == "-l" || arg == "-D" || arg == "-I" || arg == "-Xclang";
}

/* xdev -each a.prg b.prg ...: every PRG becomes a separate program with the same options */
static int s_each( int argc, char **argv )
{
   X_STRINGS options;
   X_STRINGS programs;
   X_STRINGS failed;

   for( int i = 1; i < argc; ++i )
   {
      const std::string arg = argv[i];

      if( arg == "-each" )
         continue;

      if( arg.compare( 0, 2, "-o" ) == 0 )
      {
         std::cerr << "-each nie przyjmuje -o; kazdy program dostaje nazwe od swojego pliku PRG\n";
         return 2;
      }

      if( s_hasExtension( arg, ".prg" ) )
      {
         programs.push_back( arg );
         continue;
      }

      options.push_back( arg );

      if( s_takesValue( arg ) && i + 1 < argc )
         options.emplace_back( argv[++i] );
   }

   if( programs.empty() )
   {
      std::cerr << "-each wymaga plikow PRG\n";
      return 2;
   }

   for( const std::string &program : programs )
   {
      X_STRINGS args = { argv[0], program };

      args.insert( args.end(), options.begin(), options.end() );

      if( s_runDriver( args ) != 0 )
         failed.push_back( program );
   }

   for( const std::string &program : failed )
      std::cerr << "nie zbudowano: " << program << "\n";

   return IIF( failed.empty(), 0, 1 );
}

/* ---------------------------------------------------------------------------------------------
   Command line
   --------------------------------------------------------------------------------------------- */

/* The message on stderr; exit code 2 */
static int s_usage( const std::string &message )
{
   std::cerr << message << "\n";

   return 2;
}

static bool s_reject( const std::string &message )
{
   std::cerr << message << "\n";

   return false;
}

/* Several PRG files only when a program is built */
static bool s_rejectModes()
{
   return s_reject( "niezgodne opcje; wymagany jeden plik PRG, kilka tylko przy budowie programu; uzyj --help" );
}

/* Options without a value */
static bool s_readFlag( const std::string &arg, X_OPTIONS &options )
{
   if( arg == "-c" )
      options.compileOnly = true;
   else if( arg == "-E" )
      options.preprocessOnly = true;
   else if( arg == "-fsyntax-only" )
      options.syntaxOnly = true;
   else if( arg == "-Wno-unused" )
      options.unusedWarnings = false;
   else if( arg == "-S" )
      options.assembly = true;
   else if( arg == "-emit-llvm" )
      options.emitLLVM = true;
   else if( arg == "--windows" )
      options.windowsSubsystem = true;
   else if( arg == "--shared" )
      options.sharedLibrary = true;
   else if( arg == "--dynamic" )
      options.dynamicRuntime = true;
   else
      return false;

   return true;
}

/* -o file, -L folder, -l name, -D name[=value] and -I folder, also written together: -Iinclude */
static void s_addValue( char option, const std::string &value, X_OPTIONS &options )
{
   switch( option )
   {
   case 'o': options.outputPath = value; break;
   case 'L': options.libraryPaths.push_back( value ); break;
   case 'l': options.libraries.push_back( "-l" + value ); break;
   case 'D': options.definitions.push_back( value ); break;
   case 'I': options.includePaths.push_back( value ); break;
   }
}

/* What a separate value of -o, -L, -l, -D and -I must be */
static const char *s_valueMissing( char option )
{
   switch( option )
   {
   case 'D': return " wymaga nazwy lub NAZWA=wartosc";
   case 'I': return " wymaga katalogu";
   default:  return " wymaga argumentu";
   }
}

/* The next word as the value of an option; nullptr when it is missing or empty */
static const char *s_nextValue( int argc, char **argv, int &i )
{
   if( ++i == argc || !*argv[i] )
      return nullptr;

   return argv[i];
}

/* One option starting with -; -1 when the build goes on, otherwise the exit code */
static int s_readOption( const std::string &arg, int argc, char **argv, int &i, X_OPTIONS &options )
{
   const bool valueOption = std::string( "oLlDI" ).find( arg[1] ) != std::string::npos;
   const char *value;

   if( s_readFlag( arg, options ) )
      return -1;

   if( valueOption && arg.size() > 2 )
   {
      s_addValue( arg[1], arg.substr( 2 ), options );
      return -1;
   }

   if( valueOption )
   {
      value = s_nextValue( argc, argv, i );
      if( !value )
         return s_usage( arg + s_valueMissing( arg[1] ) );

      s_addValue( arg[1], value, options );
      return -1;
   }

   if( arg == "-Xclang" )
   {
      if( ++i == argc || std::string( argv[i] ) != "-ast-dump" )
         return s_usage( "obslugiwane polecenie: -Xclang -ast-dump" );

      options.dumpAst = true;
      return -1;
   }

   if( arg == "--help" || arg == "-h" )
   {
      s_help();
      return 0;
   }

   if( arg == "--version" )
   {
      s_version();
      return 0;
   }

   return s_usage( "nieznana opcja: " + arg );
}

/* -1 when the build goes on, otherwise the exit code; after -- every word is an input */
static int s_readOptions( int argc, char **argv, X_OPTIONS &options )
{
   bool endOptions = false;

   for( int i = 1; i < argc; ++i )
   {
      const std::string arg = argv[i];
      int result;

      if( endOptions || arg.empty() || arg[0] != '-' )
      {
         options.inputs.push_back( arg );
         continue;
      }

      if( arg == "--" )
      {
         endOptions = true;
         continue;
      }

      result = s_readOption( arg, argc, argv, i, options );
      if( result >= 0 )
         return result;
   }

   return -1;
}

/* Objects, libraries, pictures and the icon leave the list of PRG inputs */
static void s_sortInputs( X_OPTIONS &options )
{
   X_STRINGS sources;

   for( const std::string &input : options.inputs )
   {
      const std::string extension = fs::path( input ).extension().string();

      if( options.archiveOutput && ( s_hasExtension( input, ".o" ) || s_hasExtension( input, ".obj" ) ) )
         options.objects.push_back( input );
      else if( extension == ".a" || extension == ".o" || extension == ".obj" || extension == ".lib" )
         options.libraries.push_back( input );
      else if( s_hasExtension( input, ".png" ) || s_hasExtension( input, ".bmp" ) )
         options.images.push_back( input );
      else if( s_hasExtension( input, ".ico" ) )
         options.icons.push_back( input );
      else
         sources.push_back( input );
   }

   options.inputs = sources;
}

/* -c, -E, -fsyntax-only or -ast-dump, and -emit-llvm are one mode each */
static int s_modes( const X_OPTIONS &options )
{
   int modes = 0;

   if( options.compileOnly )
      ++modes;

   if( options.preprocessOnly )
      ++modes;

   if( options.syntaxOnly || options.dumpAst )
      ++modes;

   if( options.emitLLVM )
      ++modes;

   return modes;
}

/* A mode that writes no program */
static bool s_noProgram( const X_OPTIONS &options )
{
   if( options.compileOnly || options.preprocessOnly || options.syntaxOnly )
      return true;

   return options.dumpAst || options.emitLLVM;
}

/* -o library.a: PRG files and objects, no other mode and no libraries */
static bool s_checkArchive( const X_OPTIONS &options, int modes )
{
   const bool empty = options.inputs.empty() && options.objects.empty();

   if( modes != 0 || empty || !options.libraries.empty() || !options.libraryPaths.empty() )
      return s_reject( "niezgodne opcje dla archiwum .a; wymagany co najmniej jeden plik PRG lub obiekt; uzyj --help" );

   for( const std::string &input : options.inputs )
   {
      if( !s_hasExtension( input, ".prg" ) )
      {
         std::cerr << "archiwum .a przyjmuje pliki PRG oraz obiekty .o: " << input << "\n";
         return false;
      }
   }

   return true;
}

/* Options that do not go together */
static bool s_checkOptions( const X_OPTIONS &options )
{
   const bool media = !options.images.empty() || !options.icons.empty();
   const bool runtimeDll = options.sharedLibrary || options.dynamicRuntime;
   const int modes = s_modes( options );

#if !X_OS_WINDOWS
   if( options.windowsSubsystem )
      return s_reject( "--windows jest dostepne tylko dla platformy Windows" );

   if( runtimeDll )
      return s_reject( "--shared i --dynamic sa na razie dostepne tylko dla platformy Windows" );
#endif

   if( options.windowsSubsystem && ( options.archiveOutput || options.assembly || s_noProgram( options ) ) )
      return s_reject( "--windows mozna uzyc tylko przy linkowaniu programu" );

   if( runtimeDll && ( options.archiveOutput || options.assembly || s_noProgram( options ) ) )
      return s_reject( "--shared i --dynamic mozna uzyc tylko przy linkowaniu" );

   if( options.sharedLibrary && options.dynamicRuntime )
      return s_reject( "--dynamic dotyczy programu; modul --shared zawsze uzywa libxdev.dll" );

   if( options.sharedLibrary && options.windowsSubsystem )
      return s_reject( "--windows dotyczy programu, nie modulu --shared" );

   if( options.icons.size() > 1 )
      return s_reject( "program ma jedna ikone .ico" );

   if( media && ( options.archiveOutput || options.sharedLibrary || s_noProgram( options ) ) )
      return s_reject( "obrazki .png, .bmp i ikone .ico wklada sie tylko do programu" );

   if( options.assembly != options.emitLLVM )
      return s_reject( "zapis LLVM IR wymaga -S -emit-llvm; zapis asemblera nie jest obslugiwany" );

   if( options.archiveOutput )
      return s_checkArchive( options, modes );

   /* Several PRG files only when a program is built */
   if( options.inputs.empty() || modes > 1 || ( modes != 0 && options.inputs.size() > 1 ) )
      return s_rejectModes();

   if( modes != 0 && ( !options.libraries.empty() || !options.libraryPaths.empty() ) )
      return s_rejectModes();

   if( ( options.syntaxOnly || options.dumpAst ) && !options.outputPath.empty() )
      return s_rejectModes();

   return true;
}

/* -o, or the name of the first PRG file with the extension */
static std::string s_defaultOutput( const X_OPTIONS &options, const char *extension )
{
   if( !options.outputPath.empty() )
      return options.outputPath;

   return fs::path( options.inputs[0] ).filename().replace_extension( extension ).string();
}

static void s_outputPaths( X_OPTIONS &options )
{
   if( options.compileOnly )
      options.objPath = s_defaultOutput( options, ".o" );
   else if( options.preprocessOnly )
      options.ppoPath = IIF( options.outputPath == "-", std::string(), options.outputPath );
   else if( options.emitLLVM )
      options.irPath = s_defaultOutput( options, ".ll" );
   else if( options.sharedLibrary )
      options.binPath = s_defaultOutput( options, ".dll" );
   else if( !options.archiveOutput && !options.syntaxOnly && !options.dumpAst )
      options.binPath = s_defaultOutput( options, IIF( X_OS_WINDOWS, ".exe", "" ) );

   /* -S -emit-llvm -o - prints the IR */
   options.printIr = options.emitLLVM && options.irPath == "-";
   if( options.printIr )
      options.irPath.clear();

   if( !options.binPath.empty() )
      options.binary = s_binaryPath( options.binPath, options.sharedLibrary );

   if( options.archiveOutput )
      options.archive = options.outputPath;
}

/* Two paths name the same file; on Windows without regard to case */
static bool s_samePath( const fs::path &a, const fs::path &b, std::error_code &code )
{
   std::string left;
   std::string right;

   if( code )
      return false;

   left = fs::weakly_canonical( a, code ).string();
   if( code )
      return false;

   right = fs::weakly_canonical( b, code ).string();
   if( code )
      return false;

#if X_OS_WINDOWS
   for( char &c : left )
      c = ( char )std::tolower( ( unsigned char )c );

   for( char &c : right )
      c = ( char )std::tolower( ( unsigned char )c );
#endif

   return left == right;
}

/* Output i may not replace an input or an earlier output */
static bool s_checkOutput( const X_OPTIONS &options, const X_PATHS &outputs, size_t i, std::error_code &code )
{
   const fs::path &output = outputs[i];

   for( const std::string &input : options.inputs )
   {
      if( s_samePath( output, input, code ) )
         return s_reject( "plik wyjsciowy nie moze zastapic zrodla" );
   }

   for( const std::string &library : options.libraries )
   {
      if( library.compare( 0, 2, "-l" ) != 0 && s_samePath( output, library, code ) )
         return s_reject( "plik wyjsciowy nie moze zastapic biblioteki wejsciowej" );
   }

   for( const std::string &object : options.objects )
   {
      if( s_samePath( output, object, code ) )
         return s_reject( "plik wyjsciowy nie moze zastapic obiektu wejsciowego" );
   }

   for( size_t j = 0; j < i; ++j )
   {
      if( !outputs[j].empty() && s_samePath( output, outputs[j], code ) )
         return s_reject( "pliki wyjsciowe musza miec rozne sciezki" );
   }

   return true;
}

static bool s_checkOutputs( const X_OPTIONS &options )
{
   X_PATHS outputs = { options.binary, options.archive, options.objPath };
   std::error_code code;

   outputs.push_back( options.irPath );
   outputs.push_back( options.ppoPath );

   for( size_t i = 0; i < outputs.size(); ++i )
   {
      if( !outputs[i].empty() && !s_checkOutput( options, outputs, i, code ) )
         return false;
   }

   if( code )
      return s_reject( code.message() );

   return true;
}

/* A running program cannot be replaced; the linker would stop the process without a message */
static bool s_checkBinary( const fs::path &binary )
{
   std::error_code code;
   std::fstream test;

   if( binary.empty() || !fs::exists( binary, code ) )
      return true;

   test.open( binary, std::ios::in | std::ios::out | std::ios::binary );
   if( !test )
   {
      std::cerr << "nie mozna zapisac '" << binary.string();
      std::cerr << "': program jest uruchomiony albo plik jest zablokowany\n";
      return false;
   }

   return true;
}

/* ---------------------------------------------------------------------------------------------
   Build
   --------------------------------------------------------------------------------------------- */

/* -lname: libname.a in the first folder that has it; empty when none does */
static std::string s_findLibrary( const std::string &library, const X_STRINGS &folders )
{
   for( const std::string &path : folders )
   {
      const fs::path candidate = fs::path( path ) / ( "lib" + library.substr( 2 ) + ".a" );
      std::error_code code;

      if( fs::is_regular_file( candidate, code ) )
         return candidate.string();
   }

   return std::string();
}

/* Libraries given as files, and every -lname found as libname.a */
static void s_addLibraryInputs( const X_STRINGS &libraries, const X_STRINGS &search, X_STRINGS &inputs )
{
   for( const std::string &library : libraries )
   {
      std::string found;

      if( library.compare( 0, 2, "-l" ) != 0 )
      {
         inputs.push_back( library );
         continue;
      }

      found = s_findLibrary( library, search );
      if( !found.empty() )
         inputs.push_back( found );
   }
}

/* Files that may hold class descriptions: objects, libraries, installed libraries and the runtime */
static bool s_interfaces( const char *argv0, X_OPTIONS &options, std::string &error )
{
   X_STRINGS search = options.libraryPaths;
   X_STRINGS userLibraries;

   search.push_back( "." );
   options.interfaces = options.objects;
   s_addLibraryInputs( options.libraries, search, options.interfaces );

   if( !s_readUserLibraries( argv0, userLibraries, error ) )
      return false;

   options.interfaces.insert( options.interfaces.end(), userLibraries.begin(), userLibraries.end() );
   options.interfaces.push_back( ( s_runtimeLibDir( argv0 ) / "libxdev.a" ).string() );

   return true;
}

/* -o library.a: every PRG becomes an object of the archive; a PRG sees the classes of the ones before */
static int s_buildArchive( X_OPTIONS &options, X_TEMP_FILES &temporary )
{
   std::string error;

   for( size_t i = 0; i < options.inputs.size(); ++i )
   {
      std::string object;

      std::cerr << "Kompilowanie '" << options.inputs[i] << "'...\n";
      object = temporary.object( i ).string();

      if( !s_compileSource( s_moduleJob( options.inputs[i], object ), options, error ) )
      {
         std::cerr << error << "\nKompilacja nie powiodla sie.\n";
         return 1;
      }

      options.objects.push_back( object );
      options.interfaces.insert( options.interfaces.begin(), object );
   }

   std::cerr << "Tworzenie biblioteki statycznej '" << options.archive.string() << "'... " << std::flush;

   if( !s_writeArchive( options.archive.string(), options.objects, error ) )
   {
      std::cerr << "Niepowodzenie.\n" << error << "\nTworzenie archiwum nie powiodlo sie.\n";
      return 2;
   }

   std::cerr << "Gotowe.\n";

   return 0;
}

static int s_linkFailed( const std::string &error )
{
   std::cerr << "Niepowodzenie.\n" << error << "\nLinkowanie nie powiodlo sie.\n";

   return 2;
}

/* The pictures go into the program as the table x_resources, the icon as a Windows resource */
static bool s_addResources( const X_OPTIONS &options, X_LINK &link, X_TEMP_FILES &temporary, std::string &error )
{
   if( !options.images.empty() )
   {
      const fs::path resources = temporary.directory / "resources.o";

      temporary.files.push_back( resources );

      if( !x_llvmResources( options.images, resources.string(), error ) )
         return false;

      link.libraries.insert( link.libraries.begin(), resources.string() );
   }

#if X_OS_WINDOWS
   if( !options.icons.empty() )
   {
      const fs::path icon = temporary.directory / "icon.res";

      temporary.files.push_back( icon );

      if( !s_writeIconResource( options.icons[0], icon.string(), error ) )
         return false;

      link.libraries.insert( link.libraries.begin(), icon.string() );
   }
#endif

   return true;
}

/* Folders searched for DLLs and -l libraries */
static void s_importSearch( const char *argv0, const X_OPTIONS &options, X_STRINGS &search )
{
   std::error_code code;
   const fs::path currentPath = fs::current_path( code );

   if( !code )
      search.push_back( currentPath.string() );

   search.push_back( fs::path( options.inputs[0] ).parent_path().string() );
   search.push_back( s_runtimeLibDir( argv0 ).string() );
   search.push_back( s_userLibDir( argv0 ).string() );
}

/* link.libraries holds the modules of the program; resources, libraries and DLL imports are added */
static int s_linkProgram( const char *argv0, const X_OPTIONS &options, X_LINK &link, X_TEMP_FILES &temporary )
{
   const char *action = IIF( options.sharedLibrary, "Tworzenie modulu DLL '", "Tworzenie programu '" );
   X_STRINGS importInputs = { link.object };
   X_STRINGS importSearch = options.libraryPaths;
   X_STRINGS imports;
   X_STRINGS userLibraries;
   std::string error;
   bool imported;

   std::cerr << action << options.binary.string() << "'... " << std::flush;

   link.libDir = s_runtimeLibDir( argv0 ).string();
   link.binary = options.binary.string();
   link.libraryPaths = options.libraryPaths;
   link.windowsSubsystem = options.windowsSubsystem;
   link.sharedLibrary = options.sharedLibrary;
   link.dynamicRuntime = options.dynamicRuntime || options.sharedLibrary;
   importInputs.insert( importInputs.end(), link.libraries.begin(), link.libraries.end() );

   if( !s_addResources( options, link, temporary, error ) )
      return s_linkFailed( error );

   link.libraries.insert( link.libraries.end(), options.libraries.begin(), options.libraries.end() );

   if( !s_readUserLibraries( argv0, userLibraries, error ) || !s_checkUserLibraryConflicts( userLibraries, error ) )
      return s_linkFailed( error );

   s_importSearch( argv0, options, importSearch );
   importInputs.push_back( ( s_runtimeLibDir( argv0 ) / "libxdev.a" ).string() );

   /* --shared and --dynamic link a library built also as a DLL through its import library */
   for( const std::string &library : userLibraries )
   {
      importInputs.push_back( library );
      link.libraries.push_back( IIF( link.dynamicRuntime, s_importLibrary( library ), library ) );
   }

   s_addLibraryInputs( options.libraries, importSearch, importInputs );

   imported = x_dllImports( importInputs, importSearch, temporary.directory.string(), imports, error );

   for( const std::string &path : imports )
   {
#if X_OS_WINDOWS
      temporary.files.emplace_back( path );
#endif
      link.libraries.push_back( path );
   }

   if( !imported || !s_linkExecutable( link, temporary, error ) )
      return s_linkFailed( error );

   std::cerr << "Gotowe.\n";

   return 0;
}

static void s_reportOutput( const X_OPTIONS &options )
{
   if( !options.objPath.empty() )
      std::cerr << "Plik obiektowy zapisano w '" << options.objPath << "'... Gotowe.\n";
   else if( !options.irPath.empty() )
      std::cerr << "LLVM IR zapisano w '" << options.irPath << "'... Gotowe.\n";
   else if( !options.ppoPath.empty() )
      std::cerr << "Wynik preprocesora zapisano w '" << options.ppoPath << "'... Gotowe.\n";
   else
      std::cerr << "Gotowe.\n";
}

/* The other PRG files of a program are compiled first, so the first file sees their classes */
static int s_buildProgram( const char *argv0, X_OPTIONS &options, X_TEMP_FILES &temporary )
{
   const char *action = IIF( options.preprocessOnly, "Przetwarzanie przez preprocesor '", "Kompilowanie '" );
   X_STRINGS modules;
   X_COMPILE job;
   X_LINK link;
   std::string error;

   for( size_t i = 1; i < options.inputs.size(); ++i )
   {
      std::string module;
      X_COMPILE moduleJob;

      std::cerr << "Kompilowanie '" << options.inputs[i] << "'...\n";
      module = temporary.object( i ).string();
      moduleJob = s_moduleJob( options.inputs[i], module );
      moduleJob.sharedModule = options.sharedLibrary;

      if( !s_compileSource( moduleJob, options, error ) )
      {
         std::cerr << error << "\nKompilacja nie powiodla sie.\n";
         return 1;
      }

      modules.push_back( module );
      options.interfaces.insert( options.interfaces.begin(), module );
   }

   std::cerr << action << options.inputs[0] << "'...\n";

   job.input = options.inputs[0];
   job.object = IIF( options.binPath.empty(), options.objPath, temporary.object( 0 ).string() );
   job.libraryModule = !options.objPath.empty() || options.sharedLibrary;
   job.sharedModule = options.sharedLibrary;
   job.dumpAst = options.dumpAst;
   job.printIr = options.printIr;
   job.irPath = options.irPath;
   job.preprocessOnly = options.preprocessOnly;
   job.ppoPath = options.ppoPath;

   if( !s_compileSource( job, options, error ) )
   {
      std::cerr << error << "\nKompilacja nie powiodla sie.\n";
      return 1;
   }

   if( options.binPath.empty() )
   {
      s_reportOutput( options );
      return 0;
   }

   link.object = job.object;
   link.libraries = modules;

   return s_linkProgram( argv0, options, link, temporary );
}

static int s_build( const char *argv0, X_OPTIONS &options )
{
   std::string error;
   X_TEMP_FILES temporary;

   if( !options.binPath.empty() || ( options.archiveOutput && !options.inputs.empty() ) )
   {
      if( !temporary.create( error ) )
         return s_usage( error );
   }

   s_banner();

   if( !s_interfaces( argv0, options, error ) )
   {
      std::cerr << error << "\nKompilacja nie powiodla sie.\n";
      return 2;
   }

   if( options.archiveOutput )
      return s_buildArchive( options, temporary );

   return s_buildProgram( argv0, options, temporary );
}

static int s_driver( int argc, char **argv )
{
   X_OPTIONS options;
   int result;

   if( argc == 1 )
   {
      s_help();
      return 0;
   }

   if( std::string( argv[1] ) == "install" || std::string( argv[1] ) == "uninstall" )
      return s_manageUserLibraries( argc, argv );

   if( argc == 2 && s_hasExtension( argv[1], ".rmk" ) )
      return s_make( argv[0], argv[1] );

   for( int i = 1; i < argc; ++i )
   {
      if( std::string( argv[i] ) == "-each" )
         return s_each( argc, argv );
   }

   result = s_readOptions( argc, argv, options );
   if( result >= 0 )
      return result;

   s_addStandardIncludePath( argv[0], options.includePaths );
   options.archiveOutput = !options.outputPath.empty() && s_hasExtension( options.outputPath, ".a" );
   s_sortInputs( options );

   if( !s_checkOptions( options ) )
      return 2;

   s_outputPaths( options );

   if( !s_checkOutputs( options ) || !s_checkBinary( options.binary ) )
      return 2;

   return s_build( argv[0], options );
}

/* MinGW expands wildcards such as *.prg in the command line */
#if defined( __MINGW32__ )
extern "C" int _dowildcard = -1;
#endif

int main( int argc, char **argv )
{
   try
   {
      return s_driver( argc, argv );
   }
   catch( const std::out_of_range & )
   {
      std::cerr << "blad wewnetrzny kompilatora: brak wpisu w metadanych lub indeks poza zakresem\n";
   }
   catch( const std::exception &error )
   {
      std::cerr << "blad kompilatora: " << error.what() << '\n';
   }

   return 1;
}
