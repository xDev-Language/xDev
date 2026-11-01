/*
 *
 */

PROCEDURE Main()

   Check( 'Transform( "Łódź", "XXXX" )        ', Transform( "Łódź", "XXXX" ), "Łódź" )
   Check( 'Transform( "Łódź", "XX" )          ', Transform( "Łódź", "XX" ), "Łó" )
   Check( 'Transform( "ąęśćźż", "@R XX-XX" )  ', Transform( "ąęśćźż", "@R XX-XX" ), "ąę-ść" )
   Check( 'Transform( "żółw", "@R X.X.X.X" )  ', Transform( "żółw", "@R X.X.X.X" ), "ż.ó.ł.w" )
   Check( 'Transform( "zażółć", "@!" )        ', Transform( "zażółć", "@!" ), "ZAżółć" )
   Check( 'Transform( "abc", "@R !!-X" )      ', Transform( "abc", "@R !!-X" ), "AB-c" )
   Check( 'Transform( "ab", "@R XX-XX" )      ', Transform( "ab", "@R XX-XX" ), "ab-  " )
   Check( 'Transform( "kółko", "X-XX" )       ', Transform( "kółko", "X-XX" ), "k-łk" )

RETURN

STATIC PROCEDURE Check( cTitle, cResult, cExpected )

   QOut( cTitle + IIF( cResult == cExpected, "OK", "ERROR: [" + cResult + "]" ) )

RETURN
