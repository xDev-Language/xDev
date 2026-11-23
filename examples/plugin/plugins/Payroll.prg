/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Examples
 * File......: Payroll.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * Payroll plugin
 *
 * $ONELINER$
 * A DLL module of the plugin shell.
 *
 * $DESCRIPTION$
 * Built with xdev --shared. The shell calls Start() and shows the text it returns.
 *
 * $EXAMPLES$
 * examples/plugin/plugins/Payroll.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

STATIC s_aEmployees := { "Anna", "Łukasz", "Zofia" }

FUNCTION Start()

   RETURN "payroll ready, " + LTrim( Str( Len( s_aEmployees ) ) ) + " employees"


