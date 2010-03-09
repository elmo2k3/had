

/*
 *
 * Comic_8
 *
 * created with FontCreator
 * written by F. Maximilian Thiele
 *
 * http://www.apetech.de/fontCreator
 * me@apetech.de
 *
 * File Name           : Comic_8.h
 * Date                : 19.02.2008
 * Font size in bytes  : 3297
 * Font width          : 10
 * Font height         : 9
 * Font first char     : 32
 * Font last char      : 128
 * Font used chars     : 96
 *
 * The font data are defined as
 *
 * struct _FONT_ {
 *     uint16_t   font_Size_in_Bytes_over_all_included_Size_it_self;
 *     uint8_t    font_Width_in_Pixel_for_fixed_drawing;
 *     uint8_t    font_Height_in_Pixel_for_all_characters;
 *     unit8_t    font_First_Char;
 *     uint8_t    font_Char_Count;
 *
 *     uint8_t    font_Char_Widths[font_Last_Char - font_First_Char +1];
 *                  // for each character the separate width in pixels,
 *                  // characters < 128 have an implicit virtual right empty row
 *
 *     uint8_t    font_data[];
 *                  // bit field of all characters
 */

#include <inttypes.h>

#ifndef COMIC_8_H
#define COMIC_8_H

#define COMIC_8_WIDTH 10
#define COMIC_8_HEIGHT 9

