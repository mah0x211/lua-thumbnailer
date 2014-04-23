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
    DATA32 *img;
    img_size_t size;
    img_size_t resize;
    uint8_t quality;
} context_t;


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


static inline void save2path( const char *path, uint8_t quality, ImlibLoadError *err )
{
    // set quality
    imlib_image_attach_data_value( "quality", NULL, quality, NULL );
    imlib_save_image_with_error_return( path, err );
    imlib_free_image();
}


static int save_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    const char *path = luaL_checkstring( L, 2 );
    ImlibLoadError err = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image work = imlib_create_image_using_data( ctx->size.w, ctx->size.h,
                                                      ctx->img );
    
    // set current image
    imlib_context_set_image( work );
    work = imlib_create_cropped_scaled_image( 0, 0, ctx->size.w, ctx->size.h, 
                                              ctx->resize.w, ctx->resize.h );
    imlib_free_image();
    imlib_context_set_image( work );
    save2path( path, ctx->quality, &err );
    // failed
    if( err ){
        liberr2errno( err );
        lua_pushboolean( L, 0 );
        lua_pushstring( L, strerror(errno) );
        return 1;
    }
    
    // success
    lua_pushboolean( L, 1 );
    
    return 0;
}


static int rawsize_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    lua_pushinteger( L, ctx->size.w );
    lua_pushinteger( L, ctx->size.h );
    
    return 2;
}


static int size_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    
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
        
        ctx->resize.w = width;
        ctx->resize.h = height;
    }
    
    lua_pushinteger( L, ctx->resize.w );
    lua_pushinteger( L, ctx->resize.h );
    
    return 2;
}


static int quality_lua( lua_State *L )
{
    context_t *ctx = (context_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    if( !lua_isnoneornil( L, 2 ) ){
        int quality = luaL_checkint( L, 2 );
        SETVAL_IN_RANGE( ctx->quality, uint8_t, quality, 0, 100 );
    }
    
    lua_pushinteger( L, ctx->quality );
    
    return 1;
}


static int dealloc_gc( lua_State *L )
{
    context_t *ctx = (context_t*)lua_touserdata( L, 1 );
    
    if( ctx->img ){
        free( ctx->img );
        ctx->img = NULL;
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
        Imlib_Image img = imlib_load_image_with_error_return( path, &err );
        
        if( img )
        {
            size_t bytes = sizeof( DATA32 );
            
            imlib_context_set_image( img );
            ctx->size.w =  imlib_image_get_width();
            ctx->size.h = imlib_image_get_height();
            // allocate buffer
            bytes *= (size_t)ctx->size.w * (size_t)ctx->size.h;
            ctx->img = malloc( bytes );
            if( ctx->img ){
                memcpy( ctx->img, imlib_image_get_data_for_reading_only(), 
                        bytes );
                imlib_free_image_and_decache();
                
                ctx->quality = 100;
                ctx->resize = (img_size_t){ 0, 0 };
                
                // set metatable
                luaL_getmetatable( L, MODULE_MT );
                lua_setmetatable( L, -2 );
                
                return 1;
            }
            
            imlib_free_image_and_decache();
        }
        else {
            liberr2errno( err );
        }
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
        { "rawsize", rawsize_lua },
        { "size", size_lua },
        { "quality", quality_lua },
        { "save", save_lua },
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
