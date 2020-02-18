//
// Created by Ben Hencke on 11/18/18.
//

#include "PBDriverAdapter.hpp"

typedef struct {
    int8_t magic[4];
    uint8_t channel;
    uint8_t recordType; //set channel ws2812 opts+data, draw all
} PBFrameHeader;

typedef struct {
    uint8_t numElements; //0 to disable channel, usually 3 (RGB) or 4 (RGBW)
    union {
        struct {
            uint8_t redi :2, greeni :2, bluei :2, whitei :2; //color orders, data on the line assumed to be RGB or RGBW
        };
        uint8_t colorOrders;
    };
    uint16_t pixels;
} PBWS2812Channel;

typedef struct {
    uint32_t frequency;
    union {
        struct {
            uint8_t redi :2, greeni :2, bluei :2; //color orders, data on the line assumed to be RGB
        };
        uint8_t colorOrders;
    };
    uint16_t pixels;
} PBAPA102DataChannel;

typedef struct {
    uint32_t frequency;
} PBAPA102ClockChannel;




static const uint32_t crc_table[16] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};


uint32_t crc_update(uint32_t crc, const void *data, size_t data_len) {
    const unsigned char *d = (const unsigned char *) data;
    unsigned int tbl_idx;

    while (data_len--) {
        tbl_idx = crc ^ *d;
        crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);
        tbl_idx = crc ^ (*d >> 4);
        crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);
        d++;
    }
    return crc & 0xffffffff;
}


void PBDriverAdapter::begin(uint32_t uartFrequency) {
    Serial1.flush();
    Serial1.begin(uartFrequency, SERIAL_8N1, SERIAL_TX_ONLY);
    timer = micros();
}

void PBDriverAdapter::end() {
    Serial1.end();
}

void PBDriverAdapter::show(uint16_t numPixels, std::function<void(uint16_t index, uint8_t rgbw[])> renderCallback,
                           std::function<void(PBChannel *)> channelSwitchCallback) {
    int curPixel = 0;
    union {
        uint32_t rgbFrame;
        uint8_t rgb[4];
    };
    union {
        uint32_t rgbFrameInit;
        uint8_t rgbFrameInitBytes[4];
    };

    while (micros() - timer < 300)
        yield();

    if (!channels)
        return;
    memset(rgb, 0, 4);

    PBFrameHeader frameHeader;
    memcpy_P(frameHeader.magic, F("UPXL"), 4);

    int total = 0;
    for (auto channel : *channels) {
        if (!channel.numElements)
            return;

        uint32_t crc = 0xffffffff;
        frameHeader.channel = channel.channelId;
        frameHeader.recordType = channel.channelType;
        Serial1.write((uint8_t *) &frameHeader, sizeof(frameHeader));
        crc = crc_update(crc, &frameHeader, sizeof(frameHeader));

        switch (channel.channelType) {
            case CHANNEL_WS2812: {
                //write the channel struct
                PBWS2812Channel pbws2812Channel;
                pbws2812Channel.numElements = channel.numElements;
                pbws2812Channel.pixels = channel.pixels;
                pbws2812Channel.colorOrders = channel.colorOrders;
                Serial1.write((uint8_t *) &pbws2812Channel, sizeof(pbws2812Channel));
                crc = crc_update(crc, &pbws2812Channel, sizeof(pbws2812Channel));
                channelSwitchCallback(&channel);
                rgbFrameInit = 0; //default to black
                break;
            }
            case CHANNEL_APA102_DATA: {
                channel.numElements = 4; //fix this to 4 bytes.
                PBAPA102DataChannel pbapa102DataChannel;
                pbapa102DataChannel.pixels = channel.pixels;
                pbapa102DataChannel.frequency = channel.frequency;
                pbapa102DataChannel.colorOrders = channel.colorOrders;
                Serial1.write((uint8_t *) &pbapa102DataChannel, sizeof(pbapa102DataChannel));
                crc = crc_update(crc, &pbapa102DataChannel, sizeof(pbapa102DataChannel));
                channelSwitchCallback(&channel);
                rgbFrameInitBytes[0] = rgbFrameInitBytes[1] = rgbFrameInitBytes[2] = 0;
                rgbFrameInitBytes[3] = 0x1f; //default to brightest black
                break;
            }
            case CHANNEL_APA102_CLOCK: {
                PBAPA102ClockChannel pbapa102ClockChannel;
                pbapa102ClockChannel.frequency = channel.frequency;
                Serial1.write((uint8_t *) &pbapa102ClockChannel, sizeof(pbapa102ClockChannel));
                crc = crc_update(crc, &pbapa102ClockChannel, sizeof(pbapa102ClockChannel));
                channel.pixels = 0; //make sure we don't send pixel data, even if misconfigured
                break;
            }
            default:
                channel.pixels = 0; //make sure we don't send pixel data, even if misconfigured
        }

        curPixel = channel.startIndex;
        for (int i = 0; i < channel.pixels; i++) {
            rgbFrame = rgbFrameInit;
            renderCallback(curPixel++, rgb);
            Serial1.write(rgb, channel.numElements);
            crc = crc_update(crc, rgb, channel.numElements);
            total++;
        }
        crc = crc ^0xffffffff;
        Serial1.write((uint8_t *) &crc, 4);
    }

//    Serial.print(total);
//    Serial.println(" pixels rendered");

    frameHeader.channel = 0xff;
    frameHeader.recordType = CHANNEL_DRAW_ALL;
    uint32_t crc = 0xffffffff;
    Serial1.write((uint8_t *) &frameHeader, sizeof(frameHeader));
    crc = crc_update(crc, &frameHeader, sizeof(frameHeader));
    crc = crc ^0xffffffff;
    Serial1.write((uint8_t *) &crc, 4);

    yield();
    Serial1.flush();
    timer = micros();
}

void PBDriverAdapter::configureChannels(std::unique_ptr<std::vector<PBChannel>> channels) {
    this->channels = std::move(channels);
}

std::vector<PBChannel> PBDriverAdapter::getChannelConfig() {
    return *this->channels;
}
