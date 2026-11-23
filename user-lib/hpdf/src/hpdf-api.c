/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev HPDF
 * File......: hpdf-api.c
 *
 */

#include "internal-hpdf.h"

// HPDF_3DAnnotExData_Set3DMeasurement( exdata, measure ) --> nStatus
X_FUNC( HPDF_3DANNOTEXDATA_SET3DMEASUREMENT )
{
   HPDF_ExData exdata = ( HPDF_ExData )x_parptr( ctx, 1 );
   HPDF_3DMeasure measure = ( HPDF_3DMeasure )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_3DAnnotExData_Set3DMeasurement( exdata, measure ) );
}

// HPDF_3DC3DMeasure_SetProjectionAnotation( measure, annotation ) --> nStatus
X_FUNC( HPDF_3DC3DMEASURE_SETPROJECTIONANOTATION )
{
   HPDF_3DMeasure measure = ( HPDF_3DMeasure )x_parptr( ctx, 1 );
   HPDF_Annotation annotation = ( HPDF_Annotation )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_3DC3DMeasure_SetProjectionAnotation( measure, annotation ) );
}

// HPDF_3DC3DMeasure_SetText( measure, text, encoder ) --> nStatus
X_FUNC( HPDF_3DC3DMEASURE_SETTEXT )
{
   HPDF_3DMeasure measure = ( HPDF_3DMeasure )x_parptr( ctx, 1 );
   const char *text = x_hpdfString( ctx, 2 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_3DC3DMeasure_SetText( measure, text, encoder ) );
}

// HPDF_3DC3DMeasure_SetTextBoxSize( measure, x, y ) --> nStatus
X_FUNC( HPDF_3DC3DMEASURE_SETTEXTBOXSIZE )
{
   HPDF_3DMeasure measure = ( HPDF_3DMeasure )x_parptr( ctx, 1 );
   HPDF_INT32 x = ( HPDF_INT32 )x_parnint( ctx, 2 );
   HPDF_INT32 y = ( HPDF_INT32 )x_parnint( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_3DC3DMeasure_SetTextBoxSize( measure, x, y ) );
}

// HPDF_3DMeasure_SetColor( measure, color ) --> nStatus
X_FUNC( HPDF_3DMEASURE_SETCOLOR )
{
   HPDF_3DMeasure measure = ( HPDF_3DMeasure )x_parptr( ctx, 1 );
   HPDF_RGBColor color = x_hpdfRGBColor( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_3DMeasure_SetColor( measure, color ) );
}

// HPDF_3DMeasure_SetName( measure, name ) --> nStatus
X_FUNC( HPDF_3DMEASURE_SETNAME )
{
   x_retnint( ctx, ( int64_t )HPDF_3DMeasure_SetName( ( HPDF_3DMeasure )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_3DMeasure_SetTextSize( measure, textsize ) --> nStatus
X_FUNC( HPDF_3DMEASURE_SETTEXTSIZE )
{
   HPDF_3DMeasure measure = ( HPDF_3DMeasure )x_parptr( ctx, 1 );
   HPDF_REAL textsize = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_3DMeasure_SetTextSize( measure, textsize ) );
}

// HPDF_3DView_Add3DC3DMeasure( view, measure ) --> nStatus
X_FUNC( HPDF_3DVIEW_ADD3DC3DMEASURE )
{
   HPDF_Dict view = ( HPDF_Dict )x_parptr( ctx, 1 );
   HPDF_3DMeasure measure = ( HPDF_3DMeasure )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_3DView_Add3DC3DMeasure( view, measure ) );
}

// HPDF_AddPage( pdf ) --> pPage
X_FUNC( HPDF_ADDPAGE )
{
   x_hpdfRetHandle( ctx, HPDF_AddPage( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_AddPageLabel( pdf, page_num, style, first_page, prefix ) --> nStatus
X_FUNC( HPDF_ADDPAGELABEL )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_UINT pageNum = ( HPDF_UINT )x_parnint( ctx, 2 );
   HPDF_PageNumStyle style = ( HPDF_PageNumStyle )x_parnint( ctx, 3 );
   HPDF_UINT firstPage = ( HPDF_UINT )x_parnint( ctx, 4 );
   const char *prefix = x_hpdfString( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_AddPageLabel( pdf, pageNum, style, firstPage, prefix ) );
}

// HPDF_AddPDFAXmpExtension( pdf, xmp_description ) --> nStatus
X_FUNC( HPDF_ADDPDFAXMPEXTENSION )
{
   x_retnint( ctx, ( int64_t )HPDF_AddPDFAXmpExtension( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_Annotation_SetBorderStyle( annot, subtype, width, dash_on, dash_off, dash_phase ) --> nStatus
X_FUNC( HPDF_ANNOTATION_SETBORDERSTYLE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_BSSubtype subtype = ( HPDF_BSSubtype )x_parnint( ctx, 2 );
   HPDF_REAL width = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_UINT16 dashOn = ( HPDF_UINT16 )x_parnint( ctx, 4 );
   HPDF_UINT16 dashOff = ( HPDF_UINT16 )x_parnint( ctx, 5 );
   HPDF_UINT16 dashPhase = ( HPDF_UINT16 )x_parnint( ctx, 6 );

   x_retnint( ctx, ( int64_t )HPDF_Annotation_SetBorderStyle( annot, subtype, width, dashOn, dashOff, dashPhase ) );
}

// HPDF_Annot_Set3DView( mmgr, annot, annot3d, view ) --> nStatus
X_FUNC( HPDF_ANNOT_SET3DVIEW )
{
   HPDF_MMgr mmgr = ( HPDF_MMgr )x_parptr( ctx, 1 );
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 2 );
   HPDF_Annotation annot3d = ( HPDF_Annotation )x_parptr( ctx, 3 );
   HPDF_Dict view = ( HPDF_Dict )x_parptr( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_Annot_Set3DView( mmgr, annot, annot3d, view ) );
}

// HPDF_Annot_SetCMYKColor( annot, color ) --> nStatus
X_FUNC( HPDF_ANNOT_SETCMYKCOLOR )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_CMYKColor color = x_hpdfCMYKColor( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_Annot_SetCMYKColor( annot, color ) );
}

// HPDF_Annot_SetGrayColor( annot, color ) --> nStatus
X_FUNC( HPDF_ANNOT_SETGRAYCOLOR )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_REAL color = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Annot_SetGrayColor( annot, color ) );
}

// HPDF_Annot_SetNoColor( annot ) --> nStatus
X_FUNC( HPDF_ANNOT_SETNOCOLOR )
{
   x_retnint( ctx, ( int64_t )HPDF_Annot_SetNoColor( ( HPDF_Annotation )x_parptr( ctx, 1 ) ) );
}

// HPDF_Annot_SetRGBColor( annot, color ) --> nStatus
X_FUNC( HPDF_ANNOT_SETRGBCOLOR )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_RGBColor color = x_hpdfRGBColor( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_Annot_SetRGBColor( annot, color ) );
}

// HPDF_AppendOutputIntents( pdf, iccname, iccdict ) --> nStatus
X_FUNC( HPDF_APPENDOUTPUTINTENTS )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *iccname = x_hpdfString( ctx, 2 );
   HPDF_Dict iccdict = ( HPDF_Dict )x_parptr( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_AppendOutputIntents( pdf, iccname, iccdict ) );
}

// HPDF_AttachFile( pdf, file ) --> pEmbeddedFile
X_FUNC( HPDF_ATTACHFILE )
{
   x_hpdfRetHandle( ctx, HPDF_AttachFile( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_CheckError( error ) --> nStatus
X_FUNC( HPDF_CHECKERROR )
{
   x_retnint( ctx, ( int64_t )HPDF_CheckError( ( HPDF_Error )x_parptr( ctx, 1 ) ) );
}

// HPDF_CreateExtGState( pdf ) --> pExtGState
X_FUNC( HPDF_CREATEEXTGSTATE )
{
   x_hpdfRetHandle( ctx, HPDF_CreateExtGState( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_CreateOutline( pdf, parent, title, encoder ) --> pOutline
X_FUNC( HPDF_CREATEOUTLINE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_Outline parent = ( HPDF_Outline )x_parptr( ctx, 2 );
   const char *title = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_CreateOutline( pdf, parent, title, encoder ) );
}

// HPDF_Destination_SetFit( dst ) --> nStatus
X_FUNC( HPDF_DESTINATION_SETFIT )
{
   x_retnint( ctx, ( int64_t )HPDF_Destination_SetFit( ( HPDF_Destination )x_parptr( ctx, 1 ) ) );
}

// HPDF_Destination_SetFitB( dst ) --> nStatus
X_FUNC( HPDF_DESTINATION_SETFITB )
{
   x_retnint( ctx, ( int64_t )HPDF_Destination_SetFitB( ( HPDF_Destination )x_parptr( ctx, 1 ) ) );
}

// HPDF_Destination_SetFitBH( dst, top ) --> nStatus
X_FUNC( HPDF_DESTINATION_SETFITBH )
{
   HPDF_Destination dst = ( HPDF_Destination )x_parptr( ctx, 1 );
   HPDF_REAL top = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Destination_SetFitBH( dst, top ) );
}

// HPDF_Destination_SetFitBV( dst, left ) --> nStatus
X_FUNC( HPDF_DESTINATION_SETFITBV )
{
   HPDF_Destination dst = ( HPDF_Destination )x_parptr( ctx, 1 );
   HPDF_REAL left = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Destination_SetFitBV( dst, left ) );
}

// HPDF_Destination_SetFitH( dst, top ) --> nStatus
X_FUNC( HPDF_DESTINATION_SETFITH )
{
   HPDF_Destination dst = ( HPDF_Destination )x_parptr( ctx, 1 );
   HPDF_REAL top = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Destination_SetFitH( dst, top ) );
}

// HPDF_Destination_SetFitR( dst, left, bottom, right, top ) --> nStatus
X_FUNC( HPDF_DESTINATION_SETFITR )
{
   HPDF_Destination dst = ( HPDF_Destination )x_parptr( ctx, 1 );
   HPDF_REAL left = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL bottom = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL right = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL top = ( HPDF_REAL )x_parnd( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_Destination_SetFitR( dst, left, bottom, right, top ) );
}

// HPDF_Destination_SetFitV( dst, left ) --> nStatus
X_FUNC( HPDF_DESTINATION_SETFITV )
{
   HPDF_Destination dst = ( HPDF_Destination )x_parptr( ctx, 1 );
   HPDF_REAL left = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Destination_SetFitV( dst, left ) );
}

// HPDF_Destination_SetXYZ( dst, left, top, zoom ) --> nStatus
X_FUNC( HPDF_DESTINATION_SETXYZ )
{
   HPDF_Destination dst = ( HPDF_Destination )x_parptr( ctx, 1 );
   HPDF_REAL left = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL top = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL zoom = ( HPDF_REAL )x_parnd( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_Destination_SetXYZ( dst, left, top, zoom ) );
}

// HPDF_EmbeddedFile_SetAFRelationship( emfile, relationship ) --> nStatus
X_FUNC( HPDF_EMBEDDEDFILE_SETAFRELATIONSHIP )
{
   HPDF_EmbeddedFile emfile = ( HPDF_EmbeddedFile )x_parptr( ctx, 1 );
   HPDF_AFRelationship relationship = ( HPDF_AFRelationship )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_EmbeddedFile_SetAFRelationship( emfile, relationship ) );
}

