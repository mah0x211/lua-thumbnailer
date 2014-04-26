lua-thumbnailer
===============

thumbnail image generator.

## Dependencies

- Imlib2

## Constants

these constants defined at the `thumbnail.*`

**for parameter of saveCrop/saveAspect methods**

- LEFT
- CENTER
- RIGHT
- TOP
- MIDDLE
- BOTTOM

## Create Image Object

these function create the image object.

### image, err = thumbnailer.load( filepath )

**Parameters**

- filepath: path string to image file.

**Returns**

1. image: image object.
2. err: error string on failure.

### image, err = thumbnailer.read( width, height, rawdata )

raw data value must be 32-bit per pixel.

**Parameters**

- width: image width.
- height: image height.
- rawdata: image raw data (light userdata).

**Returns**

1. image: image object.
2. err: error string on failure.


## Accessing Raw Data

these method returns immutable values.

### rawdata, bytes = image:raw()

**Returns**

1. rawdata: image raw data (light userdata).
2. bytes: data size.


### width, height = image:rawsize()

**Returns**

1. width: image width.
2. height: image height.


## Deallocate Memory of Raw Data immediately.

this method will deallocate memory of rawdata immediately.  

### image:free()

after calling this method, the image object can no longer be used.


## Export Options

following methods are returned values of current export option.  
you could change these values by passing the arguments of these value.


### width, height = image:size( [width, height] )

following parameters must be larger than 0.

**Parameters**

- width: image width
- height: image height

**Returns**

1. width: image width
2. height: image height


### quality = image:quality( [quality] )

following parameters must be range of 0 to 100.

**Parameters**

- quality: image quality.

**Returns**

1. quality: image quality.

### format = image:format( [format] )

following parameter must be image format string supported by imlib2 library.  
e.g. `jpg`, `png`

Note: it is set 'png' by default if the image objects created by thumbnail.read function.

**Parameters**

- format: image format string.

**Returns**

1. format: image format string.


## Export Image

### err = image:save( path )

save stretched image.

**Parameters**

- path: destination path of the image.

**Returns**

1. err: nil on success, or error string on failure.


### err = image:saveTrim( path )

save image with maintain aspect ratio after cutting margin.

**Parameters**

- path: destination path of the image.

**Returns**

1. err: nil on success, or error string on failure.


### err = image:saveCrop( path[, halign[, valign]] )

save cropped image with maintain aspect ratio.

**Parameters**

- path: destination path of the image.
- halign: horizontal alignment. (default: CENTER)
- valign: vertical alignment. (default: MIDDLE)

**Returns**

1. err: nil on success, or error string on failure.


### err = image:saveAspect( path[, h[, s[, l[, a[, halign[, valign]]]]]] )

save image with maintain aspect ratio.

**Parameters**

- path: destination path of the image.
- h: hue of background color. (0-360)
- s: saturation of background color. (0-1)
- l: lightness of background color. (0-1)
- a: alpha of background color. (0-255)
- halign: horizontal alignment. (default: CENTER)
- valign: vertical alignment. (default: MIDDLE)

**Returns**

1. err: nil on success, or error string on failure.


