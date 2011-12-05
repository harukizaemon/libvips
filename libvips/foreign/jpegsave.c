/* save to jpeg
 *
 * 24/11/11
 * 	- wrap a class around the jpeg writer
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#ifdef HAVE_EXIF
#ifdef UNTAGGED_EXIF
#include <exif-data.h>
#include <exif-loader.h>
#include <exif-ifd.h>
#include <exif-utils.h>
#else /*!UNTAGGED_EXIF*/
#include <libexif/exif-data.h>
#include <libexif/exif-loader.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-utils.h>
#endif /*UNTAGGED_EXIF*/
#endif /*HAVE_EXIF*/

#include <vips/vips.h>
#include <vips/buf.h>
#include <vips/internal.h>

/* jpeglib includes jconfig.h, which can define HAVE_STDLIB_H ... which we
 * also define. Make sure it's turned off.
 */
#ifdef HAVE_STDLIB_H
#undef HAVE_STDLIB_H
#endif /*HAVE_STDLIB_H*/

#include <jpeglib.h>
#include <jerror.h>

#include "jpeg.h"

typedef struct _VipsForeignSaveJpeg {
	VipsForeignSave parent_object;

	/* Quality factor.
	 */
	int Q;

	/* Profile to embed .. "none" means don't attach a profile.
	 */
	char *profile;

} VipsForeignSaveJpeg;

typedef VipsForeignSaveClass VipsForeignSaveJpegClass;

G_DEFINE_ABSTRACT_TYPE( VipsForeignSaveJpeg, vips_foreign_save_jpeg, 
	VIPS_TYPE_FOREIGN_SAVE );

#define UC VIPS_FORMAT_UCHAR

/* Type promotion for save ... just always go to uchar.
 */
static int bandfmt_jpeg[10] = {
/* UC  C   US  S   UI  I   F   X   D   DX */
   UC, UC, UC, UC, UC, UC, UC, UC, UC, UC
};

static void
vips_foreign_save_jpeg_class_init( VipsForeignSaveJpegClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsForeignSaveClass *save_class = (VipsForeignSaveClass *) class;

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	object_class->nickname = "jpegsave_base";
	object_class->description = _( "save jpeg" );

	save_class->saveable = VIPS_SAVEABLE_RGB_CMYK;
	save_class->format_table = bandfmt_jpeg;

	VIPS_ARG_INT( class, "Q", 10, 
		_( "Q" ), 
		_( "Q factor" ),
		VIPS_ARGUMENT_OPTIONAL_INPUT,
		G_STRUCT_OFFSET( VipsForeignSaveJpeg, Q ),
		1, 100, 75 );

	VIPS_ARG_STRING( class, "profile", 11, 
		_( "profile" ), 
		_( "ICC profile to embed" ),
		VIPS_ARGUMENT_OPTIONAL_INPUT,
		G_STRUCT_OFFSET( VipsForeignSaveJpeg, profile ),
		NULL );
}

static void
vips_foreign_save_jpeg_init( VipsForeignSaveJpeg *jpeg )
{
	jpeg->Q = 75;
}

typedef struct _VipsForeignSaveJpegFile {
	VipsForeignSaveJpeg parent_object;

	/* Filename for save.
	 */
	char *filename; 

} VipsForeignSaveJpegFile;

typedef VipsForeignSaveJpegClass VipsForeignSaveJpegFileClass;

G_DEFINE_TYPE( VipsForeignSaveJpegFile, vips_foreign_save_jpeg_file, 
	vips_foreign_save_jpeg_get_type() );

static int
vips_foreign_save_jpeg_file_build( VipsObject *object )
{
	VipsForeignSave *save = (VipsForeignSave *) object;
	VipsForeignSaveJpeg *jpeg = (VipsForeignSaveJpeg *) object;
	VipsForeignSaveJpegFile *file = (VipsForeignSaveJpegFile *) object;

	if( VIPS_OBJECT_CLASS( vips_foreign_save_jpeg_file_parent_class )->
		build( object ) )
		return( -1 );

	if( vips__jpeg_write_file( save->ready, file->filename,
		jpeg->Q, jpeg->profile ) )
		return( -1 );

	return( 0 );
}