// HPDF_EmbeddedFile_SetCreationDate( emfile, creationDate ) --> nStatus
X_FUNC( HPDF_EMBEDDEDFILE_SETCREATIONDATE )
{
   HPDF_EmbeddedFile emfile = ( HPDF_EmbeddedFile )x_parptr( ctx, 1 );
   HPDF_Date creationDate = x_hpdfDate( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_EmbeddedFile_SetCreationDate( emfile, creationDate ) );
}

// HPDF_EmbeddedFile_SetDescription( emfile, new_description ) --> nStatus
X_FUNC( HPDF_EMBEDDEDFILE_SETDESCRIPTION )
{
   HPDF_EmbeddedFile emfile = ( HPDF_EmbeddedFile )x_parptr( ctx, 1 );
   const char *newDescription = x_hpdfString( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_EmbeddedFile_SetDescription( emfile, newDescription ) );
}

// HPDF_EmbeddedFile_SetLastModificationDate( emfile, lastModificationDate ) --> nStatus
X_FUNC( HPDF_EMBEDDEDFILE_SETLASTMODIFICATIONDATE )
{
   HPDF_EmbeddedFile emfile = ( HPDF_EmbeddedFile )x_parptr( ctx, 1 );
   HPDF_Date lastModificationDate = x_hpdfDate( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_EmbeddedFile_SetLastModificationDate( emfile, lastModificationDate ) );
}

// HPDF_EmbeddedFile_SetName( emfile, name ) --> nStatus
X_FUNC( HPDF_EMBEDDEDFILE_SETNAME )
{
   HPDF_EmbeddedFile emfile = ( HPDF_EmbeddedFile )x_parptr( ctx, 1 );
   const char *name = x_hpdfString( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_EmbeddedFile_SetName( emfile, name ) );
}

// HPDF_EmbeddedFile_SetSize( emfile, size ) --> nStatus
X_FUNC( HPDF_EMBEDDEDFILE_SETSIZE )
{
   HPDF_EmbeddedFile emfile = ( HPDF_EmbeddedFile )x_parptr( ctx, 1 );
   HPDF_UINT64 size = ( HPDF_UINT64 )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_EmbeddedFile_SetSize( emfile, size ) );
}

// HPDF_EmbeddedFile_SetSubtype( emfile, subtype ) --> nStatus
X_FUNC( HPDF_EMBEDDEDFILE_SETSUBTYPE )
{
   HPDF_EmbeddedFile emfile = ( HPDF_EmbeddedFile )x_parptr( ctx, 1 );
   const char *subtype = x_hpdfString( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_EmbeddedFile_SetSubtype( emfile, subtype ) );
}

// HPDF_Encoder_GetByteType( encoder, text, index ) --> nValue
X_FUNC( HPDF_ENCODER_GETBYTETYPE )
{
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 1 );
   const char *text = x_hpdfString( ctx, 2 );
   HPDF_UINT index = ( HPDF_UINT )x_parnint( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_Encoder_GetByteType( encoder, text, index ) );
}

// HPDF_Encoder_GetType( encoder ) --> nValue
X_FUNC( HPDF_ENCODER_GETTYPE )
{
   x_retnint( ctx, ( int64_t )HPDF_Encoder_GetType( ( HPDF_Encoder )x_parptr( ctx, 1 ) ) );
}

// HPDF_Encoder_GetUnicode( encoder, code ) --> nValue
X_FUNC( HPDF_ENCODER_GETUNICODE )
{
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 1 );
   HPDF_UINT16 code = ( HPDF_UINT16 )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Encoder_GetUnicode( encoder, code ) );
}

// HPDF_Encoder_GetWritingMode( encoder ) --> nValue
X_FUNC( HPDF_ENCODER_GETWRITINGMODE )
{
   x_retnint( ctx, ( int64_t )HPDF_Encoder_GetWritingMode( ( HPDF_Encoder )x_parptr( ctx, 1 ) ) );
}

// HPDF_ExtGState_SetAlphaFill( ext_gstate, value ) --> nStatus
X_FUNC( HPDF_EXTGSTATE_SETALPHAFILL )
{
   HPDF_ExtGState extGstate = ( HPDF_ExtGState )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_ExtGState_SetAlphaFill( extGstate, value ) );
}

// HPDF_ExtGState_SetAlphaStroke( ext_gstate, value ) --> nStatus
X_FUNC( HPDF_EXTGSTATE_SETALPHASTROKE )
{
   HPDF_ExtGState extGstate = ( HPDF_ExtGState )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_ExtGState_SetAlphaStroke( extGstate, value ) );
}

// HPDF_ExtGState_SetBlendMode( ext_gstate, mode ) --> nStatus
X_FUNC( HPDF_EXTGSTATE_SETBLENDMODE )
{
   HPDF_ExtGState extGstate = ( HPDF_ExtGState )x_parptr( ctx, 1 );
   HPDF_BlendMode mode = ( HPDF_BlendMode )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_ExtGState_SetBlendMode( extGstate, mode ) );
}

// HPDF_Font_GetAscent( font ) --> nValue
X_FUNC( HPDF_FONT_GETASCENT )
{
   x_retnint( ctx, ( int64_t )HPDF_Font_GetAscent( ( HPDF_Font )x_parptr( ctx, 1 ) ) );
}

// HPDF_Font_GetBBox( font ) --> aRect
X_FUNC( HPDF_FONT_GETBBOX )
{
   x_hpdfRetRect( ctx, HPDF_Font_GetBBox( ( HPDF_Font )x_parptr( ctx, 1 ) ) );
}

// HPDF_Font_GetCapHeight( font ) --> nValue
X_FUNC( HPDF_FONT_GETCAPHEIGHT )
{
   x_retnint( ctx, ( int64_t )HPDF_Font_GetCapHeight( ( HPDF_Font )x_parptr( ctx, 1 ) ) );
}

// HPDF_Font_GetDescent( font ) --> nValue
X_FUNC( HPDF_FONT_GETDESCENT )
{
   x_retnint( ctx, ( int64_t )HPDF_Font_GetDescent( ( HPDF_Font )x_parptr( ctx, 1 ) ) );
}

// HPDF_Font_GetEncodingName( font ) --> cValue
X_FUNC( HPDF_FONT_GETENCODINGNAME )
{
   x_hpdfRetString( ctx, HPDF_Font_GetEncodingName( ( HPDF_Font )x_parptr( ctx, 1 ) ) );
}

// HPDF_Font_GetFontName( font ) --> cValue
X_FUNC( HPDF_FONT_GETFONTNAME )
{
   x_hpdfRetString( ctx, HPDF_Font_GetFontName( ( HPDF_Font )x_parptr( ctx, 1 ) ) );
}

// HPDF_Font_GetUnicodeWidth( font, code ) --> nValue
X_FUNC( HPDF_FONT_GETUNICODEWIDTH )
{
   HPDF_Font font = ( HPDF_Font )x_parptr( ctx, 1 );
   HPDF_UNICODE code = ( HPDF_UNICODE )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Font_GetUnicodeWidth( font, code ) );
}

// HPDF_Font_GetXHeight( font ) --> nValue
X_FUNC( HPDF_FONT_GETXHEIGHT )
{
   x_retnint( ctx, ( int64_t )HPDF_Font_GetXHeight( ( HPDF_Font )x_parptr( ctx, 1 ) ) );
}

// Number of bytes of the text that fit into nWidth; nRealWidth gets their width
// HPDF_Font_MeasureText( pFont, cText, nLen, nWidth, nFontSize, nCharSpace, nWordSpace, lWordWrap,
//                        @nRealWidth ) --> nBytes
X_FUNC( HPDF_FONT_MEASURETEXT )
{
   HPDF_Font font = ( HPDF_Font )x_parptr( ctx, 1 );
   const HPDF_BYTE *text = ( const HPDF_BYTE * )x_hpdfString( ctx, 2 );
   HPDF_UINT len = ( HPDF_UINT )x_parnint( ctx, 3 );
   HPDF_REAL width = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL fontSize = ( HPDF_REAL )x_parnd( ctx, 5 );
   HPDF_REAL charSpace = ( HPDF_REAL )x_parnd( ctx, 6 );
   HPDF_REAL wordSpace = ( HPDF_REAL )x_parnd( ctx, 7 );
   HPDF_BOOL wordWrap = x_hpdfBool( ctx, 8 );
   HPDF_REAL realWidth = 0;
   HPDF_UINT count;

   count = HPDF_Font_MeasureText( font, text, len, width, fontSize, charSpace, wordSpace, wordWrap, &realWidth );
   x_stornd( ctx, ( double )realWidth, 9 );
   x_retnint( ctx, ( int64_t )count );
}

// HPDF_Font_TextWidth( font, text, len ) --> aTextWidth
X_FUNC( HPDF_FONT_TEXTWIDTH )
{
   HPDF_Font font = ( HPDF_Font )x_parptr( ctx, 1 );
   const HPDF_BYTE *text = ( const HPDF_BYTE * )x_hpdfString( ctx, 2 );
   HPDF_UINT len = ( HPDF_UINT )x_parnint( ctx, 3 );

   x_hpdfRetTextWidth( ctx, HPDF_Font_TextWidth( font, text, len ) );
}

// HPDF_Free( pdf ) --> NIL
X_FUNC( HPDF_FREE )
{
   HPDF_Free( ( HPDF_Doc )x_parptr( ctx, 1 ) );
}

// HPDF_FreeDoc( pdf ) --> NIL
X_FUNC( HPDF_FREEDOC )
{
   HPDF_FreeDoc( ( HPDF_Doc )x_parptr( ctx, 1 ) );
}

// HPDF_FreeDocAll( pdf ) --> NIL
X_FUNC( HPDF_FREEDOCALL )
{
   HPDF_FreeDocAll( ( HPDF_Doc )x_parptr( ctx, 1 ) );
}

// HPDF_FreeTextAnnot_Set2PointCalloutLine( annot, startPoint, endPoint ) --> nStatus
X_FUNC( HPDF_FREETEXTANNOT_SET2POINTCALLOUTLINE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_Point startPoint = x_hpdfPoint( x_paritem( ctx, 2 ) );
   HPDF_Point endPoint = x_hpdfPoint( x_paritem( ctx, 3 ) );

   x_retnint( ctx, ( int64_t )HPDF_FreeTextAnnot_Set2PointCalloutLine( annot, startPoint, endPoint ) );
}

// HPDF_FreeTextAnnot_Set3PointCalloutLine( annot, startPoint, kneePoint, endPoint ) --> nStatus
X_FUNC( HPDF_FREETEXTANNOT_SET3POINTCALLOUTLINE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_Point startPoint = x_hpdfPoint( x_paritem( ctx, 2 ) );
   HPDF_Point kneePoint = x_hpdfPoint( x_paritem( ctx, 3 ) );
   HPDF_Point endPoint = x_hpdfPoint( x_paritem( ctx, 4 ) );

   x_retnint( ctx, ( int64_t )HPDF_FreeTextAnnot_Set3PointCalloutLine( annot, startPoint, kneePoint, endPoint ) );
}

// HPDF_FreeTextAnnot_SetDefaultStyle( annot, style ) --> nStatus
X_FUNC( HPDF_FREETEXTANNOT_SETDEFAULTSTYLE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   const char *style = x_hpdfString( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_FreeTextAnnot_SetDefaultStyle( annot, style ) );
}

