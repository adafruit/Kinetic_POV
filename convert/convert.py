# Image converter script for POV LED poi project.  Reads 16-color GIF or
# PNG image as input, generates tables which can be copied-and-pasted or
# redirected to a .h file, e.g.:
#
# $ python convert.py image.gif > graphics.h
#
# Ideal image dimensions are determined by hardware setup, e.g. LED poi
# project uses 16 LEDs, so image height should match.  Width is limited
# by AVR PROGMEM capacity; ~2.7K on Trinket allows ~340 columns max.

from PIL import Image
import sys

# Establish peak and average current limits - a function of battery
# capacity and desired run time.  LED poi project uses a 150 mAh cell,
# so average current should be kept at or below 1C rate (150 mA), though
# brief surges are OK.  Project uses two LED strips in parallel, so actual
# current is 2X these numbers, plus some overhead (~20 mA) for the MCU.
peakC = 180.0  # 180 + 180 + 20 = 380 mA = ~2.5C peak
avgC  =  60.0  #  60 +  60 + 20 = 140 mA = ~0.9C average

bR    = 1.0  # Adjust color
bG    = 1.0  # balance for
bB    = 1.0  # whiter whites!
gamma = 2.6  # For more linear-ish perceived brightness

# Current estimates are averages measured from strip on fresh LiPoly
# TO DO: like, actually take these actual measurements :/
mA0   =  1.0      # LED current when off (driver logic still needs some)
mAR   = 16.0 * bR # + current for 100% red
mAG   = 16.0 * bG # + current for 100% green
mAB   = 16.0 * bB # + current for 100% blue

# --------------------------------------------------------------------------

img    = Image.open(sys.argv[1]) # Open image name passed to script
pixels = img.load()
# Must be palette mode (GIF or PNG) with 16 colors or fewer:
assert img.mode == 'P' and len(img.getcolors()) <= 16

# Image size not validated - on purpose - in case of quick test with an
# existing (but non-optimal) file.  If too big or too small for the LED
# strip, just wastes some PROGMEM space or some LEDs will be lit wrong.
width  = img.size[0]
height = img.size[1] & 0xFFFE  # Pixels/line must be multiple of 2

# Shenanigans to extract color palette from image:
lut = img.resize((16, 1)) # Create new 16x1 image using same palette
lut.putdata(range(16))    # Fill new image pixels with 0-15 (LUT indices)
lut = list(lut.convert("RGB").getdata()) # pixels->LUT->list

# Estimate current for each element of palette:
paletteCurrent = []
for i in range(16):
	paletteCurrent.append(mA0 +
	     pow((lut[i][0] / 255.0), gamma) * mAR +
	     pow((lut[i][1] / 255.0), gamma) * mAG +
	     pow((lut[i][2] / 255.0), gamma) * mAB)

# Estimate peak and average current for each column of image
colMaxC = 0.0  # Maximum column current
colAvgC = 0.0  # Average column current
for x in range(width): # For each row...
	mA = 0.0       # Sum current of each pixel's palette entry
	for y in range(height): mA += paletteCurrent[pixels[x, y]]
	colAvgC += mA                 # Accumulate average (div later)
	if mA > colMaxC: colMaxC = mA # Monitor peak
colAvgC /= width       # Sum div into average

s1 = peakC / colMaxC   # Scaling factor for peak current constraint
s2 = avgC  / colAvgC   # Scaling factor for average current constraint
if s2 < s1:  s1 = s2   # Use smaller of two (so both constraints met),
if s1 > 1.0: s1 = 1.0  # but never increase brightness

print "// Don't edit this file!  It's software-generated."
print "// See convert.py script instead."
print
print '#define NUM_LEDS ' + str(height)
print '#define LINES    ' + str(width)
print

# Output gamma- and brightness-adjusted color palette:
s1 *= 255.0 # (0.0-1.0) -> (0.0-255.0)
bR *= s1    # Scale color balance values
bG *= s1
bB *= s1
print 'uint8_t palette[16][3] = {'
for i in range(16):
	sys.stdout.write('  { %*s, %*s, %*s }' %
	  (3, str(int(pow((lut[i][0] / 255.0), gamma) * bR + 0.5)),
	   3, str(int(pow((lut[i][1] / 255.0), gamma) * bG + 0.5)),
	   3, str(int(pow((lut[i][2] / 255.0), gamma) * bB + 0.5))))
	if i < 15: print ','
print ' };'
print

# Pixel values are just used directly (packed 2 per byte):
sys.stdout.write('const uint8_t PROGMEM pixels[] = {')
i    = 0  # Current pixel number in input
cols = 7  # Current column number in output
for x in range(width):
	for y in range(0, height, 2):
		cols += 1                      # Increment column #
		if cols >= 8:                  # If max column exceeded...
			print                  # end current line
			sys.stdout.write('  ') # and start new one
			cols = 0               # Reset counter
		p1 = pixels[x, y]              # Even pixel value
		p2 = pixels[x, y + 1]          # Odd pixel value
		sys.stdout.write('0x')
		if p1 < 10: sys.stdout.write(chr(ord('0') + p1))
		else      : sys.stdout.write(chr(ord('A') + p1 - 10))
		if p2 < 10: sys.stdout.write(chr(ord('0') + p2))
		else      : sys.stdout.write(chr(ord('A') + p2 - 10))
		i += 2
		if i < (width * height):
			sys.stdout.write(',')
			if cols < 7: sys.stdout.write(' ')
print ' };'
