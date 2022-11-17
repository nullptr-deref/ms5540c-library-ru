#ifndef MS5540C_H
#define MS5540C_H

#include <SPI.h>

// === ПРОЧТИТЕ ЭТОТ РАЗДЕЛ, ПРЕЖДЕ ЧЕМ ВЫПОЛНЯТЬ КАКИЕ-ЛИБО ДЕЙСТВИЯ С ДАТЧИКОМ MS5540C! ===
// Прежде всего, необходимо заметить, что датчик, для которого написана эта библиотека,
// работает на базе модифицированного протокола SPI, так что вам необходимо
// проконсультироваться с соответствующей вашему микроконтроллеру схемой выводов
// (https://docs.arduino.cc/hardware/uno-rev3).
// Для обыкновенного микроконтроллера Arduino Uno R3 необходимы выходы:
// MOSI (COPI) - D11
// MISO (CIPO) - D12
// SCK - D13
// ... также, по какой-то причине (пока что я не смог выяснить ее) необходимо ОБЯЗАТЕЛЬНО использовать
// вывод MCLK. Это обязательно, без подключения его куда-нибудь (вывод, который используется
// для этого, конфигурируется через конструктор класса ms5540c) запрос конверсии измерений
// работать НЕ БУДЕТ.

const int CONV_DUR = 35;

const byte RST_SEQ[3] = {
    0x15,
    0x55,
    0x40
};

const int16_t WORDS_ACQ[8] = {
    0x1D, 0x50, // 1st word
    0x1D, 0x60, // 2nd word
    0x1D, 0x90, // 3rd word
    0x1D, 0xA0  // 4th word
};

enum MsrType {
    Temperature,
    Pressure
};

const byte TMP_MSR[2] = {
    0x0F,
    0x20
};

const byte PRS_MSR[2] = {
    0x0F,
    0x40
};

enum UnitType {
    mbar,
    mmHg
};

class ms5540c {
public:
    ms5540c(int mclk);

    void init();
    void reset();
    float getTemperature();
    float getPressure(UnitType t);

private:
    int16_t readWord(int widx);
    int16_t readData(MsrType t);
    long getTempi();
    long getPressurei();

    int mclk_;
    long coefs[6];
};

#endif