// HPDF_FreeTextAnnot_SetLineEndingStyle( annot, startStyle, endStyle ) --> nStatus
X_FUNC( HPDF_FREETEXTANNOT_SETLINEENDINGSTYLE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_LineAnnotEndingStyle startStyle = ( HPDF_LineAnnotEndingStyle )x_parnint( ctx, 2 );
   HPDF_LineAnnotEndingStyle endStyle = ( HPDF_LineAnnotEndingStyle )x_parnint( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_FreeTextAnnot_SetLineEndingStyle( annot, startStyle, endStyle ) );
}

// Whole PDF file in memory
// HPDF_GetContents( pDoc, @cBuffer ) --> nStatus
X_FUNC( HPDF_GETCONTENTS )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_UINT32 size = 0;
   HPDF_STATUS status;
   HPDF_BYTE *buffer;

   status = HPDF_SaveToStream( pdf );
   if( status == HPDF_OK )
      size = HPDF_GetStreamSize( pdf );

   buffer = ( HPDF_BYTE * )x_xgrab( ( size_t )size + 1 );
   if( status == HPDF_OK )
      status = HPDF_GetContents( pdf, buffer, &size );

   if( status != HPDF_OK )
      size = 0;

   buffer[size] = '\0';
   x_storclen_buffer( ctx, ( char * )buffer, size, 2 );
   x_retnint( ctx, ( int64_t )status );
}