static uint8_t Comic_8[] = {
    0x0C, 0xE1, // size
    0x0A, // width
    0x09, // height
    0x20, // first char
    0x60, // char count
    
    // char widths
    0x00, 0x01, 0x03, 0x06, 0x05, 0x05, 0x05, 0x01, 0x03, 0x03, 
    0x03, 0x04, 0x01, 0x03, 0x00, 0x04, 0x05, 0x02, 0x03, 0x03, 
    0x04, 0x04, 0x04, 0x05, 0x04, 0x04, 0x01, 0x02, 0x02, 0x03, 
    0x02, 0x04, 0x07, 0x04, 0x04, 0x05, 0x04, 0x04, 0x04, 0x05, 
    0x05, 0x04, 0x05, 0x04, 0x04, 0x07, 0x06, 0x06, 0x04, 0x07, 
    0x05, 0x04, 0x05, 0x04, 0x04, 0x07, 0x05, 0x05, 0x05, 0x02, 
    0x03, 0x02, 0x03, 0x05, 0x01, 0x04, 0x03, 0x04, 0x04, 0x04, 
    0x04, 0x04, 0x03, 0x01, 0x03, 0x03, 0x01, 0x06, 0x04, 0x04, 
    0x04, 0x04, 0x03, 0x04, 0x03, 0x04, 0x04, 0x05, 0x04, 0x04, 
    0x03, 0x02, 0x01, 0x02, 0x04, 0x04, 
    
    // font data
    0x7C, 0x00, // 33
    0x06, 0x00, 0x06, 0x00, 0x00, 0x00, // 34
    0x74, 0x2E, 0x24, 0x3C, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 35
    0x40, 0x4C, 0xFF, 0x52, 0x30, 0x00, 0x00, 0x80, 0x00, 0x00, // 36
    0x06, 0x36, 0x2C, 0x62, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, // 37
    0x30, 0x50, 0x5E, 0x66, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, // 38
    0x06, 0x00, // 39
    0x78, 0x86, 0x00, 0x00, 0x00, 0x80, // 40
    0x00, 0x86, 0x78, 0x80, 0x00, 0x00, // 41
    0x02, 0x04, 0x0C, 0x00, 0x00, 0x00, // 42
    0x10, 0x38, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, // 43
    0xC0, 0x00, // 44
    0x20, 0x20, 0x20, 0x00, 0x00, 0x00, // 45
    0x40, 0x30, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, // 47
    0x3C, 0x42, 0x42, 0x66, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, // 48
    0x7E, 0x40, 0x00, 0x00, // 49
    0x72, 0x52, 0x4E, 0x00, 0x00, 0x00, // 50
    0x42, 0x4A, 0x7E, 0x00, 0x00, 0x00, // 51
    0x10, 0x18, 0x26, 0x7E, 0x00, 0x00, 0x00, 0x00, // 52
    0x04, 0x4A, 0x4A, 0x7A, 0x00, 0x00, 0x00, 0x00, // 53
    0x38, 0x54, 0x4A, 0x70, 0x00, 0x00, 0x00, 0x00, // 54
    0x02, 0x62, 0x1A, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, // 55
    0x24, 0x5A, 0x4A, 0x7E, 0x00, 0x00, 0x00, 0x00, // 56
    0x0C, 0x52, 0x52, 0x3E, 0x00, 0x00, 0x00, 0x00, // 57
    0x20, 0x00, // 58
    0x80, 0x48, 0x00, 0x00, // 59
    0x10, 0x28, 0x00, 0x00, // 60
    0x28, 0x28, 0x28, 0x00, 0x00, 0x00, // 61
    0x28, 0x10, 0x00, 0x00, // 62
    0x04, 0x62, 0x12, 0x0C, 0x00, 0x00, 0x00, 0x00, // 63
    0x18, 0x66, 0x5A, 0x9E, 0x92, 0x62, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 64
    0x30, 0x18, 0x1E, 0x70, 0x00, 0x00, 0x00, 0x00, // 65
    0x7E, 0x52, 0x5E, 0x20, 0x00, 0x00, 0x00, 0x00, // 66
    0x30, 0x4C, 0x42, 0x42, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, // 67
    0x7E, 0x42, 0x44, 0x78, 0x00, 0x00, 0x00, 0x00, // 68
    0x7E, 0x52, 0x0A, 0x02, 0x00, 0x00, 0x00, 0x00, // 69
    0x3E, 0x0A, 0x0A, 0x02, 0x00, 0x00, 0x00, 0x00, // 70
    0x38, 0x44, 0x52, 0x52, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, // 71
    0x3E, 0x10, 0x10, 0x10, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, // 72
    0x42, 0x7E, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00, // 73
    0x20, 0x42, 0x42, 0x3E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, // 74
    0x7E, 0x28, 0x44, 0x42, 0x00, 0x00, 0x00, 0x00, // 75
    0x7C, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, // 76
    0x40, 0x3C, 0x1E, 0x60, 0x1C, 0x3E, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 77
    0x3A, 0x06, 0x08, 0x10, 0x20, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 78
    0x30, 0x4C, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 79
    0x7C, 0x12, 0x12, 0x0C, 0x00, 0x00, 0x00, 0x00, // 80
    0x38, 0x44, 0x42, 0xA2, 0x42, 0xEC, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, // 81
    0x3C, 0x12, 0x32, 0x3C, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, // 82
    0x4C, 0x52, 0x52, 0x72, 0x00, 0x00, 0x00, 0x00, // 83
    0x02, 0x02, 0x3E, 0x42, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, // 84
    0x7C, 0x40, 0x40, 0x7C, 0x00, 0x00, 0x00, 0x00, // 85
    0x3C, 0x40, 0x38, 0x06, 0x00, 0x00, 0x00, 0x00, // 86
    0x1C, 0x60, 0x18, 0x1E, 0x60, 0x38, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 87
    0x40, 0x66, 0x18, 0x38, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, // 88
    0x02, 0x0C, 0x70, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, // 89
    0x42, 0x62, 0x5A, 0x46, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, // 90
    0xFE, 0x02, 0x80, 0x80, // 91
    0x0E, 0x30, 0xC0, 0x00, 0x00, 0x00, // 92
    0x02, 0xFE, 0x80, 0x80, // 93
    0x02, 0x02, 0x04, 0x00, 0x00, 0x00, // 94
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, // 95
    0x02, 0x00, // 96
    0x70, 0x48, 0x48, 0x78, 0x00, 0x00, 0x00, 0x00, // 97
    0x7C, 0x48, 0x78, 0x00, 0x00, 0x00, // 98
    0x30, 0x48, 0x48, 0x48, 0x00, 0x00, 0x00, 0x00, // 99
    0x30, 0x48, 0x48, 0x7C, 0x00, 0x00, 0x00, 0x00, // 100
    0x30, 0x68, 0x58, 0x58, 0x00, 0x00, 0x00, 0x00, // 101
    0x08, 0xFC, 0x0A, 0x08, 0x00, 0x00, 0x00, 0x00, // 102
    0x30, 0x48, 0x48, 0xF8, 0x80, 0x80, 0x80, 0x00, // 103
    0x3C, 0x08, 0x78, 0x00, 0x00, 0x00, // 104
    0x7A, 0x00, // 105
    0x00, 0x32, 0xC0, 0x80, 0x80, 0x00, // 106
    0x3E, 0x30, 0x48, 0x00, 0x00, 0x00, // 107
    0x3E, 0x00, // 108
    0x68, 0x10, 0x38, 0x48, 0x08, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 109
    0x20, 0x10, 0x08, 0x78, 0x00, 0x00, 0x00, 0x00, // 110
    0x30, 0x48, 0x48, 0x30, 0x00, 0x00, 0x00, 0x00, // 111
    0xB8, 0x48, 0x48, 0x78, 0x00, 0x00, 0x00, 0x00, // 112
    0x30, 0x48, 0xC8, 0x38, 0x00, 0x00, 0x00, 0x00, // 113
    0x38, 0x08, 0x08, 0x00, 0x00, 0x00, // 114
    0x40, 0x58, 0x68, 0x08, 0x00, 0x00, 0x00, 0x00, // 115
    0x08, 0x3C, 0x08, 0x00, 0x00, 0x00, // 116
    0x38, 0x40, 0x40, 0x78, 0x00, 0x00, 0x00, 0x00, // 117
    0x18, 0x60, 0x30, 0x08, 0x00, 0x00, 0x00, 0x00, // 118
    0x38, 0x60, 0x18, 0x60, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, // 119
    0x40, 0x28, 0x30, 0x48, 0x00, 0x00, 0x00, 0x00, // 120
    0x08, 0xB0, 0x70, 0x08, 0x00, 0x80, 0x00, 0x00, // 121
    0x48, 0x78, 0x48, 0x00, 0x00, 0x00, // 122
    0x70, 0x8E, 0x00, 0x00, // 123
    0xFF, 0x00, // 124
    0xFE, 0x10, 0x00, 0x00, // 125
    0x10, 0x08, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, // 126
    0x7E, 0x42, 0x42, 0x7E, 0x00, 0x00, 0x00, 0x00 // 127
    
};

#endif