/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Frontend
 * File: preprocessor.cpp
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal.h"
#include "platform.h"

#include "preprocessor.hpp"
#include "internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{

struct PP_DEFINE
{
   bool function;
   std::vector<std::string> parameters;
   PP_TOKENS body;
};

/* One open #if block */
struct PP_CONDITION
{
   bool parent;
   bool selected;
   bool alternate;
   int line;
};

/* State of one source: open #if blocks, active lines, an open block comment */
struct PP_STATE
{
   std::vector<PP_CONDITION> conditions;
   bool active = true;
   bool comment = false;
};

} // namespace

/* Words after which [ opens a string, not an index */
static const std::unordered_set<std::string> s_keywords = {
   "RETURN", "LOCAL", "STATIC", "IF", "ELSEIF", "WHILE", "CASE", "TO", "STEP", "WITH",
   "INIT", "BREAK", "FUNCTION", "PROCEDURE", "DO", "FOR", "SWITCH", "FIELD", "IN"
};

/* Two-character operators */
static const std::unordered_set<std::string> s_pairs = {
   ":=", "+=", "-=", "*=", "/=", "%=", "^=", "==", "!=", "<>", "<=", ">=", "**", "++", "--", "->", "=>"
};

/* ---------------------------------------------------------------------------------------------
   Characters and names
   --------------------------------------------------------------------------------------------- */

static bool s_identifierStart( char ch )
{
   return std::isalpha( ( unsigned char )ch ) || ch == '_';
}

static bool s_identifierPart( char ch )
{
   return std::isalnum( ( unsigned char )ch ) || ch == '_';
}

static bool s_isIdentifier( const std::string &name )
{
   if( name.empty() || !s_identifierStart( name[0] ) )
      return false;

   return std::all_of( name.begin(), name.end(), s_identifierPart );
}

static std::string s_upper( std::string text )
{
   for( char &ch : text )
      ch = ( char )std::toupper( ( unsigned char )ch );

   return text;
}

static std::string s_where( const X_SOURCE_LOCATION &where )
{
   return where.file + ": linia " + std::to_string( where.line );
}

/* Two tokens written together in the source stay together in the output; macro tokens get a space */
static bool s_sourceJoined( const PP_TOKEN &previous, const PP_TOKEN &token )
{
   return token.attached && previous.disabled.empty() && token.disabled.empty();
}

/* Tokens joined with one space, except where a token is attached to the previous one */
static std::string s_text( const PP_TOKENS &tokens )
{
   std::string text;

   for( const PP_TOKEN &token : tokens )
   {
      if( !text.empty() && !token.attached )
         text += ' ';

      text += token.text;
   }

   return text;
}

/* ---------------------------------------------------------------------------------------------
   Tokens
   --------------------------------------------------------------------------------------------- */

/* A value, a name or a closing bracket: a [ after it is an index */
static bool s_endsExpression( const PP_TOKEN &token )
{
   const char first = token.text[0];
   const std::string upper = s_upper( token.text );

   if( token.identifier )
      return !s_keywords.count( upper );

   if( std::isdigit( ( unsigned char )first ) || first == '\'' || first == '"' )
      return true;

   if( first == '[' && token.text.size() > 1 )
      return true;

   if( token.text == ")" || token.text == "]" || token.text == "}" )
      return true;

   return upper == ".T." || upper == ".F.";
}

/* The tokens so far are # define NAME or # define NAME( params ): the body starts next */
static bool s_defineBodyStart( const PP_TOKENS &tokens )
{
   if( tokens.size() < 3 || tokens[0].text != "#" || s_upper( tokens[1].text ) != "DEFINE" )
      return false;

   if( tokens.size() == 3 )
      return true;

   if( tokens[3].text != "(" || !tokens[3].attached )
      return false;

   for( size_t i = 4; i < tokens.size(); ++i )
   {
      if( tokens[i].text == ")" )
         return i + 1 == tokens.size();
   }

   return false;
}

/* [ opens a string at the start of an expression and at the start of a #define body */
static bool s_bracketString( const PP_TOKENS &tokens )
{
   if( tokens.empty() || s_defineBodyStart( tokens ) )
      return true;

   return !s_endsExpression( tokens.back() );
}

