local thumbnailer = require('thumbnailer');
local img, err = thumbnailer.load( './image.jpg' );

if err then
    print( err );
else
    img:size( 200, 200 );
    img:quality( 100 );
    img:format( 'png' );
    -- stretch
    img:save( './fit.png' );
    -- trim
    img:saveTrim( './trim.png' );
    -- crop
    img:saveCrop( './crop-lt.png', thumbnailer.LEFT, thumbnailer.TOP );
    img:saveCrop( './crop-lm.png', thumbnailer.LEFT, thumbnailer.MIDDLE );
    img:saveCrop( './crop-lb.png', thumbnailer.LEFT, thumbnailer.BOTTOM );
    img:saveCrop( './crop-rt.png', thumbnailer.RIGHT, thumbnailer.TOP );
    img:saveCrop( './crop-rm.png', thumbnailer.RIGHT, thumbnailer.MIDDLE );
    img:saveCrop( './crop-rb.png', thumbnailer.RIGHT, thumbnailer.BOTTOM );
    img:saveCrop( './crop-ct.png', thumbnailer.CENTER, thumbnailer.TOP );
    img:saveCrop( './crop-cm.png' );
    img:saveCrop( './crop-cb.png', thumbnailer.CENTER, thumbnailer.BOTTOM );
    -- aspect
    img:saveAspect( './aspect-lt.png', 0, 1, 1, 255, thumbnailer.LEFT, 
                    thumbnailer.TOP );
    img:saveAspect( './aspect-lm.png', 0, 1, 1, 255, thumbnailer.LEFT, 
                    thumbnailer.MIDDLE );
    img:saveAspect( './aspect-lb.png', 0, 1, 1, 255, thumbnailer.LEFT, 
                    thumbnailer.BOTTOM );
    img:saveAspect( './aspect-rt.png', 0, 1, 1, 255, thumbnailer.RIGHT, 
                    thumbnailer.TOP );
    img:saveAspect( './aspect-rm.png', 0, 1, 1, 255, thumbnailer.RIGHT, 
                    thumbnailer.MIDDLE );
    img:saveAspect( './aspect-rb.png', 0, 1, 1, 255, thumbnailer.RIGHT, 
                    thumbnailer.BOTTOM );
    img:saveAspect( './aspect-ct.png', 0, 1, 1, 255, thumbnailer.CENTER, 
                    thumbnailer.TOP );
    img:saveAspect( './aspect-cm.png', 0, 1, 1, 255 );
    img:saveAspect( './aspect-cb.png', 0, 1, 1, 255, thumbnailer.CENTER, 
                    thumbnailer.BOTTOM );
    -- free
    img:free();
end
