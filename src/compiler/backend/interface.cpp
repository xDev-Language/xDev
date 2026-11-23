/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Backend
 * File......: interface.cpp
 *
 */

/*
 * The class description of a library, one line per entry:
 *
 *    XDEVCLS 1
 *    C name parents
 *    M kind name scope type typeName params...
 *    F kind name procedure result resultTypeName [param type typeName]...
 *
 * An empty name is written as -.
 */

#include "internal.h"
#include "interface.hpp"

#include <cctype>
#include <filesystem>
#include <sstream>
#include <unordered_set>

#include <llvm/Object/Archive.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>

namespace fs = std::filesystem;

/* The classes read so far; a class found in two libraries is taken from the first one */
struct X_CLASS_IMPORT
{
   X_PROGRAM &program;
   std::unordered_set<std::string> known;
};

/* One description: the class being read; skip is set for a class known already */
struct X_CLASS_READER
{
   X_CLASS_IMPORT &classes;
   X_CLASS_DECL *current = nullptr;
   bool skip = false;
};

static const char s_header[] = "XDEVCLS 1";

static std::string s_upper( const std::string &value )
{
   std::string key = value;

   for( char &ch : key )
      ch = ( char )std::toupper( ( unsigned char )ch );

   return key;
}

static std::string s_field( const std::string &value )
{
   return IIF( value.empty(), std::string( "-" ), value );
}

static std::string s_value( const std::string &field )
{
   return IIF( field == "-", std::string(), field );
}

static bool s_type( const std::string &name, X_DECL_TYPE &type )
{
   for( int i = 0; i <= ( int )X_DECL_TYPE::HASH; ++i )
   {
      if( name == x_declTypeName( ( X_DECL_TYPE )i ) )
      {
         type = ( X_DECL_TYPE )i;
         return true;
      }
   }

   return false;
}

/* ---------------------------------------------------------------------------------------------
   Writing
   --------------------------------------------------------------------------------------------- */

static void s_writeMember( std::ostringstream &out, const X_CLASS_MEMBER &member )
{
   out << "M " << member.kind << ' ' << member.name << ' ' << member.scope;
   out << ' ' << x_declTypeName( member.type ) << ' ' << s_field( member.typeName );

   for( const std::string &param : member.params )
      out << ' ' << param;

   out << '\n';
}

static void s_writeParam( std::ostringstream &out, const X_FUNCTION &function, const std::string &param )
{
   const std::string key = s_upper( param );
   const auto type = function.paramTypes.find( key );
   const auto typeName = function.paramTypeNames.find( key );
   X_DECL_TYPE declared = X_DECL_TYPE::ANY;
   std::string name;

   if( type != function.paramTypes.end() )
      declared = type->second;

   if( typeName != function.paramTypeNames.end() )
      name = typeName->second;

   out << ' ' << param << ' ' << x_declTypeName( declared ) << ' ' << s_field( name );
}

static void s_writeMethod( std::ostringstream &out, const X_FUNCTION &function )
{
   const char *result = "-";

   if( function.resultType )
      result = x_declTypeName( *function.resultType );

   out << "F " << function.methodKind << ' ' << function.methodName << ' ' << IIF( function.isProcedure, 1, 0 );
   out << ' ' << result << ' ' << s_field( function.resultTypeName );

   for( const std::string &param : function.params )
      s_writeParam( out, function, param );

   out << '\n';
}

/* The class line, the members and the method bodies of the class */
static void s_writeClass( std::ostringstream &out, const X_PROGRAM &program, const X_CLASS_DECL &cls )
{
   std::string parents;

   for( const std::string &parent : cls.parents )
   {
      if( !parents.empty() )
         parents += ',';

      parents += parent;
   }

   out << "C " << cls.name << ' ' << s_field( parents ) << '\n';

   for( const X_CLASS_MEMBER &member : cls.members )
      s_writeMember( out, member );

   for( const X_FUNCTION &function : program.functions )
   {
      if( function.className != cls.name || function.methodName.empty() )
         continue;

      if( function.isDataInitializer || function.isClassDataInitializer )
         continue;

      s_writeMethod( out, function );
   }
}

std::string x_classInterfaceWrite( const X_PROGRAM &program )
{
   std::ostringstream out;

   if( program.classes.empty() )
      return {};

   out << s_header << '\n';

   for( const X_CLASS_DECL &cls : program.classes )
      s_writeClass( out, program, cls );

   return out.str();
}

/* ---------------------------------------------------------------------------------------------
   Reading
   --------------------------------------------------------------------------------------------- */

/* C name parents; a class read from another library already is skipped with its lines */
static bool s_readClass( std::istringstream &fields, X_CLASS_READER &reader )
{
   std::string name;
   std::string parents;
   std::istringstream list;

   if( !( fields >> name >> parents ) )
      return false;

   reader.skip = !reader.classes.known.insert( name ).second;
   if( reader.skip )
      return true;

   reader.classes.program.importedClasses.emplace_back();
   reader.current = &reader.classes.program.importedClasses.back();
   reader.current->name = name;

   list.str( s_value( parents ) );
   for( std::string parent; std::getline( list, parent, ',' ); )
      reader.current->parents.push_back( parent );

   return true;
}

