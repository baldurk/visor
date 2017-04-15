#pragma once

extern "C" {
void DecompressBlockBC1(uint32_t x, uint32_t y, uint32_t stride, const uint8_t *blockStorage,
                        unsigned char *image);
void DecompressBlockBC3(uint32_t x, uint32_t y, uint32_t stride, const uint8_t *blockStorage,
                        unsigned char *image);
void DecompressBlockBC2(uint32_t x, uint32_t y, uint32_t stride, const uint8_t *blockStorage,
                        unsigned char *image);
void DecompressBlockBC4(uint32_t x, uint32_t y, uint32_t stride, enum BC4Mode mode,
                        const uint8_t *blockStorage, unsigned char *image);
void DecompressBlockBC5(uint32_t x, uint32_t y, uint32_t stride, enum BC5Mode mode,
                        const uint8_t *blockStorage, unsigned char *image);
};