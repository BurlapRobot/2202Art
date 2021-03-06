#!/usr/bin/python

import RPi.GPIO as GPIO, time
from PIL import Image

# Configurable values
filename  = "all_sprites_flat.png"
dev       = "/dev/spidev0.0"
boardWidth=19
boardHeight=13
numPixels = 248

# Open SPI device, load image in RGB format and get dimensions:
spidev    = file(dev, "wb")
print "Loading..."
img       = Image.open(filename).convert("RGB")
width     = img.size[0]
height    = img.size[1]
print "%dx%d pixels" % img.size

# print "Resizing to %dx%d pixels" % (boardWidth, boardHeight)
# img = img.resize((boardWidth, boardHeight))

pixels    = img.load()



# R, G, B byte per pixel, plus extra '0' byte at end for latch.
spiBytes = bytearray(numPixels * 3 + 1)

# For testing, solid red
#for pixel in range(numPixels):
#	spiBytes[pixel*3 + 0] = 0
#	spiBytes[pixel*3 + 1] = 255
#	spiBytes[pixel*3 + 2] = 0


def getIndex(x, y):
	"""
	The strand starts at the bottom right and snakes up and to the 
	left, like so (assuming width of 4):

	  ^  11---10---09---08
	  |  04---05---06---07
	  y  03---02---01---00
	   x --->
	 
	 So, y*width will tell us the minimum index it could be.  Since
	 the rows alternate ordering, if the row is even, the additional
	 offset is width-x-1.
	"""
	index = y * boardWidth + 1;
	if (y%2==1):
	  index += x
	else:
	  index += (boardWidth-x-1)

	return index

def setIndexColor(i, r, g, b):
	spiBytes[i*3 + 0] = r
	spiBytes[i*3 + 1] = g
	spiBytes[i*3 + 2] = b

def setPixelColor(x, y, r, g, b):
	i = getIndex(x, y)
	setIndexColor(i, r, g, b)

img_offset = 0
while(1):
	time.sleep(.5)
	for x in range(boardWidth):
		for y in range(boardHeight):
			ym = (y + img_offset) % height
			try:
				setPixelColor(x, y, pixels[x, ym][0], pixels[x, ym][1], pixels[x, ym][2] )
			except IndexError:
				setPixelColor(x, y, 0, 0, 0)

	# Display the pixels
	spiBytes[numPixels] = 0 # Make sure latch is set
	spidev.write(spiBytes)
	spidev.flush()
	img_offset += 1
	if img_offset >= height:
		img_offset = 0
