#!/bin/sh
# mkcorpus.sh DIR [large]: ordinary images made with ImageMagick and cjpeg from
# ImageMagick's built-in images, for "imagebounds dump" before/after a change:
# PNG (palette, gray 1-16 bit, alpha, tRNS, interlaced), JPEG (baseline,
# progressive, gray, CMYK), BMP (1/4/8 bpp, RLE, 16/24/32 bpp), animated GIFs
# (optimized frames at offsets, disposals, interlaced, transparency), MNG.
# "large" adds images near the readers' limits: a 12000x8000 JPEG (384 MB as
# 32 bpp), a 16384x16384 gray PNG (256 MiB) and a 16000x16000 RGB PNG (1 GB).
set -e
d=$1; mkdir -p "$d"
m() { magick "$@"; }
m rose: -resize 160x120 "$d/rose.ppm"
m logo: -resize 25% "$d/logo.ppm"
m -seed 7 -size 97x61 plasma: "$d/plasma.ppm"
for s in rose logo plasma; do
  i="$d/$s.ppm"
  m "$i" -colors 200 PNG8:"$d/$s-pal8.png"
  m "$i" -colors 13 -depth 4 PNG8:"$d/$s-pal4.png"
  m "$i" -colors 2 -depth 1 -type palette PNG8:"$d/$s-pal1.png"
  m "$i" -colorspace gray -depth 8 PNG8:"$d/$s-gray8.png" 2>/dev/null || true
  m "$i" -colorspace gray -depth 16 PNG48:"$d/$s-gray16.png" 2>/dev/null || true
  m "$i" -colorspace gray -depth 2 -define png:bit-depth=2 -define png:color-type=0 "$d/$s-gray2.png"
  m "$i" -colorspace gray -threshold 50% -define png:bit-depth=1 -define png:color-type=0 "$d/$s-gray1.png"
  m "$i" -alpha set -channel A -evaluate set 60% +channel PNG32:"$d/$s-rgba.png"
  m "$i" -depth 16 PNG48:"$d/$s-rgb16.png"
  m "$i" -transparent "rgb(255,255,255)" PNG24:"$d/$s-trns.png" 2>/dev/null || true
  m "$i" -interlace PNG PNG24:"$d/$s-interlaced.png"
  m "$i" -colorspace gray -alpha set -channel A -evaluate set 50% +channel -define png:color-type=4 "$d/$s-graya.png"
  cjpeg -quality 85 "$i" > "$d/$s-base.jpg"
  cjpeg -progressive "$i" > "$d/$s-prog.jpg"
  cjpeg -grayscale "$i" > "$d/$s-gray.jpg"
  m "$i" -colorspace CMYK "$d/$s-cmyk.jpg"
  m "$i" -type palette -colors 2 BMP3:"$d/$s-1.bmp"
  m "$i" -colors 16 -compress none BMP3:"$d/$s-4.bmp" 2>/dev/null || true
  m "$i" -colors 16 -compress RLE BMP3:"$d/$s-rle4.bmp" 2>/dev/null || true
  m "$i" -colors 180 -compress none BMP3:"$d/$s-8.bmp"
  m "$i" -colors 180 -compress RLE BMP3:"$d/$s-rle8.bmp"
  m "$i" -define bmp:subtype=RGB565 BMP:"$d/$s-16.bmp" 2>/dev/null || true
  m "$i" BMP3:"$d/$s-24.bmp"
  m "$i" -alpha set -channel A -evaluate set 70% +channel BMP:"$d/$s-32a.bmp"
  m "$i" -compress none PGM:"$d/$s.pgm"
  m "$i" -compress none -colorspace gray -depth 4 PGM:"$d/$s-d4.pgm"
  m "$i" -threshold 50% PBM:"$d/$s.pbm"
  m "$i" -resize 40x40 XPM:"$d/$s.xpm"
  m "$i" -resize 40x40 -threshold 50% XBM:"$d/$s.xbm"
  m "$i" GIF:"$d/$s.gif"
  m "$i" -interlace GIF GIF:"$d/$s-interlaced.gif"
done
# animations: frames change in a part only, optimised to frames at offsets
r="$d/rose.ppm"
m "$r" \( "$r" -fill red -draw "rectangle 20,20 60,50" \) \( "$r" -fill blue -draw "circle 100,60 110,70" \) \
  \( "$r" -fill yellow -draw "rectangle 140,100 159,119" \) -set delay 10 -loop 0 -layers Optimize "$d/anim-opt.gif"
m "$r" \( "$r" -fill red -draw "rectangle 20,20 60,50" \) \( "$r" -fill blue -draw "circle 100,60 110,70" \) \
  -set delay 7 -set dispose Background -layers OptimizeFrame "$d/anim-bg.gif"
m "$r" \( "$r" -fill red -draw "rectangle 20,20 60,50" \) \( "$r" -fill blue -draw "circle 100,60 110,70" \) \
  -set delay 7 -set dispose Previous -layers OptimizePlus "$d/anim-prev.gif"
m -size 80x60 xc:none \( -size 20x20 xc:red -repage +5+5 \) \( -size 30x10 xc:blue -repage +40+30 \) \
  \( -size 10x40 xc:green -repage +65+15 \) -set dispose None -set delay 20 -loop 0 "$d/anim-transparent.gif"
m -size 80x60 xc:white \( -size 20x20 xc:red -repage +70+50 \) -set dispose Previous "$d/anim-partly-outside.gif"
m "$r" \( "$r" -fill red -draw "rectangle 20,20 60,50" \) -set delay 10 -layers Optimize -interlace GIF "$d/anim-interlaced.gif"
m "$r" \( "$r" -negate \) \( "$r" -flip \) -set delay 10 "$d/anim.mng" 2>/dev/null || true
m "$r" "$d/rose.mng" 2>/dev/null || true
if [ "$2" = large ]; then
  m -seed 3 -size 600x400 plasma: -resize 12000x8000! ppm:- | cjpeg -quality 70 > "$d/large-12000x8000.jpg"
  m -size 16384x16384 gradient:black-white -depth 8 -define png:color-type=0 -define png:compression-level=1 "$d/large-16384-gray.png"
  m -size 16000x16000 gradient:red-blue -depth 8 -define png:compression-level=1 PNG24:"$d/large-16000-rgb.png"
fi
ls "$d" | wc -l
