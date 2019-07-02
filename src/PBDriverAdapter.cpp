//
// Created by Ben Hencke on 11/18/18.
//

#include "PBDriverAdapter.hpp"

typedef struct {
    int8_t magic[4];
    uint8_t channel;
    uint8_t recordType; //set channel ws2812 opts+data, draw all
} PBFrameHeader;

enum {
    SET_CHANNEL_WS2812 = 1, DRAW_ALL
} RecordType;


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
    flush();
#ifdef ESP8266
    Serial1.begin(uartFrequency, SERIAL_8N1, SERIAL_TX_ONLY);
#endif
#ifdef ESP32
    Serial1.begin(uartFrequency, SERIAL_8N1, -1, 23);
#endif
    timer = micros();
}

void PBDriverAdapter::end() {
    Serial1.end();
}

void PBDriverAdapter::show(uint16_t numPixels, std::function<void(uint16_t index, uint8_t rgbw[])> cb) {
    int curPixel = 0;
    uint8_t rgb[4];

    while (micros() - timer < 300)
        yield();

    if (!channels)
        return;
    memset(rgb, 0, 4);

    PBFrameHeader header;
    memcpy_P(header.magic, F("UPXL"), 4);
    header.recordType = SET_CHANNEL_WS2812;

    int total = 0;
    for (auto channel : *channels) {
        if (!channel.header.numElements)
            return;

        uint32_t crc = 0xffffffff;
        header.channel = channel.channelId;
        write((uint8_t *) &header, sizeof(header));
        crc = crc_update(crc, &header, sizeof(header));

        //write the channel header struct
        write((uint8_t *) &channel.header, sizeof(channel.header));
        crc = crc_update(crc, &channel.header, sizeof(channel.header));

        curPixel = channel.startIndex;
        for (int i = 0; i < channel.header.pixels; i++) {
            memset(rgb, 0, 4);
            cb(curPixel++, rgb);
            write(rgb, channel.header.numElements);
            crc = crc_update(crc, rgb, channel.header.numElements);
            total++;
        }
        crc = crc ^0xffffffff;
        write((uint8_t *) &crc, 4);
    }

//    Serial.print(total);
//    Serial.println(" pixels rendered");

    header.channel = 0xff;
    header.recordType = DRAW_ALL;
    uint32_t crc = 0xffffffff;
    write((uint8_t *) &header, sizeof(header));
    crc = crc_update(crc, &header, sizeof(header));
    crc = crc ^0xffffffff;
    write((uint8_t *) &crc, 4);

    yield();
    flush();
    timer = micros();
}

void PBDriverAdapter::configureChannels(std::unique_ptr<std::vector<PBChannel>> channels) {
    this->channels = std::move(channels);
}

std::vector<PBChannel> PBDriverAdapter::getChannelConfig() {
    return *this->channels;
}

void PBDriverAdapter::write(const uint8_t *buffer, size_t size) {
    Serial1.write(buffer, size);
}
void PBDriverAdapter::flush() {
    Serial1.flush();
}
