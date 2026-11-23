/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Backend
 * File......: dll.cpp
 *
 */

/*
 * File   : dll.cpp
 */

#include "platform.h"
#include "dll.hpp"

#if X_OS_WINDOWS || X_OS_LINUX
#include <filesystem>
#include <map>
#include <set>
#include <llvm/Object/Archive.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>
#if X_OS_WINDOWS
#include <llvm/Object/COFF.h>
#include <llvm/Object/COFFImportFile.h>
#include <windows.h>
#endif

namespace fs = std::filesystem;

/* One DLL FUNCTION: the library, the exported symbol, the name xDev calls and the object that declares it */
struct X_DLL_IMPORT
{
   std::string library;
   std::string symbol;
   std::string alias;
   std::string module;
};

using X_IMPORTS = std::vector<X_DLL_IMPORT>;
using X_STRINGS = std::vector<std::string>;
using X_PATHS = std::vector<fs::path>;

/* ---------------------------------------------------------------------------------------------
   Declarations
   --------------------------------------------------------------------------------------------- */

static bool s_collectImports( llvm::MemoryBufferRef buffer, X_IMPORTS &imports, std::string &error );

/* .xdevdll: library, symbol and alias of every declaration, each ended by a zero byte */
static bool s_parseImports( llvm::StringRef text, const std::string &module, X_IMPORTS &imports, std::string &error )
{
   while( !text.empty() )
   {
      X_DLL_IMPORT entry;

      for( std::string *field : { &entry.library, &entry.symbol, &entry.alias } )
      {
         const size_t end = text.find( '\0' );

         if( end == llvm::StringRef::npos || end == 0 )
         {
            error = module + ": uszkodzone deklaracje importow DLL";
            return false;
         }

         *field = text.take_front( end ).str();
         text = text.drop_front( end + 1 );
      }

      entry.module = module;
      imports.push_back( std::move( entry ) );
   }

   return true;
}

static bool s_archiveImports( llvm::object::Archive &archive, X_IMPORTS &imports, std::string &error )
{
   auto archiveError = llvm::Error::success();

   for( const auto &child : archive.children( archiveError ) )
   {
      auto member = child.getMemoryBufferRef();

      if( !member )
      {
         llvm::consumeError( std::move( archiveError ) );
         error = llvm::toString( member.takeError() );
         return false;
      }

      if( !s_collectImports( *member, imports, error ) )
      {
         llvm::consumeError( std::move( archiveError ) );
         return false;
      }
   }

   if( archiveError )
   {
      error = llvm::toString( std::move( archiveError ) );
      return false;
   }

   return true;
}

/* The declarations of an object file, or of every object in an archive */
static bool s_collectImports( llvm::MemoryBufferRef buffer, X_IMPORTS &imports, std::string &error )
{
   const std::string module = buffer.getBufferIdentifier().str();
   auto binary = llvm::object::createBinary( buffer );
   llvm::object::ObjectFile *object;

   if( !binary )
   {
      error = module + ": " + llvm::toString( binary.takeError() );
      return false;
   }

   if( auto *archive = llvm::dyn_cast<llvm::object::Archive>( binary->get() ) )
      return s_archiveImports( *archive, imports, error );

   object = llvm::dyn_cast<llvm::object::ObjectFile>( binary->get() );
   if( !object )
      return true;

   for( const auto &section : object->sections() )
   {
      auto name = section.getName();

      if( !name )
      {
         error = llvm::toString( name.takeError() );
         return false;
      }

      if( *name != ".xdevdll" )
         continue;

      auto contents = section.getContents();
      if( !contents )
      {
         error = llvm::toString( contents.takeError() );
         return false;
      }

      if( !s_parseImports( *contents, module, imports, error ) )
         return false;
   }

   return true;
}

/* The declarations of all inputs; the folder of every input is searched as well */
static bool s_readInputs( const X_STRINGS &inputs, X_PATHS &directories, X_IMPORTS &declarations, std::string &error )
{
   for( const std::string &input : inputs )
   {
      auto buffer = llvm::MemoryBuffer::getFile( input );

      if( !buffer )
      {
         error = input + ": " + buffer.getError().message();
         return false;
      }

      directories.push_back( fs::path( input ).parent_path() );

      if( !s_collectImports( ( *buffer )->getMemBufferRef(), declarations, error ) )
         return false;
   }

   return true;
}

/* The first folder that holds the file; path stays empty when none does */
static bool s_findFile( const X_PATHS &directories, const fs::path &name, fs::path &path, std::string &error )
{
   for( const fs::path &directory : directories )
   {
      const fs::path candidate = directory / name;
      std::error_code code;

      if( !fs::is_regular_file( candidate, code ) )
         continue;

      path = fs::weakly_canonical( candidate, code );
      if( code )
      {
         error = code.message();
         return false;
      }

      return true;
   }

   return true;
}
#endif

#if X_OS_WINDOWS
/* ---------------------------------------------------------------------------------------------
   Windows: import libraries
   --------------------------------------------------------------------------------------------- */

