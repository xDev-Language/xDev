/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Frontend
 * File......: semantic.cpp
 *
 */

#include "semantic.hpp"
#include "constant.hpp"
#include "typecheck.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_map>

static std::string s_key( const std::string &name )
{
   std::string key = name;

   for( char &ch : key )
      ch = ( char )std::toupper( ( unsigned char )ch );

   return key;
}

/* Self and Super are reserved inside methods */
static bool s_isReserved( const std::string &upper )
{
   return upper == "SELF" || upper == "SUPER";
}

static bool s_isDeclaration( X_STMT_KIND kind )
{
   return kind == X_STMT_KIND::LOCAL || kind == X_STMT_KIND::STATIC || kind == X_STMT_KIND::FIELD;
}

static X_BINDING_KIND s_bindingKind( X_STMT_KIND kind )
{
   if( kind == X_STMT_KIND::LOCAL )
      return X_BINDING_KIND::LOCAL;

   if( kind == X_STMT_KIND::STATIC )
      return X_BINDING_KIND::STATIC;

   return X_BINDING_KIND::FIELD;
}

/* A statement that names a variable: the FOR counter, the ASSIGN target or RECOVER USING */
static bool s_namesVariable( const X_STMT &stmt )
{
   if( stmt.name.empty() )
      return false;

   return stmt.kind == X_STMT_KIND::FOR || stmt.kind == X_STMT_KIND::ASSIGN || stmt.kind == X_STMT_KIND::SEQUENCE;
}

/* Class members without a method body */
static bool s_withoutBody( const std::string &kind )
{
   return kind == "DATA" || kind == "CLASSDATA" || kind == "VIRTUAL" || kind == "DELEGATE";
}

static bool s_isData( const X_CLASS_MEMBER &member )
{
   return member.kind == "DATA" || member.kind == "CLASSDATA";
}

/* The messages a member answers by its name: a DATA also _name, an ASSIGN only _name; DESTRUCTOR and
   ON ERROR answer none */
static std::vector<std::string> s_nameMessages( const X_CLASS_MEMBER &member )
{
   if( s_isData( member ) )
      return { member.name, "_" + member.name };

   if( member.kind == "ASSIGN" )
      return { "_" + member.name };

   if( member.kind == "DESTRUCTOR" || member.kind == "ONERROR" )
      return {};

   return { member.name };
}

/* A DATA and a member that is not a DATA answer the same message: the one would hide the other */
static bool s_membersCollide( const X_CLASS_MEMBER &member, const X_CLASS_MEMBER &inherited )
{
   if( s_isData( member ) == s_isData( inherited ) )
      return false;

   for( const std::string &message : s_nameMessages( member ) )
   {
      for( const std::string &other : s_nameMessages( inherited ) )
      {
         if( message == other )
            return true;
      }
   }

   return false;
}

namespace
{

/* Name resolution: scopes, BLOCK captures, calls and class definitions */
class X_RESOLVER
{
private:
   X_PROGRAM &m_program;
   std::string &m_error;
   const X_LOCATION &m_location;
   std::unordered_map<std::string, const X_FUNCTION *> m_functions;
   std::unordered_map<std::string, const X_CLASS_DECL *> m_classes;
   std::vector<X_EXPR *> m_blocks;
   std::vector<std::string> *m_warnings;
   /* How a variable is used: X_READ, X_WRITTEN or both */
   std::unordered_map<const X_BINDING *, int> m_access;

   static constexpr int X_READ = 1;
   static constexpr int X_WRITTEN = 2;

   std::string where( int line ) const
   {
      if( m_location )
         return m_location( line );

      return "<input>:" + std::to_string( line );
   }

   bool fail( int line, const std::string &text )
   {
      m_error = where( line ) + ": error: " + text;

      return false;
   }

   /* ---------------------------------------------------------------------------------------------
      Declarations
      --------------------------------------------------------------------------------------------- */