/* #stdout and #error: the rest of the line is the message */
static bool s_messageDirective( const PP_TOKENS &tokens )
{
   std::string name;

   if( tokens.size() < 2 || tokens[0].text != "#" )
      return false;

   name = s_upper( tokens[1].text );

   return name == "STDOUT" || name == "ERROR";
}

/* The message is one token; a ; at the end continues it on the next line */
static void s_addMessage( const std::string &line, size_t i, PP_TOKENS &tokens )
{
   size_t end = line.find_last_not_of( " \t\r" ) + 1;
   const bool continued = end > i && line[end - 1] == ';';

   if( continued )
      --end;

   tokens.push_back( { line.substr( i, end - i ), false, false, {} } );

   if( continued )
      tokens.push_back( { ";", false, false, {} } );
}

static bool s_isEString( const std::string &line, size_t i )
{
   if( line[i] != 'e' && line[i] != 'E' )
      return false;

   return i + 1 < line.size() && ( line[i + 1] == '"' || line[i + 1] == '\'' );
}

/* Escaped quotes do not end an e-string; the lexer decodes the escapes */
static size_t s_eStringEnd( const std::string &line, size_t i )
{
   const char quote = line[i + 1];

   i += 2;
   while( i < line.size() && line[i] != quote )
      i += IIF( line[i] == '\\', 2, 1 );

   if( i >= line.size() )
      return std::string::npos;

   return i + 1;
}

static size_t s_stringEnd( const std::string &line, size_t i )
{
   const char quote = IIF( line[i] == '[', ']', line[i] );
   const size_t end = line.find( quote, i + 1 );

   if( end == std::string::npos )
      return end;

   return end + 1;
}

static size_t s_wordEnd( const std::string &line, size_t i )
{
   while( i < line.size() && s_identifierPart( line[i] ) )
      ++i;

   return i;
}

static bool s_numberStart( const std::string &line, size_t i )
{
   if( std::isdigit( ( unsigned char )line[i] ) )
      return true;

   return line[i] == '.' && i + 1 < line.size() && std::isdigit( ( unsigned char )line[i + 1] );
}

/* Digits, letters, dots and the sign of an exponent: 1.5e+3, 0xFF */
static size_t s_numberEnd( const std::string &line, size_t i )
{
   for( ++i; i < line.size(); ++i )
   {
      if( s_identifierPart( line[i] ) || line[i] == '.' )
         continue;

      if( ( line[i] == '+' || line[i] == '-' ) && ( line[i - 1] == 'e' || line[i - 1] == 'E' ) )
         continue;

      break;
   }

   return i;
}

/* .AND., .T. and other words between dots */
static size_t s_dottedEnd( const std::string &line, size_t i )
{
   i = s_wordEnd( line, i + 1 );

   if( i < line.size() && line[i] == '.' )
      ++i;

   return i;
}

/* End of the token at i, or npos for a string without its closing quote */
static size_t s_tokenEnd( const std::string &line, size_t i, const PP_TOKENS &tokens )
{
   if( s_isEString( line, i ) )
      return s_eStringEnd( line, i );

   if( line[i] == '\'' || line[i] == '"' || ( line[i] == '[' && s_bracketString( tokens ) ) )
      return s_stringEnd( line, i );

   if( s_identifierStart( line[i] ) )
      return s_wordEnd( line, i );

   if( s_numberStart( line, i ) )
      return s_numberEnd( line, i );

   if( line.compare( i, 3, "..." ) == 0 )
      return i + 3;

   if( line[i] == '.' && i + 1 < line.size() && s_identifierStart( line[i + 1] ) )
      return s_dottedEnd( line, i );

   return i + IIF( s_pairs.count( line.substr( i, 2 ) ), 2, 1 );
}

/* ---------------------------------------------------------------------------------------------
   Macros
   --------------------------------------------------------------------------------------------- */

static const char *s_closing( const std::string &open )
{
   if( open == "(" )
      return ")";

   if( open == "[" )
      return "]";

   return "}";
}