static bool s_readMember( std::istringstream &fields, X_CLASS_READER &reader )
{
   X_CLASS_MEMBER member;
   std::string type;
   std::string typeName;

   if( reader.skip )
      return true;

   if( reader.current == nullptr )
      return false;

   if( !( fields >> member.kind >> member.name >> member.scope >> type >> typeName ) || !s_type( type, member.type ) )
      return false;

   member.typeName = s_value( typeName );

   for( std::string param; fields >> param; )
      member.params.push_back( param );

   reader.current->members.push_back( std::move( member ) );

   return true;
}

static bool s_readParams( std::istringstream &fields, X_FUNCTION &function )
{
   for( std::string param, type, typeName; fields >> param >> type >> typeName; )
   {
      X_DECL_TYPE declared;

      if( !s_type( type, declared ) )
         return false;

      function.params.push_back( param );

      if( declared != X_DECL_TYPE::ANY )
         function.paramTypes.emplace( s_upper( param ), declared );

      if( typeName != "-" )
         function.paramTypeNames.emplace( s_upper( param ), typeName );
   }

   return true;
}

/* A method body becomes the function __XDEV_CLASS_KIND_NAME */
static bool s_readMethod( std::istringstream &fields, X_CLASS_READER &reader )
{
   X_FUNCTION function;
   int procedure;
   std::string result;
   std::string resultTypeName;

   if( reader.skip )
      return true;

   if( reader.current == nullptr )
      return false;

   if( !( fields >> function.methodKind >> function.methodName >> procedure >> result >> resultTypeName ) )
      return false;

   function.className = reader.current->name;
   function.name = "__XDEV_" + function.className + "_" + function.methodKind + "_" + function.methodName;
   function.isProcedure = procedure != 0;
   function.resultTypeName = s_value( resultTypeName );

   if( result != "-" )
   {
      X_DECL_TYPE type;

      if( !s_type( result, type ) )
         return false;

      function.resultType = type;
   }

   if( !s_readParams( fields, function ) )
      return false;

   reader.classes.program.importedMethods.push_back( std::move( function ) );

   return true;
}

/* false for a line that cannot be read */
static bool s_readLine( const std::string &line, X_CLASS_READER &reader )
{
   std::istringstream fields( line );
   std::string tag;

   fields >> tag;

   if( tag == "C" )
      return s_readClass( fields, reader );

   if( tag == "M" )
      return s_readMember( fields, reader );

   if( tag == "F" )
      return s_readMethod( fields, reader );

   return tag.empty();
}

/* The text of one .xdevcls section */
static bool s_parse( llvm::StringRef text, const std::string &source, X_CLASS_IMPORT &classes, std::string &error )
{
   std::istringstream in( text.str() );
   X_CLASS_READER reader{ classes };
   std::string line;

   if( !std::getline( in, line ) )
   {
      error = source + ": uszkodzony opis klas";
      return false;
   }

   if( line != s_header )
   {
      error = source + ": nieobslugiwana wersja opisu klas (" + line + "); przebuduj biblioteke";
      return false;
   }

   while( std::getline( in, line ) )
   {
      if( !s_readLine( line, reader ) )
      {
         error = source + ": uszkodzony opis klas";
         return false;
      }
   }

   return true;
}

static bool s_collect( llvm::MemoryBufferRef buffer, X_CLASS_IMPORT &classes, std::string &error );

static bool s_collectArchive( llvm::object::Archive &archive, X_CLASS_IMPORT &classes, std::string &error )
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

      if( !s_collect( *member, classes, error ) )
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

/* The classes of an object file, or of every object in an archive */
static bool s_collect( llvm::MemoryBufferRef buffer, X_CLASS_IMPORT &classes, std::string &error )
{
   const std::string source = buffer.getBufferIdentifier().str();
   auto binary = llvm::object::createBinary( buffer );
   llvm::object::ObjectFile *object;

   if( !binary )
   {
      error = source + ": " + llvm::toString( binary.takeError() );
      return false;
   }

   if( auto *archive = llvm::dyn_cast<llvm::object::Archive>( binary->get() ) )
      return s_collectArchive( *archive, classes, error );

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

      if( *name != ".xdevcls" )
         continue;

      auto contents = section.getContents();
      if( !contents )
      {
         error = llvm::toString( contents.takeError() );
         return false;
      }

      if( !s_parse( *contents, source, classes, error ) )
         return false;
   }

   return true;
}

bool x_classInterfaceRead( const std::vector<std::string> &inputs, X_PROGRAM &program, std::string &error )
{
   X_CLASS_IMPORT classes{ program, {} };

   for( const std::string &input : inputs )
   {
      std::error_code code;

      if( !fs::is_regular_file( input, code ) )
         continue;

      auto buffer = llvm::MemoryBuffer::getFile( input );
      if( !buffer )
      {
         error = input + ": " + buffer.getError().message();
         return false;
      }

      if( !s_collect( ( *buffer )->getMemBufferRef(), classes, error ) )
         return false;
   }

   return true;
}