static void
vips_foreign_save_jpeg_file_class_init( VipsForeignSaveJpegFileClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsForeignClass *foreign_class = (VipsForeignClass *) class;

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	object_class->nickname = "jpegsave";
	object_class->description = _( "save image to jpeg file" );
	object_class->build = vips_foreign_save_jpeg_file_build;

	foreign_class->suffs = vips__jpeg_suffs;

	VIPS_ARG_STRING( class, "filename", 1, 
		_( "Filename" ),
		_( "Filename to save to" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsForeignSaveJpegFile, filename ),
		NULL );
}

static void
vips_foreign_save_jpeg_file_init( VipsForeignSaveJpegFile *file )
{
}


/**
 * vips_jpegsave:
 * @in: image to save 
 * @filename: file to write to 
 * @Q: quality factor
 * @profile: attach this ICC profile
 * @...: %NULL-terminated list of optional named arguments
 *
 * Write a VIPS image to a file as JPEG.
 *
 * Use @Q to set the JPEG compression factor. Default 75.
 *
 * Use @profile to give the filename of a profile to be em,bedded in the JPEG.
 * This does not affect the pixels which are written, just the way 
 * they are tagged. You can use the special string "none" to mean 
 * "don't attach a profile".
 *
 * If no profile is specified and the VIPS header 
 * contains an ICC profile named VIPS_META_ICC_NAME ("icc-profile-data"), the
 * profile from the VIPS header will be attached.
 *
 * The image is automatically converted to RGB, Monochrome or CMYK before 
 * saving. Any metadata attached to the image is saved as EXIF, if possible.
 *
 * See also: vips_jpegsave_buffer(), vips_image_write_file().
 *
 * Returns: 0 on success, -1 on error.
 */
int
vips_jpegsave( VipsImage *in, const char *filename, ... )
{
	va_list ap;
	int result;

	va_start( ap, filename );
	result = vips_call_split( "jpegsave", ap, in, filename );
	va_end( ap );

	return( result );
}

typedef struct _VipsForeignSaveJpegBuffer {
	VipsForeignSaveJpeg parent_object;

	/* Save to a buffer.
	 */
	VipsArea *buf;

} VipsForeignSaveJpegBuffer;

typedef VipsForeignSaveJpegClass VipsForeignSaveJpegBufferClass;

G_DEFINE_TYPE( VipsForeignSaveJpegBuffer, vips_foreign_save_jpeg_buffer, 
	vips_foreign_save_jpeg_get_type() );

static int
vips_foreign_save_jpeg_buffer_build( VipsObject *object )
{
	VipsForeignSave *save = (VipsForeignSave *) object;
	VipsForeignSaveJpeg *jpeg = (VipsForeignSaveJpeg *) object;
	VipsForeignSaveJpegBuffer *file = (VipsForeignSaveJpegBuffer *) object;

	void *obuf;
	size_t olen;
	VipsArea *area;

	if( VIPS_OBJECT_CLASS( vips_foreign_save_jpeg_buffer_parent_class )->
		build( object ) )
		return( -1 );

	if( vips__jpeg_write_buffer( save->ready, 
		&obuf, &olen, jpeg->Q, jpeg->profile ) )
		return( -1 );

	area = vips_area_new_blob( (VipsCallbackFn) vips_free, obuf, olen );

	g_object_set( file, "buf", area, NULL );

	return( 0 );
}

static void
vips_foreign_save_jpeg_buffer_class_init( 
	VipsForeignSaveJpegBufferClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *object_class = (VipsObjectClass *) class;

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	object_class->nickname = "jpegsave_buffer";
	object_class->description = _( "save image to jpeg buffer" );
	object_class->build = vips_foreign_save_jpeg_buffer_build;

	VIPS_ARG_BOXED( class, "buffer", 1, 
		_( "Buffer" ),
		_( "Buffer to save to" ),
		VIPS_ARGUMENT_REQUIRED_OUTPUT, 
		G_STRUCT_OFFSET( VipsForeignSaveJpegBuffer, buf ),
		VIPS_TYPE_BLOB );
}

