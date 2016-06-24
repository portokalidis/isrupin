extern "C" {
#include <stdint.h>
#include <byteswap.h>
}

#include "image.hpp"

Image::Image(unsigned long low_addr, unsigned long high_addr)
{
	this->low_addr = low_addr;
	this->high_addr = high_addr;
	this->key = 0;
	this->encrypted = false;
}