/* Arguments of a macro call: end is the index after ( and becomes the index after ) */
static const char *s_arguments( const PP_TOKENS &tokens, size_t &end, bool hasParameters, std::vector<PP_TOKENS> &out )
{
   PP_TOKENS argument;
   std::vector<std::string> nesting;

   for( ; end < tokens.size(); ++end )
   {
      const std::string &text = tokens[end].text;

      if( nesting.empty() && text == ")" )
      {
         if( !argument.empty() || !out.empty() || hasParameters )
            out.push_back( argument );

         break;
      }

      if( nesting.empty() && text == "," )
      {
         out.push_back( argument );
         argument.clear();
         continue;
      }

      if( text == "(" || text == "[" || text == "{" )
         nesting.push_back( s_closing( text ) );
      else if( text == ")" || text == "]" || text == "}" )
      {
         if( nesting.empty() || nesting.back() != text )
            return "niezgodne nawiasy w makrze ";

         nesting.pop_back();
      }

      argument.push_back( tokens[end] );
   }

   if( end == tokens.size() )
      return "niedomkniete wywolanie makra ";

   ++end;

   return nullptr;
}

/* The macro body with the parameters replaced; the result does not expand the macro again */
static PP_TOKENS s_replace( const PP_DEFINE &macro, const std::vector<PP_TOKENS> &arguments, const PP_TOKEN &token )
{
   PP_TOKENS replacement;

   for( const PP_TOKEN &part : macro.body )
   {
      const auto parameter = std::find( macro.parameters.begin(), macro.parameters.end(), part.text );

      if( part.identifier && parameter != macro.parameters.end() )
      {
         const PP_TOKENS &argument = arguments[parameter - macro.parameters.begin()];

         replacement.insert( replacement.end(), argument.begin(), argument.end() );
      }
      else
         replacement.push_back( part );
   }

   for( PP_TOKEN &part : replacement )
   {
      part.disabled.insert( token.disabled.begin(), token.disabled.end() );
      part.disabled.insert( token.text );
   }

   return replacement;
}

/* #define NAME( a, b ): unique names separated by commas; body stops at the closing ) */
static bool s_parameters( const PP_TOKENS &tokens, size_t &body, std::vector<std::string> &parameters )
{
   if( body >= tokens.size() || tokens[body].text == ")" )
      return true;

   for( ;; )
   {
      if( body == tokens.size() || !tokens[body].identifier )
         return false;

      if( std::find( parameters.begin(), parameters.end(), tokens[body].text ) != parameters.end() )
         return false;

      parameters.push_back( tokens[body++].text );

      if( body == tokens.size() || tokens[body].text != "," )
         return true;

      ++body;
   }
}

/* OS_ and ARCH_ definitions of the compiler platform */
static std::vector<std::string> s_platformDefinitions()
{
   return {
      "OS_UNIX=" + std::to_string( X_OS_UNIX ),
      "OS_LINUX=" + std::to_string( X_OS_LINUX ),
      "OS_DARWIN=" + std::to_string( X_OS_DARWIN ),
      "OS_MACOS=" + std::to_string( X_OS_MACOS ),
      "OS_BSD=" + std::to_string( X_OS_BSD ),
      "OS_WINDOWS=" + std::to_string( X_OS_WINDOWS ),
      "OS_CYGWIN=" + std::to_string( X_OS_CYGWIN ),
      "OS_HAIKU=" + std::to_string( X_OS_HAIKU ),
      "OS_SOLARIS=" + std::to_string( X_OS_SOLARIS ),
      "OS_AIX=" + std::to_string( X_OS_AIX ),
      "OS_POSIX=" + std::to_string( X_OS_POSIX ),
      "OS_BSD_FAMILY=" + std::to_string( X_OS_BSD_FAMILY ),
      "ARCH_64BIT=" + std::to_string( X_ARCH_64BIT ),
      "LITTLE_ENDIAN=" + std::to_string( X_LITTLE_ENDIAN ),
      "BIG_ENDIAN=" + std::to_string( X_BIG_ENDIAN )
   };
}

/* ---------------------------------------------------------------------------------------------
   Preprocessor
   --------------------------------------------------------------------------------------------- */