static void
vips_foreign_save_jpeg_buffer_init( VipsForeignSaveJpegBuffer *file )
{
}


/**
 * vips_jpegsave_buffer:
 * @in: image to save 
 * @buf: return output buffer here
 * @len: return output length here
 * @Q: JPEG quality factor
 * @profile: attach this ICC profile
 * @...: %NULL-terminated list of optional named arguments
 *
 * As vips_jpegsave(), but save to a memory buffer. 
 *
 * The address of the buffer is returned in @obuf, the length of the buffer in
 * @olen. You are responsible for freeing the buffer with g_free() when you
 * are done with it.
 *
 * See also: vips_jpegsave(), vips_image_write_to_file().
 *
 * Returns: 0 on success, -1 on error.
 */
int
vips_jpegsave_buffer( VipsImage *in, void **buf, size_t *len, ... )
{
	va_list ap;
	VipsArea *area;
	int result;

	va_start( ap, len );
	result = vips_call_split( "jpegsave_buffer", ap, in, &area );
	va_end( ap );

	if( buf ) {
		*buf = area->data;
		area->free_fn = NULL;
	}
	if( buf ) 
		*len = area->length;

	vips_area_unref( area );

	return( result );
}

typedef struct _VipsForeignSaveJpegMime {
	VipsForeignSaveJpeg parent_object;

} VipsForeignSaveJpegMime;

typedef VipsForeignSaveJpegClass VipsForeignSaveJpegMimeClass;

G_DEFINE_TYPE( VipsForeignSaveJpegMime, vips_foreign_save_jpeg_mime, 
	vips_foreign_save_jpeg_get_type() );

static int
vips_foreign_save_jpeg_mime_build( VipsObject *object )
{
	VipsForeignSave *save = (VipsForeignSave *) object;
	VipsForeignSaveJpeg *jpeg = (VipsForeignSaveJpeg *) object;

	void *obuf;
	size_t olen;

	if( VIPS_OBJECT_CLASS( vips_foreign_save_jpeg_mime_parent_class )->
		build( object ) )
		return( -1 );

	if( vips__jpeg_write_buffer( save->ready, 
		&obuf, &olen, jpeg->Q, jpeg->profile ) )
		return( -1 );

	printf( "Content-length: %zd\r\n", olen );
	printf( "Content-type: image/jpeg\r\n" );
	printf( "\r\n" );
	if( fwrite( obuf, sizeof( char ), olen, stdout ) != olen ) {
		vips_error( "VipsJpeg", "%s", _( "error writing output" ) );
		return( -1 );
	}
	fflush( stdout );

	g_free( obuf );

	return( 0 );
}

static void
vips_foreign_save_jpeg_mime_class_init( VipsForeignSaveJpegMimeClass *class )
{
	VipsObjectClass *object_class = (VipsObjectClass *) class;

	object_class->nickname = "jpegsave_mime";
	object_class->description = _( "save image to jpeg mime" );
	object_class->build = vips_foreign_save_jpeg_mime_build;

}

static void
vips_foreign_save_jpeg_mime_init( VipsForeignSaveJpegMime *mime )
{
}

/**
 * vips_jpegsave_mime:
 * @in: image to save 
 * @Q: JPEG quality factor
 * @profile: attach this ICC profile
 * @...: %NULL-terminated list of optional named arguments
 *
 * As vips_jpegsave(), but save as a mime jpeg on stdout.
 *
 * See also: vips_jpegsave(), vips_image_write_to_file().
 *
 * Returns: 0 on success, -1 on error.
 */
int
vips_jpegsave_mime( VipsImage *in, ... )
{
	va_list ap;
	int result;

	va_start( ap, in );
	result = vips_call_split( "jpegsave_mime", ap, in );
	va_end( ap );

	return( result );
}
