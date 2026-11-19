/*
 * $DOC$
 * $NAME$
 * Invoice plugin
 *
 * $ONELINER$
 * A DLL module of the plugin shell.
 *
 * $DESCRIPTION$
 * Built with xdev --shared. The shell calls Start() and shows the text it returns.
 *
 * $EXAMPLES$
 * examples/plugin/plugins/Invoice.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

STATIC s_nRate := 23

FUNCTION Start()

   RETURN "invoices ready, VAT " + LTrim( Str( s_nRate ) ) + "%"