namespace
{

class X_PREPROCESSOR
{
private:
   std::vector<std::filesystem::path> m_paths;
   std::unordered_map<std::string, PP_DEFINE> m_definitions;
   X_PREPROCESSED &m_output;
   std::string &m_error;

   bool fail( const X_SOURCE_LOCATION &where, const std::string &message )
   {
      m_error = s_where( where ) + ": " + message;
      return false;
   }

   /* One line into tokens; a block comment may continue from the previous line */
   bool tokenize( const std::string &line, PP_TOKENS &tokens, PP_STATE &state, const X_SOURCE_LOCATION &where )
   {
      size_t i = 0;
      size_t previousEnd = std::string::npos;

      while( i < line.size() )
      {
         size_t end;

         if( state.comment )
         {
            const size_t close = line.find( "*/", i );
            const size_t nested = line.find( "/*", i );

            if( nested != std::string::npos && ( close == std::string::npos || nested < close ) )
               return fail( where, "zagniezdzony komentarz blokowy" );

            if( close == std::string::npos )
               return true;

            i = close + 2;
            state.comment = false;
            continue;
         }

         if( std::isspace( ( unsigned char )line[i] ) )
         {
            ++i;
            continue;
         }

         if( s_messageDirective( tokens ) )
         {
            s_addMessage( line, i, tokens );
            return true;
         }

         if( line.compare( i, 2, "//" ) == 0 )
            break;

         if( line.compare( i, 2, "/*" ) == 0 )
         {
            state.comment = true;
            i += 2;
            continue;
         }

         /* An open string is an error only in active lines */
         end = s_tokenEnd( line, i, tokens );
         if( end == std::string::npos )
            return !state.active || fail( where, "niedomkniety napis" );

         tokens.push_back( { line.substr( i, end - i ), s_identifierStart( line[i] ), i == previousEnd, {} } );
         previousEnd = end;
         i = end;
      }

      return true;
   }

