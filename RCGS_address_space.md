RxLCDE

0000 - 00FF - Zero page
	00E0 - 00FF - String buffer
0100 - 01FF - Hardware stack
0200 - 3FFF - Variables
4000 - 7FFF - LCD
8000 - FFFF - Cartridge

RxRCGS

0000 - 00FF - Zero page
	0000 - 0001 - Indirect address buffer
	0002 - Math library buffer
	0003 - Double sleep buffer
0100 - 01FF - Hardware stack
0200 - 3FFF - Variables
	0200 - 0209 - Controller buffers
4000 - 7FFF - RailView frame buffer x2
8000 - BFFF - Controller register repeated
C000 - FFFF - Cartridge

