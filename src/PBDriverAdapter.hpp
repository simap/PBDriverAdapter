//
// Created by Ben Hencke on 11/18/18.
//

#ifndef NEOIMAGE_PBDRIVERADAPTER_HPP
#define NEOIMAGE_PBDRIVERADAPTER_HPP

#include <Arduino.h>
#include <memory>


typedef struct {
    uint8_t numElements; //0 to disable channel, usually 3 or 4
    uint8_t redi :2, greeni :2, bluei :2, whitei :2; //color orders, data on the line assumed to be RGB or RGBW
    uint16_t pixels;
} PBChannelHeader;

typedef struct {
    PBChannelHeader header;
    uint16_t startIndex;
    uint8_t channelId;
} PBChannel;


class PBDriverAdapter {
public:
    void begin(uint32_t uartFrequency = 2000000L);
    void end();
    void configureChannels(std::unique_ptr<std::vector<PBChannel>> channels);
    std::vector<PBChannel> getChannelConfig();
    void show(uint16_t numPixels, std::function<void(uint16_t index, uint8_t rgbw[])> cb);
private:
    unsigned long timer;
    std::unique_ptr<std::vector<PBChannel>> channels;
};


#endif //NEOIMAGE_PBDRIVERADAPTER_HPP
