//
// Created by Ben Hencke on 11/18/18.
//

#ifndef NEOIMAGE_PBDRIVERADAPTER_HPP
#define NEOIMAGE_PBDRIVERADAPTER_HPP

#include <Arduino.h>
#include <memory>
#include <vector>


enum ChannelType {
    CHANNEL_WS2812 = 1, CHANNEL_DRAW_ALL, CHANNEL_APA102_DATA, CHANNEL_APA102_CLOCK
};

typedef struct {
    uint8_t channelId;
    uint8_t channelType;
    uint8_t numElements; //usually 3 or 4 if configurable. zero to disable channel
    union {
        struct {
            uint8_t redi :2, greeni :2, bluei :2, whitei :2; //color orders, data on the line assumed to be RGB or RGBW
        };
        uint8_t colorOrders;
    };
    uint16_t pixels;
    uint16_t startIndex;
    uint32_t frequency;
} PBChannel;


class PBDriverAdapter {
public:
    void begin(uint32_t uartFrequency = 2000000L);
    void end();
    void configureChannels(std::unique_ptr<std::vector<PBChannel>> channels);
    std::vector<PBChannel> getChannelConfig();
    void show(uint16_t numPixels, std::function<void(uint16_t index, uint8_t rgbw[])> renderCallback, std::function<void(PBChannel *)> channelSwitchCallback);
private:
    unsigned long timer;
    std::unique_ptr<std::vector<PBChannel>> channels;
};


#endif //NEOIMAGE_PBDRIVERADAPTER_HPP
