/*
 *  Copyright (C) 2014 Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 *
 *  thumbnailer.c
 *  lua-thumbnailer
 *
 *  Created by Masatoshi Teruya on 14/04/20.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <Imlib2.h>
#include <lauxlib.h>


// helper macros for lua_State
#define lstate_fn2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushcfunction(L,v); \
    lua_rawset(L,-3); \
}while(0)

#define lstate_num2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushnumber(L,v); \
    lua_rawset(L,-3); \
}while(0)


// MARK: lua binding
#define MODULE_MT   "thumbnailer"

enum img_align_e {
    IMG_ALIGN_NONE = 0,
    IMG_ALIGN_LEFT,
    IMG_ALIGN_CENTER,
    IMG_ALIGN_RIGHT,
    IMG_ALIGN_TOP,
    IMG_ALIGN_MIDDLE,
    IMG_ALIGN_BOTTOM
};

typedef struct {
    int w;
    int h;
} img_size_t;


typedef struct {
    int x;
    int y;
    int w;
    int h;
} img_bounds_t;


typedef struct {
    void *blob;
    size_t bytes;
    img_size_t size;
    img_size_t resize;
    uint8_t quality;
} img_t;


#define SETVAL_IN_RANGE(x,t,val,min,max) do { \
    if( val < min ){ \
        (x) = (t)min; \
    } \
    else if( val > max ){ \
        (x) = (t)max; \
    } \
    else { \
        (x) = (t)val; \
    } \
}while(0)


#define BOUNDS_ALIGN(bounds,align,size) do{ \
    switch( align ){ \
        case IMG_ALIGN_CENTER: \
            bounds.x = ( size.w - bounds.w ) / 2; \
        break; \
        case IMG_ALIGN_RIGHT: \
            bounds.x = size.w - bounds.w; \
        break; \
        case IMG_ALIGN_MIDDLE: \
            bounds.y = ( size.h - bounds.h ) / 2; \
        break; \
        case IMG_ALIGN_BOTTOM: \
            bounds.y = size.h - bounds.h; \
        break; \
    } \
}while(0)


static inline void liberr2errno( int err )
{
    switch( err )
    {
        case IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST:
        case IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT:
        case IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY:
        case IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE:
            errno = ENOENT;
        break;
        case IMLIB_LOAD_ERROR_PATH_TOO_LONG:
            errno = ENAMETOOLONG;
        break;
        case IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY:
            errno = EISDIR;
        break;
        case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ:
        case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE:
            errno = EACCES;
        break;
        case IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT:
            errno = EINVAL;
        break;
        case IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS:
            errno = EMLINK;
        break;
        case IMLIB_LOAD_ERROR_OUT_OF_MEMORY:
            errno = ENOMEM;
        break;
        case IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS:
            errno = EMFILE;
        break;
        case IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE:
            errno = ENOSPC;
        break;
        case IMLIB_LOAD_ERROR_NONE:
        default:
            errno = 0;
    }
}


static int img_load( img_t *img, const char *path )
{
    Imlib_Load_Error err = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image imimg = imlib_load_image_with_error_return( path, &err );
    
    if( img )
    {
        imlib_context_set_image( imimg );
        img->size.w =  imlib_image_get_width();
        img->size.h = imlib_image_get_height();
        // allocate buffer
        img->bytes = sizeof( DATA32 ) * (size_t)img->size.w * (size_t)img->size.h;
        img->blob = malloc( img->bytes );
        if( img->blob ){
            memcpy( img->blob, imlib_image_get_data_for_reading_only(), 
                    img->bytes );
            imlib_free_image_and_decache();
            
            img->quality = 100;
            img->resize = (img_size_t){ 0, 0 };
            return 0;
        }
        
        imlib_free_image_and_decache();
    }
    
    liberr2errno( err );
    return -1;
}


static inline void save2path( const char *path, uint8_t quality, ImlibLoadError *err )
{
    // set quality
    imlib_image_attach_data_value( "quality", NULL, quality, NULL );
    imlib_save_image_with_error_return( path, err );
    imlib_free_image_and_decache();
}


static int save_lua( lua_State *L )
{
    img_t *img = (img_t*)luaL_checkudata( L, 1, MODULE_MT );
    const char *path = luaL_checkstring( L, 2 );
    ImlibLoadError err = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image work = imlib_create_image_using_data( img->size.w, img->size.h,
                                                      img->blob );
    
    // set current image
    imlib_context_set_image( work );
    work = imlib_create_cropped_scaled_image( 0, 0, img->size.w, img->size.h, 
                                              img->resize.w, img->resize.h );
    imlib_free_image_and_decache();
    imlib_context_set_image( work );
    save2path( path, img->quality, &err );
    // failed
    if( err ){
        liberr2errno( err );
        lua_pushstring( L, strerror(errno) );
        return 2;
    }
    // success
    else {
        lua_pushnil( L );
    }
    
    return 1;
}


static int save_crop_lua( lua_State *L )
{
    img_t *img = (img_t*)luaL_checkudata( L, 1, MODULE_MT );
    const char *path = luaL_checkstring( L, 2 );
    uint8_t align = IMG_ALIGN_NONE;
    uint8_t halign = IMG_ALIGN_CENTER;
    uint8_t valign = IMG_ALIGN_MIDDLE;
    img_bounds_t bounds = (img_bounds_t){ 0, 0, 0, 0 };
    double aspect_org = 0;
    double aspect = 0;
    Imlib_Image work = NULL;
    ImlibLoadError err = IMLIB_LOAD_ERROR_NONE;
    
    // check alignment arguments
    // horizontal
    if( !lua_isnoneornil( L, 3 ) )
    {
        halign = (uint8_t)luaL_checkint( L, 3 );
        if( halign < IMG_ALIGN_LEFT || halign > IMG_ALIGN_RIGHT ){
            return luaL_argerror( L, 3, "horizontal align must be LEFT, RIGHT or CENTER" );
        }
    }
    // vertical
    if( !lua_isnoneornil( L, 4 ) )
    {
        valign = (uint8_t)luaL_checkinteger( L, 4 );
        if( valign < IMG_ALIGN_TOP || valign > IMG_ALIGN_BOTTOM ){
            return luaL_argerror( L, 4, "vertical align must be TOP, BOTTOM or MIDDLE" );
        }
    }
    
    // calculate bounds of cropped image by aspect ratio
    aspect_org = (double)img->size.w/(double)img->size.h;
    aspect = (double)img->resize.w/(double)img->resize.h;
    // based on height
    if( aspect_org > aspect ){
        bounds.h = img->size.h;
        bounds.w = (int)((double)img->size.h * aspect);
        align = (uint8_t)halign;
    }
    // based on width
    else if( aspect_org < aspect ){
        bounds.w = img->size.w;
        bounds.h = (int)((double)img->size.w / aspect);
        align = (uint8_t)valign;
    }
    // square
    else {
        bounds.w = img->size.w;
        bounds.h = img->size.h;
    }
    // calculate bounds position
    BOUNDS_ALIGN( bounds, align, img->size );
    
    // create image
    work = imlib_create_image_using_data( img->size.w, img->size.h, img->blob );
    imlib_context_set_image( work );
    work = imlib_create_cropped_scaled_image( bounds.x, bounds.y, bounds.w, 
                                              bounds.h, img->resize.w, 
                                              img->resize.h );
    imlib_free_image_and_decache();
    imlib_context_set_image( work );
    save2path( path, img->quality, &err );
    
    // failed
    if( err ){
        liberr2errno( err );
        lua_pushstring( L, strerror(errno) );
    }
    // success
    else {
        lua_pushnil( L );
    }
    
    return 1;
}


static int save_trim_lua( lua_State *L )
{
    img_t *img = (img_t*)luaL_checkudata( L, 1, MODULE_MT );
    const char *path = luaL_checkstring( L, 2 );
    img_bounds_t bounds = (img_bounds_t){ 0, 0, 0, 0 };
    double aspect_org = 0;
    double aspect = 0;
    ImlibLoadError err = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image work = NULL;
    
    // calculate bounds of image with maintaining aspect ratio.
    aspect_org = (double)img->size.w/(double)img->size.h;
    aspect = (double)img->resize.w/(double)img->resize.h;
    // based on width
    if( aspect_org > aspect ){
        bounds.w = img->resize.w;
        bounds.h = (int)((double)bounds.w / aspect_org);
    }
    // based on height
    else if( aspect_org < aspect ){
        bounds.h = img->resize.h;
        bounds.w = (int)((double)bounds.h * aspect_org);
    }
    // square
    else {
        bounds.w = img->resize.w;
        bounds.h = img->resize.h;
    }
    
    // create image
    work = imlib_create_image_using_data( img->size.w, img->size.h, img->blob );
    // set current image
    imlib_context_set_image( work );
    work = imlib_create_cropped_scaled_image( 0, 0, img->size.w, img->size.h, 
                                              bounds.w, bounds.h );
    imlib_free_image_and_decache();
    imlib_context_set_image( work );
    save2path( path, img->quality, &err );
    
    // failed
    if( err ){
        liberr2errno( err );
        lua_pushstring( L, strerror(errno) );
    }
    // success
    else {
        lua_pushnil( L );
    }
    
    return 1;
}


static int save_aspect_lua( lua_State *L )
{
    img_t *img = (img_t*)luaL_checkudata( L, 1, MODULE_MT );
    const char *path = luaL_checkstring( L, 2 );
    uint8_t align = IMG_ALIGN_NONE;
    lua_Integer halign = IMG_ALIGN_CENTER;
    lua_Integer valign = IMG_ALIGN_MIDDLE;
    float hue = 0;
    float saturation = 0;
    float lightness = 0;
    int alpha = 255;
    img_bounds_t bounds = (img_bounds_t){ 0, 0, 0, 0 };
    double aspect_org = 0;
    double aspect = 0;
    ImlibLoadError err = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image work = NULL;
    Imlib_Image boundsImage = NULL;

    
    // check arguments
    // hue
    if( !lua_isnoneornil( L, 3 ) ){
        lua_Number arg = luaL_checknumber( L, 3 );
        SETVAL_IN_RANGE( hue, float, arg, 0, 360 );
    }
    // saturation
    if( !lua_isnoneornil( L, 4 ) ){
        lua_Number arg = luaL_checknumber( L, 4 );
        SETVAL_IN_RANGE( saturation, float, arg, 0, 1 );
    }
    // lightness
    if( !lua_isnoneornil( L, 5 ) ){
        lua_Number arg = luaL_checknumber( L, 5 );
        SETVAL_IN_RANGE( lightness, float, arg, 0, 1 );
    }
    // alpha
    if( !lua_isnoneornil( L, 6 ) ){
        lua_Integer arg = luaL_checkinteger( L, 6 );
        SETVAL_IN_RANGE( alpha, int, arg, 0, 255 );
    }
    // horizontal
    if( !lua_isnoneornil( L, 7 ) )
    {
        halign = (uint8_t)luaL_checkint( L, 7 );
        if( halign < IMG_ALIGN_LEFT || halign > IMG_ALIGN_RIGHT ){
            return luaL_argerror( L, 7, "horizontal align must be LEFT, RIGHT or CENTER" );
        }
    }
    // vertical
    if( !lua_isnoneornil( L, 8 ) )
    {
        valign = (uint8_t)luaL_checkinteger( L, 8 );
        if( valign < IMG_ALIGN_TOP || valign > IMG_ALIGN_BOTTOM ){
            return luaL_argerror( L, 4, "vertical align must be TOP, BOTTOM or MIDDLE" );
        }
    }
    
    
    // calculate bounds of image with maintaining aspect ratio
    aspect_org = (double)img->size.w/(double)img->size.h;
    aspect = (double)img->resize.w/(double)img->resize.h;
    // based on width
    if( aspect_org > aspect ){
        bounds.w = img->resize.w;
        bounds.h = (int)((double)bounds.w / aspect_org);
        align = (uint8_t)valign;
    }
    // based on height
    else if( aspect_org < aspect ){
        bounds.h = img->resize.h;
        bounds.w = (int)((double)bounds.h * aspect_org);
        align = (uint8_t)halign;
    }
    // square
    else {
        bounds.w = img->resize.w;
        bounds.h = img->resize.h;
    }
    // calculate bounds position
    BOUNDS_ALIGN( bounds, align, img->resize );
    
    // create image
    work = imlib_create_image_using_data( img->size.w, img->size.h, img->blob );
    // set current image
    imlib_context_set_image( work );
    work = imlib_create_cropped_scaled_image( 0, 0, img->size.w, img->size.h, 
                                              bounds.w, bounds.h );
    imlib_free_image_and_decache();
    boundsImage = imlib_create_image( img->resize.w, img->resize.h );
    imlib_context_set_image( boundsImage );
    imlib_context_set_color_hlsa( hue, lightness, saturation, alpha );
    imlib_image_fill_rectangle( 0, 0, img->resize.w, img->resize.h );
    imlib_blend_image_onto_image( work, 0, 0, 0, bounds.w, bounds.h, bounds.x, 
                                  bounds.y, bounds.w, bounds.h );
    imlib_context_set_image( work );
    imlib_free_image_and_decache();
    imlib_context_set_image( boundsImage );
    save2path( path, img->quality, &err );
    
    // failed
    if( err ){
        liberr2errno( err );
        lua_pushstring( L, strerror(errno) );
    }
    // success
    else {
        lua_pushnil( L );
    }
    
    return 1;
}

    
    return 1;
}


static int rawsize_lua( lua_State *L )
{
    img_t *img = (img_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    lua_pushinteger( L, img->size.w );
    lua_pushinteger( L, img->size.h );
    
    return 2;
}


static int size_lua( lua_State *L )
{
    img_t *img = (img_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    if( !lua_isnoneornil( L, 2 ) )
    {
        int width = luaL_checkint( L, 2 );
        int height = luaL_checkint( L, 3 );
        
        if( width < 0 ){
            luaL_argerror( L, 2, "width must be larger than 0" );
        }
        else if( height < 0 ){
            luaL_argerror( L, 3, "height must be larger than 0" );
        }
        
        img->resize.w = width;
        img->resize.h = height;
    }
    
    lua_pushinteger( L, img->resize.w );
    lua_pushinteger( L, img->resize.h );
    
    return 2;
}


static int quality_lua( lua_State *L )
{
    img_t *img = (img_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    if( !lua_isnoneornil( L, 2 ) ){
        int quality = luaL_checkint( L, 2 );
        SETVAL_IN_RANGE( img->quality, uint8_t, quality, 0, 100 );
    }
    
    lua_pushinteger( L, img->quality );
    
    return 1;
}


static int free_lua( lua_State *L )
{
    img_t *img = (img_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    if( img->blob ){
        free( img->blob );
        img->blob = NULL;
    }
    
    return 0;
}


static int dealloc_gc( lua_State *L )
{
    img_t *img = (img_t*)lua_touserdata( L, 1 );
    
    if( img->blob ){
        free( img->blob );
    }
    
    return 0;
}


static int tostring_lua( lua_State *L )
{
    lua_pushfstring( L, MODULE_MT ": %p", lua_touserdata( L, 1 ) );
    return 1;
}


static int load_lua( lua_State *L )
{
    const char *path = luaL_checkstring( L, 1 );
    img_t *img = (img_t*)lua_newuserdata( L, sizeof( img_t ) );
    
    if( img && img_load( img, path ) == 0 ){
        // set metatable
        luaL_getmetatable( L, MODULE_MT );
        lua_setmetatable( L, -2 );
        return 1;
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


// module definition register
static void define_mt( lua_State *L, struct luaL_Reg mmethod[], 
                       struct luaL_Reg method[] )
{
    int i = 0;
    
    // create table __metatable
    luaL_newmetatable( L, MODULE_MT );
    // metamethods
    while( mmethod[i].name ){
        lstate_fn2tbl( L, mmethod[i].name, mmethod[i].func );
        i++;
    }
    // methods
    lua_pushstring( L, "__index" );
    lua_newtable( L );
    i = 0;
    while( method[i].name ){
        lstate_fn2tbl( L, method[i].name, method[i].func );
        i++;
    }
    lua_rawset( L, -3 );
    lua_pop( L, 1 );
}


LUALIB_API int luaopen_thumbnailer( lua_State *L )
{
    struct luaL_Reg mmethod[] = {
        { "__gc", dealloc_gc },
        { "__tostring", tostring_lua },
        { NULL, NULL }
    };
    struct luaL_Reg method[] = {
        // method
        { "free", free_lua },
        { "rawsize", rawsize_lua },
        { "size", size_lua },
        { "quality", quality_lua },
        { "save", save_lua },
        { "saveCrop", save_crop_lua },
        { "saveTrim", save_trim_lua },
        { "saveAspect", save_aspect_lua },
        { NULL, NULL }
    };
    
    define_mt( L, mmethod, method );
    // method
    lua_newtable( L );
    lstate_fn2tbl( L, "load", load_lua );
    // constants
    // alignments
    lstate_num2tbl( L, "LEFT", IMG_ALIGN_LEFT );
    lstate_num2tbl( L, "CENTER", IMG_ALIGN_CENTER );
    lstate_num2tbl( L, "RIGHT", IMG_ALIGN_RIGHT );
    lstate_num2tbl( L, "TOP", IMG_ALIGN_TOP );
    lstate_num2tbl( L, "MIDDLE", IMG_ALIGN_MIDDLE );
    lstate_num2tbl( L, "BOTTOM", IMG_ALIGN_BOTTOM );
    
    return 1;
}
