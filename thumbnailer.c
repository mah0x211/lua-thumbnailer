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
    IMG_ALIGN_BOTTOM,
};

typedef struct {
    int w;
    int h;
} img_size_t;

typedef struct {
    Imlib_Image img;
    int quality;
    int align;
    int crop;
    img_size_t size;
    img_size_t resize;
} context_t;


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


#define check_align(L,idx,align,min,max, errmsg ) do { \
    if( !lua_isnoneornil( L, idx ) ){ \
        int val = luaL_checkint( L, idx ); \
        if( val && ( val < min || val > max ) ){ \
            return luaL_error( L, errmsg ); \
        } \
        *(align) |= 1 << val; \
    } \
}while(0)


static int resize_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    int width = luaL_checkint( L, 2 );
    int height = luaL_checkint( L, 3 );
    int crop = 0;
    int align = IMG_ALIGN_NONE;
    
    if( width < 1 ){
        return luaL_argerror( L, 2, "width must be larger than 0" );
    }
    else if( height < 1 ){
        return luaL_argerror( L, 3, "height must be larger than 0" );
    }
    
    // check crop flag
    if( ( width != ctx->size.w || height != ctx->size.h ) && 
        !lua_isnoneornil( L, 4 ) )
    {
        luaL_checktype( L, 4, LUA_TBOOLEAN );
        crop = lua_toboolean( L, 4 );
        // alignment
        // horizontal
        check_align( L, 5, &align, IMG_ALIGN_LEFT, IMG_ALIGN_RIGHT, 
                     "horizontal alignment must be LEFT, RIGHT or CENTER value" );
        // vertical
        check_align( L, 6, &align, IMG_ALIGN_TOP, IMG_ALIGN_BOTTOM, 
                     "vertical alignment must be TOP, BOTTOM or MIDDLE value" );
    }
    
    ctx->resize.w = width;
    ctx->resize.h = height;
    ctx->crop = crop;
    ctx->align = align;
    
    return 0;
}



static int save_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    const char *path = luaL_checkstring( L, 2 );
    Imlib_Load_Error err = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image work = ctx->img;
    img_size_t src = { ctx->size.w, ctx->size.h };
    double aspect_org = (double)ctx->size.w/(double)ctx->size.h;
    double aspect = (double)ctx->resize.w/(double)ctx->resize.h;
    int x = 0;
    int y = 0;

    // set current image
    imlib_context_set_image( work );
    // clone current image for backup
    ctx->img = imlib_clone_image();
    
    // crop
    if( ctx->crop )
    {
        if( aspect_org > aspect )
        {
            src.w = (int)((double)ctx->size.h * aspect);
            src.h = ctx->size.h;
            if( ctx->align & ( 1 << IMG_ALIGN_CENTER ) ){
                x = ( ctx->size.w - src.w ) / 2;
            }
            else if( ctx->align & ( 1 << IMG_ALIGN_RIGHT ) ){
                x = ctx->size.w - src.w;
            }
        }
        else if( aspect_org < aspect )
        {
            src.h = (int)((double)ctx->size.w / aspect);
            src.w = ctx->size.w;
            if( ctx->align & ( 1 << IMG_ALIGN_MIDDLE ) ){
                y = ( ctx->size.h - src.h ) / 2;
            }
            else if( ctx->align & ( 1 << IMG_ALIGN_BOTTOM ) ){
                y = ctx->size.h - src.h;
            }
        }
    }
    
    work = imlib_create_cropped_scaled_image( x, y, src.w, src.h, 
                                              ctx->resize.w, ctx->resize.h );
    imlib_free_image_and_decache();
    imlib_context_set_image( work );
    
    // quality
    imlib_image_attach_data_value( "quality", NULL, ctx->quality, NULL );    
    imlib_save_image_with_error_return( path, (ImlibLoadError*)&err );
    imlib_free_image_and_decache();
    // failed
    if( err ){
        liberr2errno( err );
        lua_pushstring( L, strerror(errno) );
        return 1;
    }
    
    return 0;
}


static int width_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    lua_pushnumber( L, ctx->size.w );
    
    return 1;
}


static int height_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    lua_pushnumber( L, ctx->size.h );
    
    return 1;
}


static int quality_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    if( !lua_isnoneornil( L, 2 ) )
    {
        int quality = luaL_checkint( L, 2 );
        
        if( quality < 0 ){
            luaL_argerror( L, 2, "quality must be larger than 0" );
        }
        ctx->quality = quality;
    }
    
    lua_pushinteger( L, ctx->quality );
    
    return 1;
}


static int dealloc_gc( lua_State *L )
{
    if( imlib_context_get_image() )
    {
        if( imlib_get_cache_size() ){
            imlib_free_image_and_decache();
        }
        else {
            imlib_free_image();
        }
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
    context_t *ctx = lua_newuserdata( L, sizeof( context_t ) );
    
    if( ctx )
    {
        Imlib_Load_Error err = IMLIB_LOAD_ERROR_NONE;
        
        if( imlib_context_get_image() )
        {
            if( imlib_get_cache_size() ){
                imlib_free_image_and_decache();
            }
            else {
                imlib_free_image();
            }
        }
        
        ctx->img = imlib_load_image_with_error_return( path, &err );
        if( ctx->img )
        {
            ctx->quality = 100;
            ctx->crop = 0;
            ctx->align = IMG_ALIGN_NONE;
            ctx->resize = (img_size_t){ 0, 0 };
            imlib_context_set_image( ctx->img );
            ctx->size.w =  imlib_image_get_width();
            ctx->size.h = imlib_image_get_height();
            ctx->img = imlib_clone_image();
            imlib_free_image_and_decache();
            // set metatable
            luaL_getmetatable( L, MODULE_MT );
            lua_setmetatable( L, -2 );
            return 1;
        }
        
        liberr2errno( err );
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
        { "width", width_lua },
        { "height", height_lua },
        { "quality", quality_lua },
        { "resize", resize_lua },
        { "save", save_lua },
        { NULL, NULL }
    };
    
    define_mt( L, mmethod, method );
    // method
    lua_newtable( L );
    lstate_fn2tbl( L, "load", load_lua );
    // constants
    lstate_num2tbl( L, "ALIGN_LEFT", IMG_ALIGN_LEFT );
    lstate_num2tbl( L, "ALIGN_CENTER", IMG_ALIGN_CENTER );
    lstate_num2tbl( L, "ALIGN_RIGHT", IMG_ALIGN_RIGHT );
    lstate_num2tbl( L, "ALIGN_TOP", IMG_ALIGN_TOP );
    lstate_num2tbl( L, "ALIGN_MIDDLE", IMG_ALIGN_MIDDLE );
    lstate_num2tbl( L, "ALIGN_BOTTOM", IMG_ALIGN_BOTTOM );
    
    return 1;
}
