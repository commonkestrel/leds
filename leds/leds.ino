#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define MIN_PATTERN_BYTES sizeof(PatternType)  + sizeof(double)

enum PatternType:byte {
    Off,
    Solid,
    Flashing,
    HeartBeat,
    ChaserUp,
    ChaserDown,
};

struct Color {
    byte red;
    byte green;
    byte blue;

    Color(byte r, byte g, byte b): red(r), green(g), blue(b) {}

    operator uint32_t() {
        return strip.Color(red, green, blue);
    }
};

union ByteDouble {
    double interval;
    byte bytes[sizeof(double)];
};

struct {
    PatternType pattern;
    double interval;
    Color *colors;
    int numColors;

    void twoWire(int bytes) {
        pattern = static_cast<PatternType>(Wire.read());

        ByteDouble iv;

        Wire.readBytes(iv.bytes, 8);
        interval = iv.interval;
        
        numColors = Wire.available() / sizeof(Color);
        
        switch (pattern) {
        case PatternType::Off:
            free(colors);
            clearWire(); // Just clear the bus and don't hog extra memory.
            return;
        case PatternType::Solid:
            numColors = 1; // Only read a single color from the bus before clearing.
            break;
        }
        
        int numBytes = numColors * sizeof(Color);

        free(colors);
        if((colors = (Color *)malloc(numBytes))) {
            for (int i; i < numColors; ++i) {
                colors[i] = Color(Wire.read(), Wire.read(), Wire.read());
            }
        } else {
            recvError = Error::OutOfMemory;
            pattern = PatternType::Off;
        }

        currentFrame = 0;
        frameQueued = true;
        clearWire();
    }
} pattern;

enum Error:byte {
    Ok,
    MissingData,
    InvalidPatternType,
    OutOfMemory,
    InvalidRegister,
};
Error recvError = Error::Ok;

unsigned long previousMillis = 0;

#define LED_PIN 6
#define LED_COUNT 300
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

bool frameQueued = false;
int currentFrame = 0;
bool increasing = true;

void setup() {
    pattern.pattern = PatternType::Off;
    pattern.interval = INFINITY;
    pattern.colors = nullptr;
    pattern.numColors = 0;

    Wire.begin(4);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);

    strip.begin();
    strip.show();
}

void loop() {
    if (millis() - previousMillis > pattern.interval) {
        frameQueued = true;
        previousMillis = millis();
    }

    if (frameQueued) {
        switch (pattern.pattern) {
        case PatternType::Off:
            strip.clear();
            strip.show();
            break;
        case PatternType::Solid:
            strip.fill(pattern.colors[currentFrame]);
            strip.show();
            break;
        case PatternType::Flashing:
            strip.fill(pattern.colors[currentFrame]);
            strip.show();
            currentFrame = ++currentFrame % pattern.numColors;
            break;
        case PatternType::HeartBeat:
            strip.fill(pattern.colors[currentFrame]);
            strip.show();

            if (increasing)
                ++currentFrame;
            else
                --currentFrame;

            if (currentFrame == 0 && currentFrame >= pattern.numColors - 1)
                increasing = !increasing;
            break;
        case PatternType::ChaserUp:
            if (currentFrame)
                strip.setPixelColor(currentFrame - 1, 0);
            else 
                strip.setPixelColor(LED_COUNT - 1, 0); 

            for (int i = 0; i < pattern.numColors; ++i) {
                strip.setPixelColor((currentFrame + i) % LED_COUNT, pattern.colors[i]);
            }
            currentFrame = ++currentFrame % pattern.numColors;
        case PatternType::ChaserDown:
            /* Too lazy to implement this yet, I'm pretty sure I can find a better method for the chasers. */
        }
    }
}

void readRegister() {
    switch (Wire.read()) {
    case 0x00:
        Wire.write(recvError);
    case 0x01:
        ByteDouble bd;
        bd.interval = pattern.interval;
        Wire.write(bd.bytes, 8);
    default:
        recvError = Error::InvalidRegister;
    }
}

int zeroLoop(int input, int cap) {
    if (input < 0) {
        return cap + (input % cap);
    } else {
        return input % cap;
    }
}

void clearWire() {
    while (Wire.available()) {
        Wire.read();
    }
}

void receiveEvent(int bytes) {
    if (bytes == 1) {
        readRegister();
    } else if (bytes >= MIN_PATTERN_BYTES) {
        pattern.twoWire(bytes);
    } else {
        recvError = Error::MissingData;
    }
}

void requestEvent() {
    Wire.write(recvError);
}