// HPDF_GetCurrentEncoder( pdf ) --> pEncoder
X_FUNC( HPDF_GETCURRENTENCODER )
{
   x_hpdfRetHandle( ctx, HPDF_GetCurrentEncoder( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetCurrentPage( pdf ) --> pPage
X_FUNC( HPDF_GETCURRENTPAGE )
{
   x_hpdfRetHandle( ctx, HPDF_GetCurrentPage( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetDocMMgr( doc ) --> pMMgr
X_FUNC( HPDF_GETDOCMMGR )
{
   x_hpdfRetHandle( ctx, HPDF_GetDocMMgr( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetEncoder( pdf, encoding_name ) --> pEncoder
X_FUNC( HPDF_GETENCODER )
{
   x_hpdfRetHandle( ctx, HPDF_GetEncoder( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_GetError( pdf ) --> nStatus
X_FUNC( HPDF_GETERROR )
{
   x_retnint( ctx, ( int64_t )HPDF_GetError( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetErrorDetail( pdf ) --> nStatus
X_FUNC( HPDF_GETERRORDETAIL )
{
   x_retnint( ctx, ( int64_t )HPDF_GetErrorDetail( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetFont( pdf, font_name, encoding_name ) --> pFont
X_FUNC( HPDF_GETFONT )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *fontName = x_hpdfString( ctx, 2 );
   const char *encodingName = x_hpdfString( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_GetFont( pdf, fontName, encodingName ) );
}

// HPDF_GetInfoAttr( pdf, type ) --> cValue
X_FUNC( HPDF_GETINFOATTR )
{
   x_hpdfRetString( ctx, HPDF_GetInfoAttr( ( HPDF_Doc )x_parptr( ctx, 1 ), ( HPDF_InfoType )x_parnint( ctx, 2 ) ) );
}

// HPDF_GetPageByIndex( pdf, index ) --> pPage
X_FUNC( HPDF_GETPAGEBYINDEX )
{
   x_hpdfRetHandle( ctx, HPDF_GetPageByIndex( ( HPDF_Doc )x_parptr( ctx, 1 ), ( HPDF_UINT )x_parnint( ctx, 2 ) ) );
}

// HPDF_GetPageLayout( pdf ) --> nValue
X_FUNC( HPDF_GETPAGELAYOUT )
{
   x_retnint( ctx, ( int64_t )HPDF_GetPageLayout( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetPageMMgr( page ) --> pMMgr
X_FUNC( HPDF_GETPAGEMMGR )
{
   x_hpdfRetHandle( ctx, HPDF_GetPageMMgr( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetPageMode( pdf ) --> nValue
X_FUNC( HPDF_GETPAGEMODE )
{
   x_retnint( ctx, ( int64_t )HPDF_GetPageMode( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetStreamSize( pdf ) --> nValue
X_FUNC( HPDF_GETSTREAMSIZE )
{
   x_retnint( ctx, ( int64_t )HPDF_GetStreamSize( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_GetTTFontDefFromFile( pdf, file_name, embedding ) --> pFontDef
X_FUNC( HPDF_GETTTFONTDEFFROMFILE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *fileName = x_hpdfString( ctx, 2 );
   HPDF_BOOL embedding = x_hpdfBool( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_GetTTFontDefFromFile( pdf, fileName, embedding ) );
}

// HPDF_GetVersion() --> cValue
X_FUNC( HPDF_GETVERSION )
{
   x_hpdfRetString( ctx, HPDF_GetVersion() );
}

// HPDF_GetViewerPreference( pdf ) --> nValue
X_FUNC( HPDF_GETVIEWERPREFERENCE )
{
   x_retnint( ctx, ( int64_t )HPDF_GetViewerPreference( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_HasDoc( pdf ) --> lValue
X_FUNC( HPDF_HASDOC )
{
   x_retl( ctx, HPDF_HasDoc( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_ICC_LoadIccFromMem( pdf, mmgr, iccdata, xref, numcomponent ) --> pOutputIntent
X_FUNC( HPDF_ICC_LOADICCFROMMEM )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_MMgr mmgr = ( HPDF_MMgr )x_parptr( ctx, 2 );
   HPDF_Stream iccdata = ( HPDF_Stream )x_parptr( ctx, 3 );
   HPDF_Xref xref = ( HPDF_Xref )x_parptr( ctx, 4 );
   int numcomponent = ( int )x_parnint( ctx, 5 );

   x_hpdfRetHandle( ctx, HPDF_ICC_LoadIccFromMem( pdf, mmgr, iccdata, xref, numcomponent ) );
}

// HPDF_Image_AddSMask( image, smask ) --> nStatus
X_FUNC( HPDF_IMAGE_ADDSMASK )
{
   HPDF_Image image = ( HPDF_Image )x_parptr( ctx, 1 );
   HPDF_Image smask = ( HPDF_Image )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Image_AddSMask( image, smask ) );
}

// HPDF_Image_GetBitsPerComponent( image ) --> nValue
X_FUNC( HPDF_IMAGE_GETBITSPERCOMPONENT )
{
   x_retnint( ctx, ( int64_t )HPDF_Image_GetBitsPerComponent( ( HPDF_Image )x_parptr( ctx, 1 ) ) );
}

// HPDF_Image_GetColorSpace( image ) --> cValue
X_FUNC( HPDF_IMAGE_GETCOLORSPACE )
{
   x_hpdfRetString( ctx, HPDF_Image_GetColorSpace( ( HPDF_Image )x_parptr( ctx, 1 ) ) );
}

// HPDF_Image_GetHeight( image ) --> nValue
X_FUNC( HPDF_IMAGE_GETHEIGHT )
{
   x_retnint( ctx, ( int64_t )HPDF_Image_GetHeight( ( HPDF_Image )x_parptr( ctx, 1 ) ) );
}

// HPDF_Image_GetSize( image ) --> aPoint
X_FUNC( HPDF_IMAGE_GETSIZE )
{
   x_hpdfRetPoint( ctx, HPDF_Image_GetSize( ( HPDF_Image )x_parptr( ctx, 1 ) ) );
}

// Size of an image as { nWidth, nHeight }
// HPDF_Image_GetSize2( pImage, @aSize ) --> nStatus
X_FUNC( HPDF_IMAGE_GETSIZE2 )
{
   HPDF_Point size = { 0, 0 };
   HPDF_STATUS status = HPDF_Image_GetSize2( ( HPDF_Image )x_parptr( ctx, 1 ), &size );

   x_hpdfStorePoint( ctx, 2, size );
   x_retnint( ctx, ( int64_t )status );
}

// HPDF_Image_GetWidth( image ) --> nValue
X_FUNC( HPDF_IMAGE_GETWIDTH )
{
   x_retnint( ctx, ( int64_t )HPDF_Image_GetWidth( ( HPDF_Image )x_parptr( ctx, 1 ) ) );
}

// HPDF_Image_LoadRaw1BitImageFromMem( pdf, buf, width, height, line_width, black_is1, top_is_first ) --> pImage
X_FUNC( HPDF_IMAGE_LOADRAW1BITIMAGEFROMMEM )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const HPDF_BYTE *buf = ( const HPDF_BYTE * )x_hpdfString( ctx, 2 );
   HPDF_UINT width = ( HPDF_UINT )x_parnint( ctx, 3 );
   HPDF_UINT height = ( HPDF_UINT )x_parnint( ctx, 4 );
   HPDF_UINT lineWidth = ( HPDF_UINT )x_parnint( ctx, 5 );
   HPDF_BOOL blackIs1 = x_hpdfBool( ctx, 6 );
   HPDF_BOOL topIsFirst = x_hpdfBool( ctx, 7 );
   HPDF_Image image;

   image = HPDF_Image_LoadRaw1BitImageFromMem( pdf, buf, width, height, lineWidth, blackIs1, topIsFirst );
   x_hpdfRetHandle( ctx, image );
}

// HPDF_Image_SetColorMask( image, rmin, rmax, gmin, gmax, bmin, bmax ) --> nStatus
X_FUNC( HPDF_IMAGE_SETCOLORMASK )
{
   HPDF_Image image = ( HPDF_Image )x_parptr( ctx, 1 );
   HPDF_UINT rmin = ( HPDF_UINT )x_parnint( ctx, 2 );
   HPDF_UINT rmax = ( HPDF_UINT )x_parnint( ctx, 3 );
   HPDF_UINT gmin = ( HPDF_UINT )x_parnint( ctx, 4 );
   HPDF_UINT gmax = ( HPDF_UINT )x_parnint( ctx, 5 );
   HPDF_UINT bmin = ( HPDF_UINT )x_parnint( ctx, 6 );
   HPDF_UINT bmax = ( HPDF_UINT )x_parnint( ctx, 7 );

   x_retnint( ctx, ( int64_t )HPDF_Image_SetColorMask( image, rmin, rmax, gmin, gmax, bmin, bmax ) );
}

// HPDF_Image_SetMaskImage( image, mask_image ) --> nStatus
X_FUNC( HPDF_IMAGE_SETMASKIMAGE )
{
   HPDF_Image image = ( HPDF_Image )x_parptr( ctx, 1 );
   HPDF_Image maskImage = ( HPDF_Image )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Image_SetMaskImage( image, maskImage ) );
}

// HPDF_InsertPage( pdf, page ) --> pPage
X_FUNC( HPDF_INSERTPAGE )
{
   x_hpdfRetHandle( ctx, HPDF_InsertPage( ( HPDF_Doc )x_parptr( ctx, 1 ), ( HPDF_Page )x_parptr( ctx, 2 ) ) );
}

// HPDF_LineAnnot_SetCaption( annot, showCaption, position, horzOffset, vertOffset ) --> nStatus
X_FUNC( HPDF_LINEANNOT_SETCAPTION )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_BOOL showCaption = x_hpdfBool( ctx, 2 );
   HPDF_LineAnnotCapPosition position = ( HPDF_LineAnnotCapPosition )x_parnint( ctx, 3 );
   HPDF_INT horzOffset = ( HPDF_INT )x_parnint( ctx, 4 );
   HPDF_INT vertOffset = ( HPDF_INT )x_parnint( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_LineAnnot_SetCaption( annot, showCaption, position, horzOffset, vertOffset ) );
}

// HPDF_LineAnnot_SetLeader( annot, leaderLen, leaderExtLen, leaderOffsetLen ) --> nStatus
X_FUNC( HPDF_LINEANNOT_SETLEADER )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_INT leaderLen = ( HPDF_INT )x_parnint( ctx, 2 );
   HPDF_INT leaderExtLen = ( HPDF_INT )x_parnint( ctx, 3 );
   HPDF_INT leaderOffsetLen = ( HPDF_INT )x_parnint( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_LineAnnot_SetLeader( annot, leaderLen, leaderExtLen, leaderOffsetLen ) );
}

// HPDF_LineAnnot_SetPosition( annot, startPoint, startStyle, endPoint, endStyle ) --> nStatus
X_FUNC( HPDF_LINEANNOT_SETPOSITION )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_Point startPoint = x_hpdfPoint( x_paritem( ctx, 2 ) );
   HPDF_LineAnnotEndingStyle startStyle = ( HPDF_LineAnnotEndingStyle )x_parnint( ctx, 3 );
   HPDF_Point endPoint = x_hpdfPoint( x_paritem( ctx, 4 ) );
   HPDF_LineAnnotEndingStyle endStyle = ( HPDF_LineAnnotEndingStyle )x_parnint( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_LineAnnot_SetPosition( annot, startPoint, startStyle, endPoint, endStyle ) );
}

// HPDF_LinkAnnot_SetBorderStyle( annot, width, dash_on, dash_off ) --> nStatus
X_FUNC( HPDF_LINKANNOT_SETBORDERSTYLE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_REAL width = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_UINT16 dashOn = ( HPDF_UINT16 )x_parnint( ctx, 3 );
   HPDF_UINT16 dashOff = ( HPDF_UINT16 )x_parnint( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_LinkAnnot_SetBorderStyle( annot, width, dashOn, dashOff ) );
}

// HPDF_LinkAnnot_SetHighlightMode( annot, mode ) --> nStatus
X_FUNC( HPDF_LINKANNOT_SETHIGHLIGHTMODE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_AnnotHighlightMode mode = ( HPDF_AnnotHighlightMode )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_LinkAnnot_SetHighlightMode( annot, mode ) );
}

// HPDF_LinkAnnot_SetJavaScript( annot, javascript ) --> nStatus
X_FUNC( HPDF_LINKANNOT_SETJAVASCRIPT )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_JavaScript javascript = ( HPDF_JavaScript )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_LinkAnnot_SetJavaScript( annot, javascript ) );
}

// HPDF_LoadIccProfileFromFile( pdf, icc_file_name, numcomponent ) --> pOutputIntent
X_FUNC( HPDF_LOADICCPROFILEFROMFILE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *iccFileName = x_hpdfString( ctx, 2 );
   int numcomponent = ( int )x_parnint( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_LoadIccProfileFromFile( pdf, iccFileName, numcomponent ) );
}

// HPDF_LoadJpegImageFromFile( pdf, filename ) --> pImage
X_FUNC( HPDF_LOADJPEGIMAGEFROMFILE )
{
   x_hpdfRetHandle( ctx, HPDF_LoadJpegImageFromFile( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_LoadJpegImageFromMem( pdf, buffer, size ) --> pImage
X_FUNC( HPDF_LOADJPEGIMAGEFROMMEM )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const HPDF_BYTE *buffer = ( const HPDF_BYTE * )x_hpdfString( ctx, 2 );
   HPDF_UINT size = ( HPDF_UINT )x_parnint( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_LoadJpegImageFromMem( pdf, buffer, size ) );
}

// HPDF_LoadPngImageFromFile( pdf, filename ) --> pImage
X_FUNC( HPDF_LOADPNGIMAGEFROMFILE )
{
   x_hpdfRetHandle( ctx, HPDF_LoadPngImageFromFile( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_LoadPngImageFromFile2( pdf, filename ) --> pImage
X_FUNC( HPDF_LOADPNGIMAGEFROMFILE2 )
{
   x_hpdfRetHandle( ctx, HPDF_LoadPngImageFromFile2( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_LoadPngImageFromMem( pdf, buffer, size ) --> pImage
X_FUNC( HPDF_LOADPNGIMAGEFROMMEM )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const HPDF_BYTE *buffer = ( const HPDF_BYTE * )x_hpdfString( ctx, 2 );
   HPDF_UINT size = ( HPDF_UINT )x_parnint( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_LoadPngImageFromMem( pdf, buffer, size ) );
}

// HPDF_LoadRawImageFromFile( pdf, filename, width, height, color_space ) --> pImage
X_FUNC( HPDF_LOADRAWIMAGEFROMFILE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *filename = x_hpdfString( ctx, 2 );
   HPDF_UINT width = ( HPDF_UINT )x_parnint( ctx, 3 );
   HPDF_UINT height = ( HPDF_UINT )x_parnint( ctx, 4 );
   HPDF_ColorSpace colorSpace = ( HPDF_ColorSpace )x_parnint( ctx, 5 );

   x_hpdfRetHandle( ctx, HPDF_LoadRawImageFromFile( pdf, filename, width, height, colorSpace ) );
}

// HPDF_LoadRawImageFromMem( pdf, buf, width, height, color_space, bits_per_component ) --> pImage
X_FUNC( HPDF_LOADRAWIMAGEFROMMEM )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const HPDF_BYTE *buf = ( const HPDF_BYTE * )x_hpdfString( ctx, 2 );
   HPDF_UINT width = ( HPDF_UINT )x_parnint( ctx, 3 );
   HPDF_UINT height = ( HPDF_UINT )x_parnint( ctx, 4 );
   HPDF_ColorSpace colorSpace = ( HPDF_ColorSpace )x_parnint( ctx, 5 );
   HPDF_UINT bitsPerComponent = ( HPDF_UINT )x_parnint( ctx, 6 );

   x_hpdfRetHandle( ctx, HPDF_LoadRawImageFromMem( pdf, buf, width, height, colorSpace, bitsPerComponent ) );
}

// HPDF_LoadTTFontFromFile( pdf, file_name, embedding ) --> cValue
X_FUNC( HPDF_LOADTTFONTFROMFILE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *fileName = x_hpdfString( ctx, 2 );
   HPDF_BOOL embedding = x_hpdfBool( ctx, 3 );

   x_hpdfRetString( ctx, HPDF_LoadTTFontFromFile( pdf, fileName, embedding ) );
}

// HPDF_LoadTTFontFromFile2( pdf, file_name, index, embedding ) --> cValue
X_FUNC( HPDF_LOADTTFONTFROMFILE2 )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *fileName = x_hpdfString( ctx, 2 );
   HPDF_UINT index = ( HPDF_UINT )x_parnint( ctx, 3 );
   HPDF_BOOL embedding = x_hpdfBool( ctx, 4 );

   x_hpdfRetString( ctx, HPDF_LoadTTFontFromFile2( pdf, fileName, index, embedding ) );
}

// HPDF_LoadTTFontFromMemory( pdf, buffer, size, embedding ) --> cValue
X_FUNC( HPDF_LOADTTFONTFROMMEMORY )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const HPDF_BYTE *buffer = ( const HPDF_BYTE * )x_hpdfString( ctx, 2 );
   HPDF_UINT size = ( HPDF_UINT )x_parnint( ctx, 3 );
   HPDF_BOOL embedding = x_hpdfBool( ctx, 4 );

   x_hpdfRetString( ctx, HPDF_LoadTTFontFromMemory( pdf, buffer, size, embedding ) );
}

// HPDF_LoadType1FontFromFile( pdf, afm_file_name, data_file_name ) --> cValue
X_FUNC( HPDF_LOADTYPE1FONTFROMFILE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *afmFileName = x_hpdfString( ctx, 2 );
   const char *dataFileName = x_hpdfString( ctx, 3 );

   x_hpdfRetString( ctx, HPDF_LoadType1FontFromFile( pdf, afmFileName, dataFileName ) );
}

// HPDF_LoadU3DFromFile( pdf, filename ) --> pImage
X_FUNC( HPDF_LOADU3DFROMFILE )
{
   x_hpdfRetHandle( ctx, HPDF_LoadU3DFromFile( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_LoadU3DFromMem( pdf, buffer, size ) --> pImage
X_FUNC( HPDF_LOADU3DFROMMEM )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const HPDF_BYTE *buffer = ( const HPDF_BYTE * )x_hpdfString( ctx, 2 );
   HPDF_UINT size = ( HPDF_UINT )x_parnint( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_LoadU3DFromMem( pdf, buffer, size ) );
}

// HPDF_MarkupAnnot_SetCloudEffect( annot, cloudIntensity ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETCLOUDEFFECT )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_INT cloudIntensity = ( HPDF_INT )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetCloudEffect( annot, cloudIntensity ) );
}

// HPDF_MarkupAnnot_SetCreationDate( annot, value ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETCREATIONDATE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_Date value = x_hpdfDate( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetCreationDate( annot, value ) );
}

// HPDF_MarkupAnnot_SetIntent( annot, intent ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETINTENT )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_AnnotIntent intent = ( HPDF_AnnotIntent )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetIntent( annot, intent ) );
}

// HPDF_MarkupAnnot_SetInteriorCMYKColor( annot, color ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETINTERIORCMYKCOLOR )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_CMYKColor color = x_hpdfCMYKColor( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetInteriorCMYKColor( annot, color ) );
}

// HPDF_MarkupAnnot_SetInteriorGrayColor( annot, color ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETINTERIORGRAYCOLOR )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_REAL color = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetInteriorGrayColor( annot, color ) );
}

// HPDF_MarkupAnnot_SetInteriorRGBColor( annot, color ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETINTERIORRGBCOLOR )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_RGBColor color = x_hpdfRGBColor( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetInteriorRGBColor( annot, color ) );
}

// HPDF_MarkupAnnot_SetInteriorTransparent( annot ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETINTERIORTRANSPARENT )
{
   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetInteriorTransparent( ( HPDF_Annotation )x_parptr( ctx, 1 ) ) );
}

// HPDF_MarkupAnnot_SetPopup( annot, popup ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETPOPUP )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_Annotation popup = ( HPDF_Annotation )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetPopup( annot, popup ) );
}

// HPDF_MarkupAnnot_SetRectDiff( annot, rect ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETRECTDIFF )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetRectDiff( annot, rect ) );
}

// HPDF_MarkupAnnot_SetSubject( annot, name ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETSUBJECT )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   const char *name = x_hpdfString( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetSubject( annot, name ) );
}

// HPDF_MarkupAnnot_SetTitle( annot, name ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETTITLE )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   const char *name = x_hpdfString( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetTitle( annot, name ) );
}

// HPDF_MarkupAnnot_SetTransparency( annot, value ) --> nStatus
X_FUNC( HPDF_MARKUPANNOT_SETTRANSPARENCY )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_MarkupAnnot_SetTransparency( annot, value ) );
}

// Create a document; errors are read with HPDF_GetError() and HPDF_GetErrorDetail(), a C error handler
// cannot be given
// HPDF_New() --> pDoc | NIL
X_FUNC( HPDF_NEW )
{
   x_hpdfRetHandle( ctx, HPDF_New( NULL, NULL ) );
}

// HPDF_NewDoc( pdf ) --> nStatus
X_FUNC( HPDF_NEWDOC )
{
   x_retnint( ctx, ( int64_t )HPDF_NewDoc( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// Create a document with a memory pool of nMemPoolBufSize bytes; the handlers must be NIL
// HPDF_NewEx( NIL, NIL, NIL, nMemPoolBufSize, NIL ) --> pDoc | NIL
X_FUNC( HPDF_NEWEX )
{
   x_hpdfRetHandle( ctx, HPDF_NewEx( NULL, NULL, NULL, ( HPDF_UINT )x_parnint( ctx, 4 ), NULL ) );
}

// HPDF_Outline_SetDestination( outline, dst ) --> nStatus
X_FUNC( HPDF_OUTLINE_SETDESTINATION )
{
   HPDF_Outline outline = ( HPDF_Outline )x_parptr( ctx, 1 );
   HPDF_Destination dst = ( HPDF_Destination )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Outline_SetDestination( outline, dst ) );
}

// HPDF_Outline_SetOpened( outline, opened ) --> nStatus
X_FUNC( HPDF_OUTLINE_SETOPENED )
{
   x_retnint( ctx, ( int64_t )HPDF_Outline_SetOpened( ( HPDF_Outline )x_parptr( ctx, 1 ), x_hpdfBool( ctx, 2 ) ) );
}

// HPDF_Page_Arc( page, x, y, ray, ang1, ang2 ) --> nStatus
X_FUNC( HPDF_PAGE_ARC )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL ray = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL ang1 = ( HPDF_REAL )x_parnd( ctx, 5 );
   HPDF_REAL ang2 = ( HPDF_REAL )x_parnd( ctx, 6 );

   x_retnint( ctx, ( int64_t )HPDF_Page_Arc( page, x, y, ray, ang1, ang2 ) );
}

// HPDF_Page_BeginText( page ) --> nStatus
X_FUNC( HPDF_PAGE_BEGINTEXT )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_BeginText( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_Circle( page, x, y, ray ) --> nStatus
X_FUNC( HPDF_PAGE_CIRCLE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL ray = ( HPDF_REAL )x_parnd( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_Page_Circle( page, x, y, ray ) );
}

// HPDF_Page_Clip( page ) --> nStatus
X_FUNC( HPDF_PAGE_CLIP )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_Clip( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_ClosePath( page ) --> nStatus
X_FUNC( HPDF_PAGE_CLOSEPATH )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_ClosePath( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_ClosePathEofillStroke( page ) --> nStatus
X_FUNC( HPDF_PAGE_CLOSEPATHEOFILLSTROKE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_ClosePathEofillStroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_ClosePathFillStroke( page ) --> nStatus
X_FUNC( HPDF_PAGE_CLOSEPATHFILLSTROKE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_ClosePathFillStroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_ClosePathStroke( page ) --> nStatus
X_FUNC( HPDF_PAGE_CLOSEPATHSTROKE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_ClosePathStroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_Concat( page, a, b, c, d, x, y ) --> nStatus
X_FUNC( HPDF_PAGE_CONCAT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL a = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL b = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL c = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL d = ( HPDF_REAL )x_parnd( ctx, 5 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 6 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 7 );

   x_retnint( ctx, ( int64_t )HPDF_Page_Concat( page, a, b, c, d, x, y ) );
}

// HPDF_Page_Create3DAnnot( page, rect, tb, np, u3d, ap ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATE3DANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   HPDF_BOOL tb = x_hpdfBool( ctx, 3 );
   HPDF_BOOL np = x_hpdfBool( ctx, 4 );
   HPDF_U3D u3d = ( HPDF_U3D )x_parptr( ctx, 5 );
   HPDF_Image ap = ( HPDF_Image )x_parptr( ctx, 6 );

   x_hpdfRetHandle( ctx, HPDF_Page_Create3DAnnot( page, rect, tb, np, u3d, ap ) );
}

// HPDF_Page_Create3DAnnotExData( page ) --> pExData
X_FUNC( HPDF_PAGE_CREATE3DANNOTEXDATA )
{
   x_hpdfRetHandle( ctx, HPDF_Page_Create3DAnnotExData( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_Create3DC3DMeasure( page, firstanchorpoint, textanchorpoint ) --> p3DMeasure
X_FUNC( HPDF_PAGE_CREATE3DC3DMEASURE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Point3D firstanchorpoint = x_hpdfPoint3D( x_paritem( ctx, 2 ) );
   HPDF_Point3D textanchorpoint = x_hpdfPoint3D( x_paritem( ctx, 3 ) );

   x_hpdfRetHandle( ctx, HPDF_Page_Create3DC3DMeasure( page, firstanchorpoint, textanchorpoint ) );
}

// HPDF_Page_Create3DView( page, u3d, annot3d, name ) --> pDict
X_FUNC( HPDF_PAGE_CREATE3DVIEW )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_U3D u3d = ( HPDF_U3D )x_parptr( ctx, 2 );
   HPDF_Annotation annot3d = ( HPDF_Annotation )x_parptr( ctx, 3 );
   const char *name = x_hpdfString( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_Create3DView( page, u3d, annot3d, name ) );
}

// HPDF_Page_CreateCircleAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATECIRCLEANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateCircleAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateDestination( page ) --> pDestination
X_FUNC( HPDF_PAGE_CREATEDESTINATION )
{
   x_hpdfRetHandle( ctx, HPDF_Page_CreateDestination( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_CreateFreeTextAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATEFREETEXTANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateFreeTextAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateHighlightAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATEHIGHLIGHTANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateHighlightAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateLineAnnot( page, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATELINEANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   const char *text = x_hpdfString( ctx, 2 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateLineAnnot( page, text, encoder ) );
}

// HPDF_Page_CreateLinkAnnot( page, rect, dst ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATELINKANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   HPDF_Destination dst = ( HPDF_Destination )x_parptr( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateLinkAnnot( page, rect, dst ) );
}

// HPDF_Page_CreatePD33DMeasure( page, annotationPlaneNormal, firstAnchorPoint, secondAnchorPoint,
//                               leaderLinesDirection, measurementValuePoint, textYDirection, value,
//                               unitsString ) --> p3DMeasure
X_FUNC( HPDF_PAGE_CREATEPD33DMEASURE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Point3D normal = x_hpdfPoint3D( x_paritem( ctx, 2 ) );
   HPDF_Point3D first = x_hpdfPoint3D( x_paritem( ctx, 3 ) );
   HPDF_Point3D second = x_hpdfPoint3D( x_paritem( ctx, 4 ) );
   HPDF_Point3D leader = x_hpdfPoint3D( x_paritem( ctx, 5 ) );
   HPDF_Point3D valuePoint = x_hpdfPoint3D( x_paritem( ctx, 6 ) );
   HPDF_Point3D textY = x_hpdfPoint3D( x_paritem( ctx, 7 ) );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 8 );
   const char *units = x_hpdfString( ctx, 9 );
   HPDF_3DMeasure measure;

   measure = HPDF_Page_CreatePD33DMeasure( page, normal, first, second, leader, valuePoint, textY, value, units );
   x_hpdfRetHandle( ctx, measure );
}

// HPDF_Page_CreatePopupAnnot( page, rect, parent ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATEPOPUPANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   HPDF_Annotation parent = ( HPDF_Annotation )x_parptr( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreatePopupAnnot( page, rect, parent ) );
}

// HPDF_Page_CreateProjectionAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATEPROJECTIONANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateProjectionAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateSquareAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATESQUAREANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateSquareAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateSquigglyAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATESQUIGGLYANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateSquigglyAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateStampAnnot( page, rect, name, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATESTAMPANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   HPDF_StampAnnotName name = ( HPDF_StampAnnotName )x_parnint( ctx, 3 );
   const char *text = x_hpdfString( ctx, 4 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 5 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateStampAnnot( page, rect, name, text, encoder ) );
}

// HPDF_Page_CreateStrikeOutAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATESTRIKEOUTANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateStrikeOutAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateTextAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATETEXTANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateTextAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateUnderlineAnnot( page, rect, text, encoder ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATEUNDERLINEANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *text = x_hpdfString( ctx, 3 );
   HPDF_Encoder encoder = ( HPDF_Encoder )x_parptr( ctx, 4 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateUnderlineAnnot( page, rect, text, encoder ) );
}

// HPDF_Page_CreateURILinkAnnot( page, rect, uri ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATEURILINKANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );
   const char *uri = x_hpdfString( ctx, 3 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateURILinkAnnot( page, rect, uri ) );
}

// HPDF_Page_CreateWidgetAnnot( page, rect ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATEWIDGETANNOT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 2 ) );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateWidgetAnnot( page, rect ) );
}

// HPDF_Page_CreateWidgetAnnot_WhiteOnlyWhilePrint( pdf, page, rect ) --> pAnnotation
X_FUNC( HPDF_PAGE_CREATEWIDGETANNOT_WHITEONLYWHILEPRINT )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 2 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 3 ) );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateWidgetAnnot_WhiteOnlyWhilePrint( pdf, page, rect ) );
}

// HPDF_Page_CreateXObjectAsWhiteRect( pdf, page, rect ) --> pXObject
X_FUNC( HPDF_PAGE_CREATEXOBJECTASWHITERECT )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 2 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 3 ) );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateXObjectAsWhiteRect( pdf, page, rect ) );
}

// HPDF_Page_CreateXObjectFromImage( pdf, page, rect, image, zoom ) --> pXObject
X_FUNC( HPDF_PAGE_CREATEXOBJECTFROMIMAGE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 2 );
   HPDF_Rect rect = x_hpdfRect( x_paritem( ctx, 3 ) );
   HPDF_Image image = ( HPDF_Image )x_parptr( ctx, 4 );
   HPDF_BOOL zoom = x_hpdfBool( ctx, 5 );

   x_hpdfRetHandle( ctx, HPDF_Page_CreateXObjectFromImage( pdf, page, rect, image, zoom ) );
}

// HPDF_Page_CurveTo( page, x1, y1, x2, y2, x3, y3 ) --> nStatus
X_FUNC( HPDF_PAGE_CURVETO )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x1 = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y1 = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL x2 = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL y2 = ( HPDF_REAL )x_parnd( ctx, 5 );
   HPDF_REAL x3 = ( HPDF_REAL )x_parnd( ctx, 6 );
   HPDF_REAL y3 = ( HPDF_REAL )x_parnd( ctx, 7 );

   x_retnint( ctx, ( int64_t )HPDF_Page_CurveTo( page, x1, y1, x2, y2, x3, y3 ) );
}

// HPDF_Page_CurveTo2( page, x2, y2, x3, y3 ) --> nStatus
X_FUNC( HPDF_PAGE_CURVETO2 )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x2 = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y2 = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL x3 = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL y3 = ( HPDF_REAL )x_parnd( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_Page_CurveTo2( page, x2, y2, x3, y3 ) );
}

// HPDF_Page_CurveTo3( page, x1, y1, x3, y3 ) --> nStatus
X_FUNC( HPDF_PAGE_CURVETO3 )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x1 = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y1 = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL x3 = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL y3 = ( HPDF_REAL )x_parnd( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_Page_CurveTo3( page, x1, y1, x3, y3 ) );
}

// HPDF_Page_DrawImage( page, image, x, y, width, height ) --> nStatus
X_FUNC( HPDF_PAGE_DRAWIMAGE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Image image = ( HPDF_Image )x_parptr( ctx, 2 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL width = ( HPDF_REAL )x_parnd( ctx, 5 );
   HPDF_REAL height = ( HPDF_REAL )x_parnd( ctx, 6 );

   x_retnint( ctx, ( int64_t )HPDF_Page_DrawImage( page, image, x, y, width, height ) );
}

// HPDF_Page_Ellipse( page, x, y, xray, yray ) --> nStatus
X_FUNC( HPDF_PAGE_ELLIPSE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL xray = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL yray = ( HPDF_REAL )x_parnd( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_Page_Ellipse( page, x, y, xray, yray ) );
}

// HPDF_Page_EndPath( page ) --> nStatus
X_FUNC( HPDF_PAGE_ENDPATH )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_EndPath( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_EndText( page ) --> nStatus
X_FUNC( HPDF_PAGE_ENDTEXT )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_EndText( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_Eoclip( page ) --> nStatus
X_FUNC( HPDF_PAGE_EOCLIP )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_Eoclip( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_Eofill( page ) --> nStatus
X_FUNC( HPDF_PAGE_EOFILL )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_Eofill( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_EofillStroke( page ) --> nStatus
X_FUNC( HPDF_PAGE_EOFILLSTROKE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_EofillStroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_ExecuteXObject( page, obj ) --> nStatus
X_FUNC( HPDF_PAGE_EXECUTEXOBJECT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_XObject obj = ( HPDF_XObject )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_ExecuteXObject( page, obj ) );
}

// HPDF_Page_Fill( page ) --> nStatus
X_FUNC( HPDF_PAGE_FILL )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_Fill( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_FillStroke( page ) --> nStatus
X_FUNC( HPDF_PAGE_FILLSTROKE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_FillStroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetCharSpace( page ) --> nValue
X_FUNC( HPDF_PAGE_GETCHARSPACE )
{
   x_retnd( ctx, ( double )HPDF_Page_GetCharSpace( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetCMYKFill( page ) --> aCMYKColor
X_FUNC( HPDF_PAGE_GETCMYKFILL )
{
   x_hpdfRetCMYKColor( ctx, HPDF_Page_GetCMYKFill( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetCMYKStroke( page ) --> aCMYKColor
X_FUNC( HPDF_PAGE_GETCMYKSTROKE )
{
   x_hpdfRetCMYKColor( ctx, HPDF_Page_GetCMYKStroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetCurrentFont( page ) --> pFont
X_FUNC( HPDF_PAGE_GETCURRENTFONT )
{
   x_hpdfRetHandle( ctx, HPDF_Page_GetCurrentFont( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetCurrentFontSize( page ) --> nValue
X_FUNC( HPDF_PAGE_GETCURRENTFONTSIZE )
{
   x_retnd( ctx, ( double )HPDF_Page_GetCurrentFontSize( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetCurrentPos( page ) --> aPoint
X_FUNC( HPDF_PAGE_GETCURRENTPOS )
{
   x_hpdfRetPoint( ctx, HPDF_Page_GetCurrentPos( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// Current position of the path as { nX, nY }
// HPDF_Page_GetCurrentPos2( pPage, @aPos ) --> nStatus
X_FUNC( HPDF_PAGE_GETCURRENTPOS2 )
{
   HPDF_Point pos = { 0, 0 };
   HPDF_STATUS status = HPDF_Page_GetCurrentPos2( ( HPDF_Page )x_parptr( ctx, 1 ), &pos );

   x_hpdfStorePoint( ctx, 2, pos );
   x_retnint( ctx, ( int64_t )status );
}

// HPDF_Page_GetCurrentTextPos( page ) --> aPoint
X_FUNC( HPDF_PAGE_GETCURRENTTEXTPOS )
{
   x_hpdfRetPoint( ctx, HPDF_Page_GetCurrentTextPos( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// Current position of the text as { nX, nY }
// HPDF_Page_GetCurrentTextPos2( pPage, @aPos ) --> nStatus
X_FUNC( HPDF_PAGE_GETCURRENTTEXTPOS2 )
{
   HPDF_Point pos = { 0, 0 };
   HPDF_STATUS status = HPDF_Page_GetCurrentTextPos2( ( HPDF_Page )x_parptr( ctx, 1 ), &pos );

   x_hpdfStorePoint( ctx, 2, pos );
   x_retnint( ctx, ( int64_t )status );
}

// HPDF_Page_GetDash( page ) --> aDashMode
X_FUNC( HPDF_PAGE_GETDASH )
{
   x_hpdfRetDashMode( ctx, HPDF_Page_GetDash( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetFillingColorSpace( page ) --> nValue
X_FUNC( HPDF_PAGE_GETFILLINGCOLORSPACE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GetFillingColorSpace( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetFlat( page ) --> nValue
X_FUNC( HPDF_PAGE_GETFLAT )
{
   x_retnd( ctx, ( double )HPDF_Page_GetFlat( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetGMode( page ) --> nValue
X_FUNC( HPDF_PAGE_GETGMODE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GetGMode( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetGrayFill( page ) --> nValue
X_FUNC( HPDF_PAGE_GETGRAYFILL )
{
   x_retnd( ctx, ( double )HPDF_Page_GetGrayFill( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetGrayStroke( page ) --> nValue
X_FUNC( HPDF_PAGE_GETGRAYSTROKE )
{
   x_retnd( ctx, ( double )HPDF_Page_GetGrayStroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetGStateDepth( page ) --> nValue
X_FUNC( HPDF_PAGE_GETGSTATEDEPTH )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GetGStateDepth( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetHeight( page ) --> nValue
X_FUNC( HPDF_PAGE_GETHEIGHT )
{
   x_retnd( ctx, ( double )HPDF_Page_GetHeight( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetHorizontalScalling( page ) --> nValue
X_FUNC( HPDF_PAGE_GETHORIZONTALSCALLING )
{
   x_retnd( ctx, ( double )HPDF_Page_GetHorizontalScalling( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetLineCap( page ) --> nValue
X_FUNC( HPDF_PAGE_GETLINECAP )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GetLineCap( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetLineJoin( page ) --> nValue
X_FUNC( HPDF_PAGE_GETLINEJOIN )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GetLineJoin( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetLineWidth( page ) --> nValue
X_FUNC( HPDF_PAGE_GETLINEWIDTH )
{
   x_retnd( ctx, ( double )HPDF_Page_GetLineWidth( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetMiterLimit( page ) --> nValue
X_FUNC( HPDF_PAGE_GETMITERLIMIT )
{
   x_retnd( ctx, ( double )HPDF_Page_GetMiterLimit( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetRGBFill( page ) --> aRGBColor
X_FUNC( HPDF_PAGE_GETRGBFILL )
{
   x_hpdfRetRGBColor( ctx, HPDF_Page_GetRGBFill( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetRGBStroke( page ) --> aRGBColor
X_FUNC( HPDF_PAGE_GETRGBSTROKE )
{
   x_hpdfRetRGBColor( ctx, HPDF_Page_GetRGBStroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetStrokingColorSpace( page ) --> nValue
X_FUNC( HPDF_PAGE_GETSTROKINGCOLORSPACE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GetStrokingColorSpace( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetTextLeading( page ) --> nValue
X_FUNC( HPDF_PAGE_GETTEXTLEADING )
{
   x_retnd( ctx, ( double )HPDF_Page_GetTextLeading( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetTextMatrix( page ) --> aTransMatrix
X_FUNC( HPDF_PAGE_GETTEXTMATRIX )
{
   x_hpdfRetTransMatrix( ctx, HPDF_Page_GetTextMatrix( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetTextRaise( page ) --> nValue
X_FUNC( HPDF_PAGE_GETTEXTRAISE )
{
   x_retnd( ctx, ( double )HPDF_Page_GetTextRaise( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetTextRenderingMode( page ) --> nValue
X_FUNC( HPDF_PAGE_GETTEXTRENDERINGMODE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GetTextRenderingMode( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetTextRise( page ) --> nValue
X_FUNC( HPDF_PAGE_GETTEXTRISE )
{
   x_retnd( ctx, ( double )HPDF_Page_GetTextRise( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetTransMatrix( page ) --> aTransMatrix
X_FUNC( HPDF_PAGE_GETTRANSMATRIX )
{
   x_hpdfRetTransMatrix( ctx, HPDF_Page_GetTransMatrix( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetWidth( page ) --> nValue
X_FUNC( HPDF_PAGE_GETWIDTH )
{
   x_retnd( ctx, ( double )HPDF_Page_GetWidth( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GetWordSpace( page ) --> nValue
X_FUNC( HPDF_PAGE_GETWORDSPACE )
{
   x_retnd( ctx, ( double )HPDF_Page_GetWordSpace( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GRestore( page ) --> nStatus
X_FUNC( HPDF_PAGE_GRESTORE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GRestore( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_GSave( page ) --> nStatus
X_FUNC( HPDF_PAGE_GSAVE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_GSave( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_Insert_Shared_Content_Stream( page, shared_stream ) --> nStatus
X_FUNC( HPDF_PAGE_INSERT_SHARED_CONTENT_STREAM )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Dict sharedStream = ( HPDF_Dict )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_Insert_Shared_Content_Stream( page, sharedStream ) );
}

// HPDF_Page_LineTo( page, x, y ) --> nStatus
X_FUNC( HPDF_PAGE_LINETO )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_Page_LineTo( page, x, y ) );
}

// Number of bytes of the text that fit into nWidth with the font of the page; nRealWidth gets their width
// HPDF_Page_MeasureText( pPage, cText, nWidth, lWordWrap, @nRealWidth ) --> nBytes
X_FUNC( HPDF_PAGE_MEASURETEXT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   const char *text = x_hpdfString( ctx, 2 );
   HPDF_REAL width = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_BOOL wordWrap = x_hpdfBool( ctx, 4 );
   HPDF_REAL realWidth = 0;
   HPDF_UINT count;

   count = HPDF_Page_MeasureText( page, text, width, wordWrap, &realWidth );
   x_stornd( ctx, ( double )realWidth, 5 );
   x_retnint( ctx, ( int64_t )count );
}

// HPDF_Page_MoveTextPos( page, x, y ) --> nStatus
X_FUNC( HPDF_PAGE_MOVETEXTPOS )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_Page_MoveTextPos( page, x, y ) );
}

// HPDF_Page_MoveTextPos2( page, x, y ) --> nStatus
X_FUNC( HPDF_PAGE_MOVETEXTPOS2 )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_Page_MoveTextPos2( page, x, y ) );
}

// HPDF_Page_MoveTo( page, x, y ) --> nStatus
X_FUNC( HPDF_PAGE_MOVETO )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_Page_MoveTo( page, x, y ) );
}

// HPDF_Page_MoveToNextLine( page ) --> nStatus
X_FUNC( HPDF_PAGE_MOVETONEXTLINE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_MoveToNextLine( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// Start a new content stream of the page; pStream gets it
// HPDF_Page_New_Content_Stream( pPage, @pStream ) --> nStatus
X_FUNC( HPDF_PAGE_NEW_CONTENT_STREAM )
{
   HPDF_Dict stream = NULL;
   HPDF_STATUS status = HPDF_Page_New_Content_Stream( ( HPDF_Page )x_parptr( ctx, 1 ), &stream );

   x_storptr( ctx, stream, 2 );
   x_retnint( ctx, ( int64_t )status );
}

// HPDF_Page_Rectangle( page, x, y, width, height ) --> nStatus
X_FUNC( HPDF_PAGE_RECTANGLE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL width = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL height = ( HPDF_REAL )x_parnd( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_Page_Rectangle( page, x, y, width, height ) );
}

// HPDF_Page_SetBoundary( page, boundary, left, bottom, right, top ) --> nStatus
X_FUNC( HPDF_PAGE_SETBOUNDARY )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_PageBoundary boundary = ( HPDF_PageBoundary )x_parnint( ctx, 2 );
   HPDF_REAL left = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL bottom = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL right = ( HPDF_REAL )x_parnd( ctx, 5 );
   HPDF_REAL top = ( HPDF_REAL )x_parnd( ctx, 6 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetBoundary( page, boundary, left, bottom, right, top ) );
}

// HPDF_Page_SetCharSpace( page, value ) --> nStatus
X_FUNC( HPDF_PAGE_SETCHARSPACE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetCharSpace( page, value ) );
}

// HPDF_Page_SetCMYKFill( page, c, m, y, k ) --> nStatus
X_FUNC( HPDF_PAGE_SETCMYKFILL )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL c = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL m = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL k = ( HPDF_REAL )x_parnd( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetCMYKFill( page, c, m, y, k ) );
}

// HPDF_Page_SetCMYKStroke( page, c, m, y, k ) --> nStatus
X_FUNC( HPDF_PAGE_SETCMYKSTROKE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL c = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL m = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL k = ( HPDF_REAL )x_parnd( ctx, 5 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetCMYKStroke( page, c, m, y, k ) );
}

// HPDF_Page_SetDash( page, dash_ptn, num_param, phase ) --> nStatus
X_FUNC( HPDF_PAGE_SETDASH )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL *dashPtn = x_hpdfReals( x_paritem( ctx, 2 ) );
   HPDF_UINT numParam = ( HPDF_UINT )x_parnint( ctx, 3 );
   HPDF_REAL phase = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_STATUS result = HPDF_Page_SetDash( page, dashPtn, numParam, phase );

   x_xfree( dashPtn );
   x_retnint( ctx, ( int64_t )result );
}

// HPDF_Page_SetExtGState( page, ext_gstate ) --> nStatus
X_FUNC( HPDF_PAGE_SETEXTGSTATE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_ExtGState extGstate = ( HPDF_ExtGState )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetExtGState( page, extGstate ) );
}

// HPDF_Page_SetFlat( page, flatness ) --> nStatus
X_FUNC( HPDF_PAGE_SETFLAT )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_SetFlat( ( HPDF_Page )x_parptr( ctx, 1 ), ( HPDF_REAL )x_parnd( ctx, 2 ) ) );
}

// HPDF_Page_SetFontAndSize( page, font, size ) --> nStatus
X_FUNC( HPDF_PAGE_SETFONTANDSIZE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Font font = ( HPDF_Font )x_parptr( ctx, 2 );
   HPDF_REAL size = ( HPDF_REAL )x_parnd( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetFontAndSize( page, font, size ) );
}

// HPDF_Page_SetGrayFill( page, gray ) --> nStatus
X_FUNC( HPDF_PAGE_SETGRAYFILL )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL gray = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetGrayFill( page, gray ) );
}

// HPDF_Page_SetGrayStroke( page, gray ) --> nStatus
X_FUNC( HPDF_PAGE_SETGRAYSTROKE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL gray = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetGrayStroke( page, gray ) );
}

// HPDF_Page_SetHeight( page, value ) --> nStatus
X_FUNC( HPDF_PAGE_SETHEIGHT )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_SetHeight( ( HPDF_Page )x_parptr( ctx, 1 ), ( HPDF_REAL )x_parnd( ctx, 2 ) ) );
}

// HPDF_Page_SetHorizontalScalling( page, value ) --> nStatus
X_FUNC( HPDF_PAGE_SETHORIZONTALSCALLING )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetHorizontalScalling( page, value ) );
}

// HPDF_Page_SetLineCap( page, line_cap ) --> nStatus
X_FUNC( HPDF_PAGE_SETLINECAP )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_LineCap lineCap = ( HPDF_LineCap )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetLineCap( page, lineCap ) );
}

// HPDF_Page_SetLineJoin( page, line_join ) --> nStatus
X_FUNC( HPDF_PAGE_SETLINEJOIN )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_LineJoin lineJoin = ( HPDF_LineJoin )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetLineJoin( page, lineJoin ) );
}

// HPDF_Page_SetLineWidth( page, line_width ) --> nStatus
X_FUNC( HPDF_PAGE_SETLINEWIDTH )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL lineWidth = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetLineWidth( page, lineWidth ) );
}

// HPDF_Page_SetMiterLimit( page, miter_limit ) --> nStatus
X_FUNC( HPDF_PAGE_SETMITERLIMIT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL miterLimit = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetMiterLimit( page, miterLimit ) );
}

// HPDF_Page_SetRGBFill( page, r, g, b ) --> nStatus
X_FUNC( HPDF_PAGE_SETRGBFILL )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL r = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL g = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL b = ( HPDF_REAL )x_parnd( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetRGBFill( page, r, g, b ) );
}

// HPDF_Page_SetRGBStroke( page, r, g, b ) --> nStatus
X_FUNC( HPDF_PAGE_SETRGBSTROKE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL r = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL g = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL b = ( HPDF_REAL )x_parnd( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetRGBStroke( page, r, g, b ) );
}

// HPDF_Page_SetRotate( page, angle ) --> nStatus
X_FUNC( HPDF_PAGE_SETROTATE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_UINT16 angle = ( HPDF_UINT16 )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetRotate( page, angle ) );
}

// HPDF_Page_SetShading( page, shading ) --> nStatus
X_FUNC( HPDF_PAGE_SETSHADING )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_Shading shading = ( HPDF_Shading )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetShading( page, shading ) );
}

// HPDF_Page_SetSize( page, size, direction ) --> nStatus
X_FUNC( HPDF_PAGE_SETSIZE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_PageSizes size = ( HPDF_PageSizes )x_parnint( ctx, 2 );
   HPDF_PageDirection direction = ( HPDF_PageDirection )x_parnint( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetSize( page, size, direction ) );
}

// HPDF_Page_SetSlideShow( page, type, disp_time, trans_time ) --> nStatus
X_FUNC( HPDF_PAGE_SETSLIDESHOW )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_TransitionStyle type = ( HPDF_TransitionStyle )x_parnint( ctx, 2 );
   HPDF_REAL dispTime = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL transTime = ( HPDF_REAL )x_parnd( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetSlideShow( page, type, dispTime, transTime ) );
}

// HPDF_Page_SetTextLeading( page, value ) --> nStatus
X_FUNC( HPDF_PAGE_SETTEXTLEADING )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetTextLeading( page, value ) );
}

// HPDF_Page_SetTextMatrix( page, a, b, c, d, x, y ) --> nStatus
X_FUNC( HPDF_PAGE_SETTEXTMATRIX )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL a = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL b = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL c = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL d = ( HPDF_REAL )x_parnd( ctx, 5 );
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 6 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 7 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetTextMatrix( page, a, b, c, d, x, y ) );
}

// HPDF_Page_SetTextRaise( page, value ) --> nStatus
X_FUNC( HPDF_PAGE_SETTEXTRAISE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetTextRaise( page, value ) );
}

// HPDF_Page_SetTextRenderingMode( page, mode ) --> nStatus
X_FUNC( HPDF_PAGE_SETTEXTRENDERINGMODE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_TextRenderingMode mode = ( HPDF_TextRenderingMode )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetTextRenderingMode( page, mode ) );
}

// HPDF_Page_SetTextRise( page, value ) --> nStatus
X_FUNC( HPDF_PAGE_SETTEXTRISE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetTextRise( page, value ) );
}

// HPDF_Page_SetWidth( page, value ) --> nStatus
X_FUNC( HPDF_PAGE_SETWIDTH )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_SetWidth( ( HPDF_Page )x_parptr( ctx, 1 ), ( HPDF_REAL )x_parnd( ctx, 2 ) ) );
}

// HPDF_Page_SetWordSpace( page, value ) --> nStatus
X_FUNC( HPDF_PAGE_SETWORDSPACE )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL value = ( HPDF_REAL )x_parnd( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_Page_SetWordSpace( page, value ) );
}

// HPDF_Page_SetZoom( page, zoom ) --> nStatus
X_FUNC( HPDF_PAGE_SETZOOM )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_SetZoom( ( HPDF_Page )x_parptr( ctx, 1 ), ( HPDF_REAL )x_parnd( ctx, 2 ) ) );
}

// HPDF_Page_ShowText( page, text ) --> nStatus
X_FUNC( HPDF_PAGE_SHOWTEXT )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_ShowText( ( HPDF_Page )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_Page_ShowTextNextLine( page, text ) --> nStatus
X_FUNC( HPDF_PAGE_SHOWTEXTNEXTLINE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_ShowTextNextLine( ( HPDF_Page )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_Page_ShowTextNextLineEx( page, word_space, char_space, text ) --> nStatus
X_FUNC( HPDF_PAGE_SHOWTEXTNEXTLINEEX )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL wordSpace = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL charSpace = ( HPDF_REAL )x_parnd( ctx, 3 );
   const char *text = x_hpdfString( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_Page_ShowTextNextLineEx( page, wordSpace, charSpace, text ) );
}

// HPDF_Page_Stroke( page ) --> nStatus
X_FUNC( HPDF_PAGE_STROKE )
{
   x_retnint( ctx, ( int64_t )HPDF_Page_Stroke( ( HPDF_Page )x_parptr( ctx, 1 ) ) );
}

// HPDF_Page_TextOut( page, xpos, ypos, text ) --> nStatus
X_FUNC( HPDF_PAGE_TEXTOUT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL xpos = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL ypos = ( HPDF_REAL )x_parnd( ctx, 3 );
   const char *text = x_hpdfString( ctx, 4 );

   x_retnint( ctx, ( int64_t )HPDF_Page_TextOut( page, xpos, ypos, text ) );
}

// Write text into a rectangle with line breaks; nAlign is HPDF_TALIGN_*, nLen gets the number of bytes
// written (the rest did not fit)
// HPDF_Page_TextRect( pPage, nLeft, nTop, nRight, nBottom, cText, nAlign, @nLen ) --> nStatus
X_FUNC( HPDF_PAGE_TEXTRECT )
{
   HPDF_Page page = ( HPDF_Page )x_parptr( ctx, 1 );
   HPDF_REAL left = ( HPDF_REAL )x_parnd( ctx, 2 );
   HPDF_REAL top = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL right = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL bottom = ( HPDF_REAL )x_parnd( ctx, 5 );
   const char *text = x_hpdfString( ctx, 6 );
   HPDF_TextAlignment align = ( HPDF_TextAlignment )x_parnint( ctx, 7 );
   HPDF_UINT len = 0;
   HPDF_STATUS status;

   status = HPDF_Page_TextRect( page, left, top, right, bottom, text, align, &len );
   x_stornll( ctx, ( long long )len, 8 );
   x_retnint( ctx, ( int64_t )status );
}

// HPDF_Page_TextWidth( page, text ) --> nValue
X_FUNC( HPDF_PAGE_TEXTWIDTH )
{
   x_retnd( ctx, ( double )HPDF_Page_TextWidth( ( HPDF_Page )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_PopupAnnot_SetOpened( annot, opened ) --> nStatus
X_FUNC( HPDF_POPUPANNOT_SETOPENED )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_BOOL opened = x_hpdfBool( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_PopupAnnot_SetOpened( annot, opened ) );
}

// HPDF_ProjectionAnnot_SetExData( annot, exdata ) --> nStatus
X_FUNC( HPDF_PROJECTIONANNOT_SETEXDATA )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_ExData exdata = ( HPDF_ExData )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_ProjectionAnnot_SetExData( annot, exdata ) );
}

// Read the next nSize bytes of the stream made by HPDF_SaveToStream(); nSize gets the count read,
// HPDF_STREAM_EOF ends the stream
// HPDF_ReadFromStream( pDoc, @cBuffer, @nSize ) --> nStatus
X_FUNC( HPDF_READFROMSTREAM )
{
   HPDF_UINT32 size = ( HPDF_UINT32 )IIF( x_parnint( ctx, 3 ) > 0, x_parnint( ctx, 3 ), 4096 );
   HPDF_BYTE *buffer = ( HPDF_BYTE * )x_xgrab( ( size_t )size + 1 );
   HPDF_STATUS status = HPDF_ReadFromStream( ( HPDF_Doc )x_parptr( ctx, 1 ), buffer, &size );

   buffer[size] = '\0';
   x_storclen_buffer( ctx, ( char * )buffer, size, 2 );
   x_stornll( ctx, ( long long )size, 3 );
   x_retnint( ctx, ( int64_t )status );
}

// HPDF_ResetError( pdf ) --> NIL
X_FUNC( HPDF_RESETERROR )
{
   HPDF_ResetError( ( HPDF_Doc )x_parptr( ctx, 1 ) );
}

// HPDF_ResetStream( pdf ) --> nStatus
X_FUNC( HPDF_RESETSTREAM )
{
   x_retnint( ctx, ( int64_t )HPDF_ResetStream( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_SaveToFile( pdf, file_name ) --> nStatus
X_FUNC( HPDF_SAVETOFILE )
{
   x_retnint( ctx, ( int64_t )HPDF_SaveToFile( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_SaveToStream( pdf ) --> nStatus
X_FUNC( HPDF_SAVETOSTREAM )
{
   x_retnint( ctx, ( int64_t )HPDF_SaveToStream( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_SetCompressionMode( pdf, mode ) --> nStatus
X_FUNC( HPDF_SETCOMPRESSIONMODE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_UINT mode = ( HPDF_UINT )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_SetCompressionMode( pdf, mode ) );
}

// HPDF_SetCurrentEncoder( pdf, encoding_name ) --> nStatus
X_FUNC( HPDF_SETCURRENTENCODER )
{
   x_retnint( ctx, ( int64_t )HPDF_SetCurrentEncoder( ( HPDF_Doc )x_parptr( ctx, 1 ), x_hpdfString( ctx, 2 ) ) );
}

// HPDF_SetEncryptionMode( pdf, mode, key_len ) --> nStatus
X_FUNC( HPDF_SETENCRYPTIONMODE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_EncryptMode mode = ( HPDF_EncryptMode )x_parnint( ctx, 2 );
   HPDF_UINT keyLen = ( HPDF_UINT )x_parnint( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_SetEncryptionMode( pdf, mode, keyLen ) );
}

// Only removes the error handler: errors are read with HPDF_GetError() and HPDF_GetErrorDetail()
// HPDF_SetErrorHandler( pDoc, NIL ) --> nStatus
X_FUNC( HPDF_SETERRORHANDLER )
{
   x_retnint( ctx, ( int64_t )HPDF_SetErrorHandler( ( HPDF_Doc )x_parptr( ctx, 1 ), NULL ) );
}

// HPDF_SetInfoAttr( pdf, type, value ) --> nStatus
X_FUNC( HPDF_SETINFOATTR )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_InfoType type = ( HPDF_InfoType )x_parnint( ctx, 2 );
   const char *value = x_hpdfString( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_SetInfoAttr( pdf, type, value ) );
}

// HPDF_SetInfoDateAttr( pdf, type, value ) --> nStatus
X_FUNC( HPDF_SETINFODATEATTR )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_InfoType type = ( HPDF_InfoType )x_parnint( ctx, 2 );
   HPDF_Date value = x_hpdfDate( x_paritem( ctx, 3 ) );

   x_retnint( ctx, ( int64_t )HPDF_SetInfoDateAttr( pdf, type, value ) );
}

// HPDF_SetOpenAction( pdf, open_action ) --> nStatus
X_FUNC( HPDF_SETOPENACTION )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_Destination openAction = ( HPDF_Destination )x_parptr( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_SetOpenAction( pdf, openAction ) );
}

// HPDF_SetPageLayout( pdf, layout ) --> nStatus
X_FUNC( HPDF_SETPAGELAYOUT )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_PageLayout layout = ( HPDF_PageLayout )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_SetPageLayout( pdf, layout ) );
}

// HPDF_SetPageMode( pdf, mode ) --> nStatus
X_FUNC( HPDF_SETPAGEMODE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_PageMode mode = ( HPDF_PageMode )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_SetPageMode( pdf, mode ) );
}

// HPDF_SetPagesConfiguration( pdf, page_per_pages ) --> nStatus
X_FUNC( HPDF_SETPAGESCONFIGURATION )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_UINT pagePerPages = ( HPDF_UINT )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_SetPagesConfiguration( pdf, pagePerPages ) );
}

// HPDF_SetPassword( pdf, owner_passwd, user_passwd ) --> nStatus
X_FUNC( HPDF_SETPASSWORD )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   const char *ownerPasswd = x_hpdfString( ctx, 2 );
   const char *userPasswd = x_hpdfString( ctx, 3 );

   x_retnint( ctx, ( int64_t )HPDF_SetPassword( pdf, ownerPasswd, userPasswd ) );
}

// HPDF_SetPDFAConformance( pdf, pdfa_type ) --> nStatus
X_FUNC( HPDF_SETPDFACONFORMANCE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_PDFAType pdfaType = ( HPDF_PDFAType )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_SetPDFAConformance( pdf, pdfaType ) );
}

// HPDF_SetPermission( pdf, permission ) --> nStatus
X_FUNC( HPDF_SETPERMISSION )
{
   x_retnint( ctx, ( int64_t )HPDF_SetPermission( ( HPDF_Doc )x_parptr( ctx, 1 ), ( HPDF_UINT )x_parnint( ctx, 2 ) ) );
}

// HPDF_SetViewerPreference( pdf, value ) --> nStatus
X_FUNC( HPDF_SETVIEWERPREFERENCE )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_UINT value = ( HPDF_UINT )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_SetViewerPreference( pdf, value ) );
}

// HPDF_Shading_AddVertexRGB( shading, edgeFlag, x, y, r, g, b ) --> nStatus
X_FUNC( HPDF_SHADING_ADDVERTEXRGB )
{
   HPDF_Shading shading = ( HPDF_Shading )x_parptr( ctx, 1 );
   int flag = ( int )x_parnint( ctx, 2 );
   HPDF_Shading_FreeFormTriangleMeshEdgeFlag edgeFlag = ( HPDF_Shading_FreeFormTriangleMeshEdgeFlag )flag;
   HPDF_REAL x = ( HPDF_REAL )x_parnd( ctx, 3 );
   HPDF_REAL y = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_UINT8 r = ( HPDF_UINT8 )x_parnint( ctx, 5 );
   HPDF_UINT8 g = ( HPDF_UINT8 )x_parnint( ctx, 6 );
   HPDF_UINT8 b = ( HPDF_UINT8 )x_parnint( ctx, 7 );

   x_retnint( ctx, ( int64_t )HPDF_Shading_AddVertexRGB( shading, edgeFlag, x, y, r, g, b ) );
}

// HPDF_Shading_New( pdf, type, colorSpace, xMin, xMax, yMin, yMax ) --> pShading
X_FUNC( HPDF_SHADING_NEW )
{
   HPDF_Doc pdf = ( HPDF_Doc )x_parptr( ctx, 1 );
   HPDF_ShadingType type = ( HPDF_ShadingType )x_parnint( ctx, 2 );
   HPDF_ColorSpace colorSpace = ( HPDF_ColorSpace )x_parnint( ctx, 3 );
   HPDF_REAL xMin = ( HPDF_REAL )x_parnd( ctx, 4 );
   HPDF_REAL xMax = ( HPDF_REAL )x_parnd( ctx, 5 );
   HPDF_REAL yMin = ( HPDF_REAL )x_parnd( ctx, 6 );
   HPDF_REAL yMax = ( HPDF_REAL )x_parnd( ctx, 7 );

   x_hpdfRetHandle( ctx, HPDF_Shading_New( pdf, type, colorSpace, xMin, xMax, yMin, yMax ) );
}

// HPDF_TextAnnot_SetIcon( annot, icon ) --> nStatus
X_FUNC( HPDF_TEXTANNOT_SETICON )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_AnnotIcon icon = ( HPDF_AnnotIcon )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )HPDF_TextAnnot_SetIcon( annot, icon ) );
}

// HPDF_TextAnnot_SetOpened( annot, opened ) --> nStatus
X_FUNC( HPDF_TEXTANNOT_SETOPENED )
{
   x_retnint( ctx, ( int64_t )HPDF_TextAnnot_SetOpened( ( HPDF_Annotation )x_parptr( ctx, 1 ), x_hpdfBool( ctx, 2 ) ) );
}

// HPDF_TextMarkupAnnot_SetQuadPoints( annot, lb, rb, rt, lt ) --> nStatus
X_FUNC( HPDF_TEXTMARKUPANNOT_SETQUADPOINTS )
{
   HPDF_Annotation annot = ( HPDF_Annotation )x_parptr( ctx, 1 );
   HPDF_Point lb = x_hpdfPoint( x_paritem( ctx, 2 ) );
   HPDF_Point rb = x_hpdfPoint( x_paritem( ctx, 3 ) );
   HPDF_Point rt = x_hpdfPoint( x_paritem( ctx, 4 ) );
   HPDF_Point lt = x_hpdfPoint( x_paritem( ctx, 5 ) );

   x_retnint( ctx, ( int64_t )HPDF_TextMarkupAnnot_SetQuadPoints( annot, lb, rb, rt, lt ) );
}

// HPDF_UseCNSEncodings( pdf ) --> nStatus
X_FUNC( HPDF_USECNSENCODINGS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseCNSEncodings( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_UseCNSFonts( pdf ) --> nStatus
X_FUNC( HPDF_USECNSFONTS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseCNSFonts( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_UseCNTEncodings( pdf ) --> nStatus
X_FUNC( HPDF_USECNTENCODINGS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseCNTEncodings( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_UseCNTFonts( pdf ) --> nStatus
X_FUNC( HPDF_USECNTFONTS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseCNTFonts( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_UseJPEncodings( pdf ) --> nStatus
X_FUNC( HPDF_USEJPENCODINGS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseJPEncodings( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_UseJPFonts( pdf ) --> nStatus
X_FUNC( HPDF_USEJPFONTS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseJPFonts( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_UseKREncodings( pdf ) --> nStatus
X_FUNC( HPDF_USEKRENCODINGS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseKREncodings( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_UseKRFonts( pdf ) --> nStatus
X_FUNC( HPDF_USEKRFONTS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseKRFonts( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}

// HPDF_UseUTFEncodings( pdf ) --> nStatus
X_FUNC( HPDF_USEUTFENCODINGS )
{
   x_retnint( ctx, ( int64_t )HPDF_UseUTFEncodings( ( HPDF_Doc )x_parptr( ctx, 1 ) ) );
}