/* The exported names of a Windows x64 DLL */
static bool s_dllExports( const fs::path &file, std::set<std::string> &exports, std::string &error )
{
   auto buffer = llvm::MemoryBuffer::getFile( file.string() );

   if( !buffer )
   {
      error = file.string() + ": " + buffer.getError().message();
      return false;
   }

   auto dll = llvm::object::COFFObjectFile::create( ( *buffer )->getMemBufferRef() );
   if( !dll )
   {
      error = file.string() + ": " + llvm::toString( dll.takeError() );
      return false;
   }

   if( ( *dll )->getMachine() != IMAGE_FILE_MACHINE_AMD64 || !( ( *dll )->getCharacteristics() & IMAGE_FILE_DLL ) )
   {
      error = "DLL FUNCTION wymaga biblioteki PE Windows x64: " + file.string();
      return false;
   }

   for( const auto &exported : ( *dll )->export_directories() )
   {
      llvm::StringRef name;

      if( auto code = exported.getSymbolName( name ) )
      {
         error = llvm::toString( std::move( code ) );
         return false;
      }

      exports.insert( name.str() );
   }

   return true;
}

/* The import library of one DLL: alias is the name xDev calls, symbol the name the DLL exports */
static bool s_importLibrary( const fs::path &file, const X_IMPORTS &group, const std::string &path, std::string &error )
{
   const auto machine = static_cast<llvm::COFF::MachineTypes>( IMAGE_FILE_MACHINE_AMD64 );
   std::set<std::string> exports;
   std::set<std::string> aliases;
   std::vector<llvm::object::COFFShortExport> entries;

   if( !s_dllExports( file, exports, error ) )
      return false;

   for( const X_DLL_IMPORT &entry : group )
   {
      if( !exports.count( entry.symbol ) )
      {
         error = entry.module + ": brak funkcji " + entry.symbol + " w DLL " + file.string();
         return false;
      }

      if( aliases.insert( entry.alias ).second )
      {
         llvm::object::COFFShortExport exported;

         exported.Name = entry.alias;
         exported.ExportAs = entry.symbol;
         entries.push_back( std::move( exported ) );
      }
   }

   if( auto code = llvm::object::writeImportLibrary( file.filename().string(), path, entries, machine, true ) )
   {
      error = llvm::toString( std::move( code ) );
      return false;
   }

   return true;
}

bool x_dllImports( const std::vector<std::string> &inputs, const std::vector<std::string> &searchPaths,
                   const std::string &directory, std::vector<std::string> &imports, std::string &error )
{
   X_IMPORTS declarations;
   X_PATHS directories{ fs::current_path() };
   std::map<fs::path, X_IMPORTS> groups;
   std::map<std::string, fs::path> resolved;
   wchar_t systemDirectory[MAX_PATH];
   size_t index = 0;

   for( const std::string &path : searchPaths )
      directories.emplace_back( path );

   if( GetSystemDirectoryW( systemDirectory, MAX_PATH ) != 0 )
      directories.emplace_back( systemDirectory );

   if( !s_readInputs( inputs, directories, declarations, error ) )
      return false;

   /* The declarations grouped by the real path of their DLL */
   for( const X_DLL_IMPORT &entry : declarations )
   {
      auto found = resolved.find( entry.library );

      if( found == resolved.end() )
      {
         fs::path path;

         if( !s_findFile( directories, entry.library, path, error ) )
            return false;

         if( path.empty() )
         {
            error = entry.module + ": nie znaleziono DLL: " + entry.library;
            return false;
         }

         found = resolved.emplace( entry.library, path ).first;
      }

      groups[found->second].push_back( entry );
   }

   for( const auto &group : groups )
   {
      const std::string path = ( fs::path( directory ) / ( "import" + std::to_string( index++ ) + ".a" ) ).string();

      imports.push_back( path );

      if( !s_importLibrary( group.first, group.second, path, error ) )
         return false;
   }

   return true;
}
#elif X_OS_LINUX
/* ---------------------------------------------------------------------------------------------
   Linux: shared libraries
   --------------------------------------------------------------------------------------------- */

/* A library found in no folder is left to the search of the linker: -l:name */
bool x_dllImports( const std::vector<std::string> &inputs, const std::vector<std::string> &searchPaths,
                   const std::string &, std::vector<std::string> &imports, std::string &error )
{
   X_IMPORTS declarations;
   X_PATHS directories{ fs::current_path() };
   std::set<std::string> resolved;

   for( const std::string &path : searchPaths )
      directories.emplace_back( path );

   if( !s_readInputs( inputs, directories, declarations, error ) )
      return false;

   for( const X_DLL_IMPORT &entry : declarations )
   {
      const fs::path name( entry.library );
      fs::path path;

      if( !resolved.insert( entry.library ).second )
         continue;

      if( !s_findFile( directories, name, path, error ) )
         return false;

      if( !path.empty() )
         imports.push_back( path.string() );
      else if( !name.has_parent_path() )
         imports.push_back( "-l:" + entry.library );
      else
      {
         error = entry.module + ": nie znaleziono biblioteki: " + entry.library;
         return false;
      }
   }

   return true;
}
#else
bool x_dllImports( const std::vector<std::string> &, const std::vector<std::string> &,
                   const std::string &, std::vector<std::string> &, std::string & )
{
   return true;
}
#endif