   /* A repeated untyped LOCAL is allowed; STATIC, FIELD and typed names must be unique */
   bool declare( X_SCOPE &scope, const X_LOCAL_DECL &decl, X_BINDING_KIND kind )
   {
      const std::string upper = s_key( decl.name );
      const X_DECL_TYPE type = decl.declaredType.value_or( X_DECL_TYPE::ANY );
      const auto found = scope.symbols.find( upper );

      if( found == scope.symbols.end() )
      {
         scope.symbols.emplace( upper, X_BINDING{ decl.name, kind, type, decl.declaredTypeName, decl.line, &decl } );
         return true;
      }

      if( kind == X_BINDING_KIND::STATIC || found->second.kind == X_BINDING_KIND::STATIC )
         return fail( decl.line, "STATIC koliduje z parametrem/LOCAL lub powtarza deklaracje: " + decl.name );

      if( kind == X_BINDING_KIND::FIELD || found->second.kind == X_BINDING_KIND::FIELD )
         return fail( decl.line, "FIELD koliduje ze zmienna: " + decl.name );

      if( type != X_DECL_TYPE::ANY || found->second.type != X_DECL_TYPE::ANY )
         return fail( decl.line, "powtorzona deklaracja zmiennej z typem: " + decl.name );

      return true;
   }

   /* LOCAL, STATIC and FIELD anywhere in the body belong to the whole function */
   bool declarations( const std::vector<X_STMT> &body, X_SCOPE &scope, bool method )
   {
      for( const X_STMT &stmt : body )
      {
         if( s_isDeclaration( stmt.kind ) )
         {
            for( const X_LOCAL_DECL &decl : stmt.locals )
            {
               if( method && s_isReserved( s_key( decl.name ) ) )
                  return fail( decl.line, "zastrzezona nazwa w metodzie: " + decl.name );

               if( !declare( scope, decl, s_bindingKind( stmt.kind ) ) )
                  return false;
            }
         }

         if( !declarations( stmt.body, scope, method ) || !declarations( stmt.elseBody, scope, method ) )
            return false;

         if( !declarations( stmt.otherwiseBody, scope, method ) )
            return false;

         for( const X_CASE_BRANCH &branch : stmt.cases )
         {
            if( !declarations( branch.body, scope, method ) )
               return false;
         }
      }

      return true;
   }

