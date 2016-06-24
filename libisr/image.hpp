#ifndef IMAGE_HPP
#define IMAGE_HPP

class Image {
public:
	unsigned long low_addr;
	unsigned long high_addr;
	uint16_t key;

	Image(unsigned long low_addr, unsigned long high_addr);

	inline void SetKey(uint16_t key) {
		this->key = key;
		this->encrypted = true;
	}

	inline uint16_t Decode(uint16_t data) {
		return (data ^ this->key);
	}

	inline void DecodeBuf(unsigned long addr, uint8_t *buf, unsigned len) {
		uint16_t key;

		key = (addr & 0x1)? bswap_16(this->key) : this->key;

		while (len >= 2) {
			*(uint16_t *)buf = *(uint16_t *)buf ^ key;
			buf += 2;
			len -= 2;
		}
		if (len > 0)
			*buf = *buf ^ (uint8_t)key;
	}

	inline uint32_t DecodeLong(unsigned long addr, uint32_t data) {
		uint16_t key;

		key = (addr & 0x1)? bswap_16(this->key) : this->key;
		return ((data & 0xffff) ^ key) | 
			((data & ~0xffff) ^ (key << 16));
	}

	inline bool IsEncrypted(void) { return encrypted; };

private:
	bool encrypted;

};

#endif