   /* Replaces #define names; a name is not expanded inside its own replacement */
   bool expand( PP_TOKENS &tokens, const X_SOURCE_LOCATION &where, size_t &steps, size_t depth = 0 )
   {
      if( depth > 128 )
         return fail( where, "przekroczony limit zagniezdzen makr" );

      for( size_t i = 0; i < tokens.size(); )
      {
         const PP_TOKEN token = tokens[i];
         const auto definition = m_definitions.find( token.text );
         size_t end = i + 1;
         std::vector<PP_TOKENS> arguments;
         PP_TOKENS replacement;

         if( !token.identifier || definition == m_definitions.end() || token.disabled.count( token.text ) )
         {
            ++i;
            continue;
         }

         if( ++steps > 4096 )
            return fail( where, "przekroczony limit rozwiniec #define" );

         if( definition->second.function )
         {
            const PP_DEFINE &macro = definition->second;
            const char *message;

            /* A function-like name without ( stays as it is */
            if( end == tokens.size() || tokens[end].text != "(" )
            {
               ++i;
               continue;
            }

            ++end;
            message = s_arguments( tokens, end, !macro.parameters.empty(), arguments );
            if( message )
               return fail( where, message + token.text );

            if( arguments.size() != macro.parameters.size() )
               return fail( where, "niewlasciwa liczba argumentow makra " + token.text );

            for( PP_TOKENS &argument : arguments )
            {
               if( !expand( argument, where, steps, depth + 1 ) )
                  return false;
            }
         }

         replacement = s_replace( definition->second, arguments, token );

         if( tokens.size() - ( end - i ) + replacement.size() > 1048576 )
            return fail( where, "rozwiniecie #define jest zbyt duze" );

         tokens.erase( tokens.begin() + i, tokens.begin() + end );
         tokens.insert( tokens.begin() + i, replacement.begin(), replacement.end() );
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Conditions
      --------------------------------------------------------------------------------------------- */

   /* defined NAME or defined( NAME ) at tokens[i] becomes 1 or 0 */
   bool replaceDefined( PP_TOKENS &tokens, size_t i )
   {
      size_t name = i + 1;
      const bool parentheses = name < tokens.size() && tokens[name].text == "(";

      if( parentheses )
         ++name;

      if( name >= tokens.size() || !tokens[name].identifier )
         return false;

      if( parentheses && ( name + 1 >= tokens.size() || tokens[name + 1].text != ")" ) )
         return false;

      tokens[i].text = IIF( m_definitions.count( tokens[name].text ), "1", "0" );
      tokens[i].identifier = false;
      tokens.erase( tokens.begin() + i + 1, tokens.begin() + name + 1 + parentheses );

      return true;
   }

   bool condition( PP_TOKENS tokens, const X_SOURCE_LOCATION &where, bool &value )
   {
      size_t steps = 0;
      std::string message;

      for( size_t i = 0; i < tokens.size(); ++i )
      {
         if( tokens[i].text == "defined" && !replaceDefined( tokens, i ) )
            return fail( where, "defined wymaga nazwy definicji" );
      }

      if( !expand( tokens, where, steps ) )
         return false;

      return x_ppCondition( tokens, value, message ) || fail( where, message );
   }

   bool directiveIf( const PP_TOKENS &tokens, PP_STATE &state, const X_SOURCE_LOCATION &where )
   {
      bool selected = false;

      if( state.active && !condition( PP_TOKENS( tokens.begin() + 2, tokens.end() ), where, selected ) )
         return false;

      state.conditions.push_back( { state.active, selected, false, where.line } );
      state.active = state.active && selected;

      return true;
   }

   /* The #elif condition is evaluated only when no earlier branch was selected */
   bool directiveElif( const PP_TOKENS &tokens, PP_STATE &state, const X_SOURCE_LOCATION &where )
   {
      bool selected = false;

      if( state.conditions.empty() || state.conditions.back().alternate )
         return fail( where, "#elif bez #if albo po #else" );

      PP_CONDITION &branch = state.conditions.back();
      const bool open = branch.parent && !branch.selected;

      if( open && !condition( PP_TOKENS( tokens.begin() + 2, tokens.end() ), where, selected ) )
         return false;

      state.active = open && selected;
      branch.selected = branch.selected || selected;

      return true;
   }

   bool directiveIfdef( const PP_TOKENS &tokens, PP_STATE &state, const X_SOURCE_LOCATION &where )
   {
      const std::string command = s_upper( tokens[1].text );
      bool defined;
      bool selected;

      if( tokens.size() != 3 || !tokens[2].identifier )
         return fail( where, "#" + command + " wymaga nazwy definicji" );

      defined = m_definitions.count( tokens[2].text ) != 0;
      selected = IIF( command == "IFDEF", defined, !defined );
      state.conditions.push_back( { state.active, selected, false, where.line } );
      state.active = state.active && selected;

      return true;
   }

   /* #else switches to the other branch, #endif closes the block */
   bool directiveElse( const PP_TOKENS &tokens, PP_STATE &state, const X_SOURCE_LOCATION &where )
   {
      const std::string command = s_upper( tokens[1].text );

      if( tokens.size() != 2 || state.conditions.empty() )
         return fail( where, "nieprawidlowe #" + command );

      PP_CONDITION &block = state.conditions.back();

      if( command == "ENDIF" )
      {
         state.active = block.parent;
         state.conditions.pop_back();
         return true;
      }

      if( block.alternate )
         return fail( where, "powtorzone #else" );

      block.alternate = true;
      state.active = block.parent && !block.selected;

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Directives
      --------------------------------------------------------------------------------------------- */

   bool directiveMessage( const PP_TOKENS &tokens, const X_SOURCE_LOCATION &where )
   {
      const std::string message = s_text( PP_TOKENS( tokens.begin() + 2, tokens.end() ) );

      if( s_upper( tokens[1].text ) == "ERROR" )
         return fail( where, "#error " + message );

      m_output.messages.push_back( message );

      return true;
   }

   /* #define NAME body or #define NAME( params ) body; ( must be attached to the name */
   bool directiveDefine( const PP_TOKENS &tokens, const X_SOURCE_LOCATION &where )
   {
      PP_DEFINE macro = { false, {}, {} };
      size_t body = 3;

      if( tokens.size() < 3 || !tokens[2].identifier )
         return fail( where, "#define wymaga nazwy" );

      if( body < tokens.size() && tokens[body].attached && tokens[body].text == "(" )
      {
         macro.function = true;
         ++body;

         if( !s_parameters( tokens, body, macro.parameters ) )
            return fail( where, "nieprawidlowe parametry #define" );

         if( body == tokens.size() || tokens[body++].text != ")" )
            return fail( where, "brak ) w #define" );
      }

      macro.body.assign( tokens.begin() + body, tokens.end() );

      if( m_definitions.count( tokens[2].text ) )
         m_output.warnings.push_back( s_where( where ) + ": ponowna definicja " + tokens[2].text );

      m_definitions[tokens[2].text] = std::move( macro );

      return true;
   }

   /* The folder of the current file first, then -I and INCLUDE */
   bool directiveInclude( const PP_TOKENS &tokens, const X_SOURCE_LOCATION &where, size_t depth )
   {
      std::filesystem::path name;
      std::vector<std::filesystem::path> candidates;

      if( tokens.size() != 3 || tokens[2].text.size() < 3 || tokens[2].text.front() != '"' )
         return fail( where, "#include wymaga nazwy pliku w cudzyslowie" );

      name = tokens[2].text.substr( 1, tokens[2].text.size() - 2 );
      candidates.push_back( std::filesystem::path( where.file ).parent_path() / name );

      if( !name.is_absolute() )
      {
         for( const std::filesystem::path &path : m_paths )
            candidates.push_back( path / name );
      }

      for( const std::filesystem::path &candidate : candidates )
      {
         std::error_code code;

         if( std::filesystem::is_regular_file( candidate, code ) )
            return read( candidate, depth + 1 );
      }

      return fail( where, "nie znaleziono naglowka: " + name.string() );
   }

   bool directive( const PP_TOKENS &tokens, PP_STATE &state, const X_SOURCE_LOCATION &where, size_t depth )
   {
      const std::string command = IIF( tokens.size() > 1, s_upper( tokens[1].text ), "" );

      if( command == "IF" )
         return directiveIf( tokens, state, where );

      if( command == "ELIF" )
         return directiveElif( tokens, state, where );

      if( command == "IFDEF" || command == "IFNDEF" )
         return directiveIfdef( tokens, state, where );

      if( command == "ELSE" || command == "ENDIF" )
         return directiveElse( tokens, state, where );

      /* The other directives are skipped in an inactive block */
      if( !state.active )
         return true;

      if( command == "ERROR" || command == "STDOUT" )
         return directiveMessage( tokens, where );

      if( command == "DEFINE" )
         return directiveDefine( tokens, where );

      if( command == "UNDEF" )
      {
         if( tokens.size() != 3 || !tokens[2].identifier )
            return fail( where, "#undef wymaga nazwy definicji" );

         m_definitions.erase( tokens[2].text );
         return true;
      }

      if( command == "INCLUDE" )
         return directiveInclude( tokens, where, depth );

      return fail( where, "nieobslugiwana dyrektywa #" + command );
   }

   /* ---------------------------------------------------------------------------------------------
      Output
      --------------------------------------------------------------------------------------------- */

   /* An active line goes to the output; a ; inside it starts a new output line */
   bool emit( PP_TOKENS &tokens, const X_SOURCE_LOCATION &where )
   {
      size_t steps = 0;
      const PP_TOKEN *previous = nullptr;

      if( !expand( tokens, where, steps ) )
         return false;

      for( const PP_TOKEN &token : tokens )
      {
         if( token.text == ";" )
         {
            m_output.text += '\n';
            m_output.lines.push_back( where );
            previous = nullptr;
         }
         else
         {
            if( previous && !s_sourceJoined( *previous, token ) )
               m_output.text += ' ';

            m_output.text += token.text;
            previous = &token;
         }
      }

      m_output.text += '\n';
      m_output.lines.push_back( where );

      return true;
   }

public:
   /* -I paths first, then INCLUDE from the environment: ; on Windows, : elsewhere */
   X_PREPROCESSOR( const std::vector<std::string> &includePaths, X_PREPROCESSED &result, std::string &error )
      : m_output( result ), m_error( error )
   {
      const char *environment = std::getenv( "INCLUDE" );

      for( const std::string &path : includePaths )
         m_paths.emplace_back( path );

      if( environment != nullptr )
      {
         std::istringstream list( environment );
         std::string path;

         while( std::getline( list, path, IIF( X_OS_WINDOWS, ';', ':' ) ) )
         {
            if( !path.empty() )
               m_paths.emplace_back( path );
         }
      }
   }

   /* -D name[=value], read as a #define line */
   bool configure( const std::vector<std::string> &values )
   {
      for( const std::string &value : values )
      {
         const size_t equals = value.find( '=' );
         const std::string name = value.substr( 0, equals );
         const X_SOURCE_LOCATION where = { "-D" + value, 1 };
         std::string body;
         PP_TOKENS tokens;
         PP_STATE state;

         if( !s_isIdentifier( name ) )
            return fail( where, "nieprawidlowa nazwa definicji" );

         if( equals != std::string::npos )
            body = value.substr( equals + 1 );

         if( !tokenize( "#define " + name + " " + body, tokens, state, where ) )
            return false;

         if( state.comment )
            return fail( where, "niedomkniety komentarz w definicji" );

         if( !directive( tokens, state, where, 0 ) )
            return false;
      }

      return true;
   }

   bool read( const std::filesystem::path &file, size_t depth )
   {
      X_SOURCE_LOCATION where = { file.lexically_normal().string(), 1 };
      std::ifstream stream;
      std::string line;
      PP_TOKENS tokens;
      PP_STATE state;
      bool continuation = false;
      int lineNumber = 0;

      if( depth > 15 )
         return fail( where, "przekroczono 15 poziomow #include" );

      stream.open( file, std::ios::binary );
      if( !stream )
         return fail( where, "nie mozna odczytac pliku" );

      m_output.files.push_back( where.file );

      while( std::getline( stream, line ) )
      {
         ++lineNumber;

         if( !line.empty() && line.back() == '\r' )
            line.pop_back();

         /* UTF-8 BOM */
         if( lineNumber == 1 && line.compare( 0, 3, "\xEF\xBB\xBF" ) == 0 )
            line.erase( 0, 3 );

         if( !continuation )
            where.line = lineNumber;

         if( line.find( '\0' ) != std::string::npos )
            return fail( where, "bajt NUL w pliku zrodlowym" );

         if( !tokenize( line, tokens, state, where ) )
            return false;

         /* ; at the end of a line: the next line continues the statement */
         continuation = !tokens.empty() && tokens.back().text == ";";
         if( continuation )
         {
            tokens.pop_back();
            continue;
         }

         if( !tokens.empty() && tokens[0].text == "#" )
         {
            if( !directive( tokens, state, where, depth ) )
               return false;
         }
         else if( state.active )
         {
            if( !emit( tokens, where ) )
               return false;
         }

         tokens.clear();
      }

      if( stream.bad() )
         return fail( where, "blad odczytu pliku" );

      if( state.comment )
         return fail( where, "niedomkniety komentarz" );

      if( continuation )
         return fail( where, "brak dalszego ciagu po ;" );

      if( !state.conditions.empty() )
      {
         where.line = state.conditions.back().line;
         return fail( where, "brak #endif" );
      }

      return true;
   }
};

} // namespace

/* ---------------------------------------------------------------------------------------------
   Interface
   --------------------------------------------------------------------------------------------- */

std::string X_PREPROCESSED::location( int line ) const
{
   const size_t index = IIF( line > 0, ( size_t )line - 1, 0 );

   if( lines.empty() )
      return "linia " + std::to_string( line );

   return s_where( lines[std::min( index, lines.size() - 1 )] );
}

bool x_preprocess( const std::string &file, const std::vector<std::string> &includePaths,
                   const std::vector<std::string> &definitions, X_PREPROCESSED &result, std::string &error )
{
   result = X_PREPROCESSED{};

   X_PREPROCESSOR preprocessor( includePaths, result, error );

   if( !preprocessor.configure( s_platformDefinitions() ) || !preprocessor.configure( definitions ) )
      return false;

   return preprocessor.read( file, 0 );
}