   /* Parameters and Self open the scope of a function */
   bool parameters( X_FUNCTION &function )
   {
      function.scope.symbols.clear();

      for( const std::string &param : function.params )
      {
         const std::string upper = s_key( param );
         const auto type = function.paramTypes.find( upper );
         const auto typeName = function.paramTypeNames.find( upper );
         X_BINDING binding{ param, X_BINDING_KIND::PARAMETER, X_DECL_TYPE::ANY, std::string(), function.line };

         if( !function.className.empty() && s_isReserved( upper ) )
            return fail( function.line, "zastrzezona nazwa w metodzie: " + param );

         if( type != function.paramTypes.end() )
            binding.type = type->second;

         if( typeName != function.paramTypeNames.end() )
            binding.typeName = typeName->second;

         function.scope.symbols.emplace( upper, binding );
      }

      if( !function.className.empty() && function.methodKind != "CLASSMETHOD" )
      {
         X_BINDING self{ "SELF", X_BINDING_KIND::SELF, X_DECL_TYPE::OBJECT, function.className, function.line };

         function.scope.symbols.emplace( "SELF", self );
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Names
      --------------------------------------------------------------------------------------------- */

   /* The function scope first, then the module scope */
   const X_BINDING *lookup( const std::string &upper, const X_FUNCTION &function ) const
   {
      const auto local = function.scope.symbols.find( upper );
      const auto module = m_program.scope.symbols.find( upper );

      if( local != function.scope.symbols.end() )
         return &local->second;

      if( module != m_program.scope.symbols.end() )
         return &module->second;

      return nullptr;
   }

   /* written: the name is only the target of := or RECOVER USING, its value is not read */
   bool name( const std::string &text, int line, X_FUNCTION &function, bool receiver = false, bool written = false )
   {
      const std::string upper = s_key( text );
      const X_BINDING *symbol;

      if( !function.className.empty() && s_isReserved( upper ) )
      {
         if( function.methodKind == "CLASSMETHOD" && !receiver )
            return fail( line, "CLASSMETHOD: Self dostepne tylko jako odbiorca Self:skladowa lub ::skladowa" );

         if( upper == "SUPER" || function.methodKind == "CLASSMETHOD" )
            return true;
      }

      /* A block parameter: the blocks inside the block that declares it capture it */
      for( size_t i = m_blocks.size(); i > 0; --i )
      {
         if( m_blocks[i - 1]->scope.symbols.count( upper ) == 0 )
            continue;

         for( size_t j = i; j < m_blocks.size(); ++j )
         {
            std::vector<std::string> &captures = m_blocks[j]->captures;

            if( std::find( captures.begin(), captures.end(), upper ) == captures.end() )
               captures.push_back( upper );
         }

         return true;
      }

      symbol = lookup( upper, function );
      if( !symbol )
         return fail( line, "nieznana zmienna: " + text );

      if( symbol->kind == X_BINDING_KIND::FIELD )
         return fail( line, "FIELD nie jest magazynem zmiennej: " + text );

      m_access[symbol] |= IIF( written, X_WRITTEN, X_READ );

      /* Every open block captures a LOCAL or a parameter */
      if( m_blocks.empty() || symbol->kind == X_BINDING_KIND::STATIC )
         return true;

      if( symbol->type == X_DECL_TYPE::STRUCT )
         return fail( line, "STRUCT nie moze byc przechwycony przez BLOCK" );

      for( X_EXPR *block : m_blocks )
      {
         std::vector<std::string> &captures = block->captures;

         if( std::find( captures.begin(), captures.end(), upper ) == captures.end() )
            captures.push_back( upper );
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Expressions
      --------------------------------------------------------------------------------------------- */

   /* {| params | ... }: a new scope of block parameters; the captured names are sorted */
   bool block( X_EXPR &expr, X_FUNCTION &function )
   {
      expr.scope.symbols.clear();
      expr.captures.clear();

      for( const std::string &param : expr.blockParams )
      {
         X_BINDING binding{ param, X_BINDING_KIND::BLOCK_PARAMETER, X_DECL_TYPE::ANY, {}, expr.line, nullptr };

         expr.scope.symbols.emplace( s_key( param ), binding );
      }

      m_blocks.push_back( &expr );

      for( X_EXPR_PTR &arg : expr.args )
      {
         if( !expression( *arg, function ) )
            return false;
      }

      m_blocks.pop_back();
      std::sort( expr.captures.begin(), expr.captures.end() );

      return true;
   }

   /* Internal class functions, INIT / EXIT procedures and functions computed at compile time */
   bool call( const X_EXPR &expr )
   {
      const std::string called = s_key( expr.text );
      const auto found = m_functions.find( called );
      const bool isCall = expr.kind == X_EXPR_KIND::CALL;
      std::string function;

      if( !expr.internalCall && called.rfind( "__XDEV", 0 ) == 0 )
         return fail( expr.line, "nie mozna wywolywac wewnetrznych funkcji klas: " + expr.text );

      if( found != m_functions.end() && found->second->role != X_PROC_ROLE::NORMAL )
         return fail( expr.line, "INIT/EXIT PROCEDURE nie mozna wywolywac bezposrednio: " + expr.text );

      if( isCall && ( called == "PCOUNT" || called == "APARAMS" ) && !expr.args.empty() )
      {
         function = IIF( called == "PCOUNT", "PCount", "AParams" );
         return fail( expr.line, function + "() nie przyjmuje argumentow" );
      }

      /* Unused( var, ... ) marks variables as used on purpose; it makes no code */
      if( isCall && called == "UNUSED" )
      {
         if( expr.args.empty() )
            return fail( expr.line, "Unused() wymaga co najmniej jednej zmiennej" );

         for( size_t i = 0; i < expr.args.size(); ++i )
         {
            if( expr.args[i]->kind != X_EXPR_KIND::NAME || expr.argByReference[i] )
               return fail( expr.line, "Unused() przyjmuje tylko nazwy zmiennych" );
         }
      }

      if( isCall && ( called == "PROCNAME" || called == "PROCLINE" ) )
      {
         function = IIF( called == "PROCNAME", "ProcName", "ProcLine" );

         if( expr.args.size() > 1 )
            return fail( expr.line, function + "() przyjmuje najwyzej jeden argument" );
      }

      return true;
   }

   bool expression( X_EXPR &expr, X_FUNCTION &function, bool receiver = false )
   {
      if( expr.kind == X_EXPR_KIND::NAME )
         return name( expr.text, expr.line, function, receiver );

      if( expr.kind == X_EXPR_KIND::BLOCK )
         return block( expr, function );

      /* name := value writes the variable without reading it */
      if( expr.kind == X_EXPR_KIND::ASSIGN && expr.assignOp == X_ASSIGN_OP::SET && expr.args[0]->kind == X_EXPR_KIND::NAME )
      {
         if( !name( expr.args[0]->text, expr.args[0]->line, function, false, true ) )
            return false;

         return expression( *expr.args[1], function );
      }

      if( expr.kind == X_EXPR_KIND::CALL || expr.kind == X_EXPR_KIND::FUNCTION_ADDRESS )
      {
         if( !call( expr ) )
            return false;
      }

      /* The receiver of a message may be Self in a CLASSMETHOD */
      for( size_t i = 0; i < expr.args.size(); ++i )
      {
         const bool isReceiver = expr.kind == X_EXPR_KIND::SEND && i == 0;

         if( !expression( *expr.args[i], function, isReceiver ) )
            return false;
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Statements
      --------------------------------------------------------------------------------------------- */

   /* REQUEST names an external symbol for the linker */
   bool requests( const X_STMT &stmt )
   {
      for( const std::string &requested : stmt.requests )
      {
         const auto found = m_functions.find( s_key( requested ) );

         if( found == m_functions.end() )
            continue;

         if( found->second->isStatic || found->second->role != X_PROC_ROLE::NORMAL )
            return fail( stmt.line, "REQUEST wymaga symbolu zewnetrznego, nie STATIC/INIT/EXIT: " + requested );
      }

      return true;
   }

   bool statements( std::vector<X_STMT> &body, X_FUNCTION &function )
   {
      for( X_STMT &stmt : body )
      {
         X_EXPR *values[] = { stmt.expr.get(), stmt.target.get(), stmt.forLimit.get(), stmt.forStep.get() };

         /* The FOR counter is read by the loop; := and RECOVER USING only write */
         if( s_namesVariable( stmt ) && !name( stmt.name, stmt.line, function, false, stmt.kind != X_STMT_KIND::FOR ) )
            return false;

         for( X_EXPR *value : values )
         {
            if( value && !expression( *value, function ) )
               return false;
         }

         for( X_LOCAL_DECL &decl : stmt.locals )
         {
            if( decl.init && !expression( *decl.init, function ) )
               return false;

            for( X_EXPR_PTR &dim : decl.dimensions )
            {
               if( !expression( *dim, function ) )
                  return false;
            }
         }

         if( !statements( stmt.body, function ) || !statements( stmt.elseBody, function ) )
            return false;

         if( !statements( stmt.otherwiseBody, function ) )
            return false;

         for( X_CASE_BRANCH &branch : stmt.cases )
         {
            if( branch.condition && !expression( *branch.condition, function ) )
               return false;

            if( !statements( branch.body, function ) )
               return false;
         }

         if( !requests( stmt ) )
            return false;
      }

      return true;
   }

   /* LOCAL, STATIC and parameters of a function whose value is never read, in the order of their lines; the
      parameters of a method are left out, their number is fixed by the declaration in the class */
   void unusedVariables( const X_FUNCTION &function )
   {
      const bool method = !function.className.empty();
      const std::string owner = IIF( method, function.className + ":" + function.methodName, function.name );
      std::vector<const X_BINDING *> unused;

      for( const auto &entry : function.scope.symbols )
      {
         const X_BINDING &binding = entry.second;
         const bool variable = binding.kind == X_BINDING_KIND::LOCAL || binding.kind == X_BINDING_KIND::STATIC;
         const bool parameter = binding.kind == X_BINDING_KIND::PARAMETER && !method;
         const auto access = m_access.find( &binding );

         if( !variable && !parameter )
            continue;

         if( access == m_access.end() || !( access->second & X_READ ) )
            unused.push_back( &binding );
      }

      std::sort( unused.begin(), unused.end(), []( const X_BINDING *a, const X_BINDING *b ) { return a->line < b->line; } );

      for( const X_BINDING *binding : unused )
      {
         const auto access = m_access.find( binding );
         const bool parameter = binding->kind == X_BINDING_KIND::PARAMETER;
         const bool initialized = binding->declaration != nullptr && binding->declaration->init != nullptr;
         const bool written = initialized || ( access != m_access.end() && ( access->second & X_WRITTEN ) );
         const std::string what = IIF( parameter, "parametr ", "zmienna " );
         std::string text;

         if( written )
            text = what + binding->name + " ma przypisana wartosc, ale nie jest odczytywan" + IIF( parameter, "y", "a" );
         else
            text = what + binding->name + " nie jest uzywan" + IIF( parameter, "y", "a" );

         m_warnings->push_back( where( binding->line ) + ": warning: " + text + " w " + owner + "()" );
      }
   }

   /* ---------------------------------------------------------------------------------------------
      Classes
      --------------------------------------------------------------------------------------------- */

   /* Depth-first walk over the parents; states: 1 = being visited, 2 = done */
   bool inheritance( const X_CLASS_DECL &cls, std::unordered_map<std::string, int> &states )
   {
      const std::string className = s_key( cls.name );
      std::set<std::string> parents;

      if( states[className] == 1 )
         return fail( cls.line, "cykl dziedziczenia: " + cls.name );

      if( states[className] == 2 )
         return true;

      states[className] = 1;

      for( const std::string &parent : cls.parents )
      {
         const auto found = m_classes.find( s_key( parent ) );

         if( !parents.insert( s_key( parent ) ).second )
            return fail( cls.line, "powtorzona klasa bazowa: " + parent );

         if( found != m_classes.end() && !inheritance( *found->second, states ) )
            return false;
      }

      states[className] = 2;

      return true;
   }

   /* The members of the class against the members of one ancestor of this module, then of its parents */
   bool ancestorCollisions( const X_CLASS_DECL &cls, const std::string &ancestorName )
   {
      const auto found = m_classes.find( s_key( ancestorName ) );

      if( found == m_classes.end() )
         return true;

      const X_CLASS_DECL &ancestor = *found->second;

      for( const X_CLASS_MEMBER &member : cls.members )
      {
         for( const X_CLASS_MEMBER &inherited : ancestor.members )
         {
            if( s_membersCollide( member, inherited ) )
            {
               const std::string place = cls.name + ":" + member.name;

               return fail( cls.line, "kolizja nazw pola i metody z klasa bazowa " + ancestor.name + ": " + place );
            }
         }
      }

      for( const std::string &parent : ancestor.parents )
      {
         if( !ancestorCollisions( cls, parent ) )
            return false;
      }

      return true;
   }

   /* A subclass may not give a DATA of an ancestor's name to a method, nor a method's name to a DATA */
   bool memberCollisions( const X_CLASS_DECL &cls )
   {
      for( const std::string &parent : cls.parents )
      {
         if( !ancestorCollisions( cls, parent ) )
            return false;
      }

      return true;
   }

   /* Every method of the class has a body with the declared number of parameters */
   bool memberDefinitions( const X_CLASS_DECL &cls )
   {
      for( const X_CLASS_MEMBER &member : cls.members )
      {
         const std::string symbol = "__XDEV_" + cls.name + "_" + member.kind + "_" + member.name;
         const auto found = m_functions.find( s_key( symbol ) );

         if( s_withoutBody( member.kind ) )
            continue;

         if( found == m_functions.end() || found->second->params.size() != member.params.size() )
            return fail( cls.line, "brak zgodnej definicji " + member.kind + " " + cls.name + ":" + member.name );
      }

      return true;
   }

   /* A method body belongs to a class of this module; imported classes are registered elsewhere */
   bool methodDeclaration( const X_FUNCTION &function )
   {
      const std::string className = s_key( function.className );
      const X_CLASS_DECL *cls = nullptr;
      const X_CLASS_MEMBER *declaration = nullptr;

      for( const X_CLASS_DECL &own : m_program.classes )
      {
         if( s_key( own.name ) == className )
         {
            cls = &own;
            break;
         }
      }

      for( size_t i = 0; cls && i < cls->members.size(); ++i )
      {
         const X_CLASS_MEMBER &member = cls->members[i];

         if( member.name != function.methodName )
            continue;

         if( member.kind == "DELEGATE" || member.kind == "VIRTUAL" )
            return fail( function.line, member.kind + " nie moze miec ciala metody: " + function.methodName );

         if( member.kind == function.methodKind )
            declaration = &member;
      }

      if( !declaration )
         return fail( function.line, "metoda bez deklaracji: " + function.className + ":" + function.methodName );

      return true;
   }

   bool classDefinitions()
   {
      std::unordered_map<std::string, int> states;

      for( const X_CLASS_DECL &cls : m_program.classes )
      {
         if( !inheritance( cls, states ) || !memberCollisions( cls ) || !memberDefinitions( cls ) )
            return false;
      }

      for( const X_FUNCTION &function : m_program.functions )
      {
         if( function.className.empty() || function.isDataInitializer )
            continue;

         if( !methodDeclaration( function ) )
            return false;
      }

      return true;
   }

   /* ---------------------------------------------------------------------------------------------
      Program
      --------------------------------------------------------------------------------------------- */

   /* Functions, DLL functions and classes by upper-case name */
   bool registerSymbols( bool libraryModule )
   {
      for( const X_FUNCTION &function : m_program.functions )
      {
         if( !m_functions.emplace( s_key( function.name ), &function ).second )
            return fail( function.line, "powtorzona funkcja: " + function.name );

         if( !libraryModule && function.isStatic && s_key( function.name ) == "MAIN" )
            return fail( function.line, "STATIC Main nie moze byc punktem wejscia" );
      }

      for( const X_DLL_FUNCTION &function : m_program.dllFunctions )
      {
         if( !m_functions.emplace( s_key( function.name ), &function ).second )
            return fail( function.line, "powtorzona funkcja: " + function.name );
      }

      for( const X_CLASS_DECL &cls : m_program.classes )
      {
         if( !m_classes.emplace( s_key( cls.name ), &cls ).second )
            return fail( cls.line, "powtorzona deklaracja klasy: " + cls.name );
      }

      for( const X_CLASS_DECL &cls : m_program.importedClasses )
         m_classes.emplace( s_key( cls.name ), &cls );

      return true;
   }

public:
   X_RESOLVER( X_PROGRAM &program, std::string &error, const X_LOCATION &location, std::vector<std::string> *warnings )
      : m_program( program ), m_error( error ), m_location( location ), m_warnings( warnings )
   {
   }

   bool run( bool libraryModule )
   {
      X_FUNCTION module;
      std::vector<std::string> errors;

      m_program.symbolsResolved = false;
      m_program.scope.symbols.clear();

      if( !registerSymbols( libraryModule ) )
         return false;

      if( !classDefinitions() || !declarations( m_program.statements, m_program.scope, false ) )
         return false;

      /* Outside functions: only STATIC, FIELD and REQUEST */
      for( const X_STMT &stmt : m_program.statements )
      {
         if( !s_isDeclaration( stmt.kind ) && stmt.kind != X_STMT_KIND::REQUEST )
            return fail( stmt.line, "kod poza FUNCTION/PROCEDURE" );
      }

      if( !statements( m_program.statements, module ) )
         return false;

      /* The first error of every function is reported; the next function starts clean */
      for( X_FUNCTION &function : m_program.functions )
      {
         if( !resolveFunction( function ) )
         {
            errors.push_back( m_error );
            m_error.clear();
            m_blocks.clear();
         }
      }

      if( !errors.empty() )
      {
         m_error = x_joinLines( errors );
         return false;
      }

      m_program.symbolsResolved = true;

      return true;
   }

   /* The names of one function */
   bool resolveFunction( X_FUNCTION &function )
   {
      if( !parameters( function ) )
         return false;

      if( !declarations( function.body, function.scope, !function.className.empty() ) )
         return false;

      if( !statements( function.body, function ) )
         return false;

      for( X_LOCAL_DECL &decl : function.dataInitializers )
      {
         if( decl.init && !expression( *decl.init, function ) )
            return false;
      }

      if( m_warnings != nullptr && !function.isDataInitializer )
         unusedVariables( function );

      return true;
   }
};

} // namespace

/* ---------------------------------------------------------------------------------------------
   Semantic analysis: names, then types, then constants
   --------------------------------------------------------------------------------------------- */

bool x_analyzeProgram( X_PROGRAM &program, std::string &error, bool libraryModule, const X_LOCATION &location,
                       std::vector<std::string> *warnings )
{
   X_RESOLVER resolver( program, error, location, warnings );

   if( !resolver.run( libraryModule ) )
      return false;

   if( !x_checkTypes( program, error, location ) )
      return false;

   return x_validateConstants( program, error, location );
}
