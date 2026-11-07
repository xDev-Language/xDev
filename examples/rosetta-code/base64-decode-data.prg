/*
 * $DOC$
 * $NAME$
 * Base64 decode data
 *
 * $ONELINER$
 * Demonstrates the Base64 decode data task.
 *
 * $DESCRIPTION$
 * Implements the Base64 decode data task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/base64-decode-data.prg
 *
 * $OUTPUT$
 * To err is human, but to really foul things up you need a computer.
 *     -- Paul R. Ehrlich
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cData := "VG8gZXJyIGlzIGh1bWFuLCBidXQgdG8gcmVhbGx5IGZvdWwgdGhpbmdzIHVwIHlvdSBuZWVkIGEgY29tcHV0ZXIuCiAgICAtLSBQYXVsIFIuIEVocmxpY2g="

   QOut( Base64Decode( cData ) )

   RETURN
